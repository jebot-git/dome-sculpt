// SPDX-License-Identifier: GPL-3.0-or-later
// Design references: pinned Blender meshlaplacian.cc interior-grid edge
// barriers/flood regions and armature_skinning.cc deform-bone filtering.
// This is an original interior geodesic solver, not Blender's heat solver.
#include "volume-binding.hpp"
#include "dome/mesh-processing.hpp"
#include <bit>
#include <map>
#include <numeric>
#include <queue>
#include <numbers>

namespace dome::detail {
namespace {
struct Work {
  size_t used = 0, maximum;
  const std::function<bool()> &cancelled;
  void step(size_t n = 1) {
    if (n > maximum - used)
      throw std::length_error("Volume binding work budget exceeded");
    used += n;
    if ((used & 1023) < n) check();
  }
  void check() const {
    if (cancelled && cancelled())
      throw std::runtime_error("Volume binding cancelled");
  }
};
struct Memory {
  size_t planned = 0, maximum;
  void add(size_t count, size_t bytes) {
    if (count > (maximum - planned) / bytes)
      throw std::length_error("Volume binding memory budget exceeded");
    planned += count * bytes;
  }
};
using Point = std::array<double, 3>;
Point as_point(Vec3 p) { return {p.x, p.y, p.z}; }
Point operator-(Point a, Point b) {
  for (int k = 0; k < 3; ++k) a[k] -= b[k];
  return a;
}
double dot3(Point a, Point b) {
  return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];
}
Point cross3(Point a, Point b) {
  return {a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2],
          a[0]*b[1]-a[1]*b[0]};
}
double norm(Point p) { return std::sqrt(dot3(p,p)); }
Vec3 as_vec(Point p) { return {float(p[0]),float(p[1]),float(p[2])}; }
struct Components {
  std::vector<uint32_t> triangle;
  size_t count = 0;
};
Components components(const Mesh &m, Work &work) {
  const size_t nt = m.indices.size()/3;
  std::map<std::array<float,3>,uint32_t> positions;
  std::vector<uint32_t> vertex(m.positions.size()), parent(nt);
  for (size_t i = 0; i < vertex.size(); ++i) {
    work.step(); auto p=m.positions[i];
    auto [at,added]=positions.emplace(std::array{p.x,p.y,p.z},uint32_t(positions.size()));
    vertex[i]=at->second;
  }
  std::iota(parent.begin(),parent.end(),0);
  auto root=[&](uint32_t a) {
    while (a!=parent[a]) {work.step();parent[a]=parent[parent[a]];a=parent[a];}
    return a;
  };
  struct Edge { uint32_t face; bool direction; unsigned count; };
  std::map<std::array<uint32_t,2>,Edge> edges;
  for (uint32_t t=0;t<nt;++t) for (int k=0;k<3;++k) {
    work.step();auto a=vertex[m.indices[t*3+k]],b=vertex[m.indices[t*3+(k+1)%3]];
    if(a==b)throw std::invalid_argument("Volume binding requires nondegenerate faces");
    bool direction=a<b;auto key=std::array{std::min(a,b),std::max(a,b)};
    auto [at,added]=edges.emplace(key,Edge{t,direction,1});
    if(!added) {
      if(at->second.count++!=1 || at->second.direction==direction)
        throw std::invalid_argument("Volume binding requires consistently oriented closed manifold components");
      auto x=root(t),y=root(at->second.face);parent[x]=y;
    }
  }
  for(const auto &[edge,e]:edges) {
    work.step();if(e.count!=2)
      throw std::invalid_argument("Volume binding requires closed components; open edges remain unbound");
  }
  Components out;out.triangle.resize(nt);
  std::map<uint32_t,uint32_t> names;
  std::vector<uint32_t> position_component(positions.size(),UINT32_MAX);
  for(uint32_t t=0;t<nt;++t) {
    work.step();auto [at,added]=names.emplace(root(t),uint32_t(names.size()));
    out.triangle[t]=at->second;
    for(int k=0;k<3;++k) {
      auto &previous=position_component[vertex[m.indices[t*3+k]]];
      if(previous!=UINT32_MAX && previous!=at->second)
        throw std::invalid_argument("Volume binding cannot merge coincident disconnected surface vertices");
      previous=at->second;
    }
  }
  out.count=names.size();return out;
}
// Integer winding in each closed component is orientation-independent. Nested
// components use odd/even material fill, so a sealed inner cavity stays empty.
bool material_inside(const Mesh &m,const Components &parts,Point p,Work &work) {
  std::vector<double> sum(parts.count),compensation(parts.count);
  for(size_t t=0;t<parts.triangle.size();++t) {
    work.step();auto a=as_point(m.positions[m.indices[t*3]])-p;
    auto b=as_point(m.positions[m.indices[t*3+1]])-p;
    auto c=as_point(m.positions[m.indices[t*3+2]])-p;
    double la=norm(a),lb=norm(b),lc=norm(c);
    if(!la || !lb || !lc)throw std::runtime_error("Volume classification touches a surface vertex");
    double value=2*std::atan2(dot3(a,cross3(b,c)),
      la*lb*lc+dot3(a,b)*lc+dot3(b,c)*la+dot3(c,a)*lb);
    auto id=parts.triangle[t];double y=value-compensation[id],next=sum[id]+y;
    compensation[id]=(next-sum[id])-y;sum[id]=next;
  }
  bool inside=false;
  for(auto angle:sum) {
    double winding=std::abs(angle)/(4*std::numbers::pi);
    if(!std::isfinite(winding) || std::min(winding,std::abs(winding-1))>1e-5)
      throw std::runtime_error("Volume classification is ambiguous or the closed shell intersects itself");
    if(winding>.5)inside=!inside;
  }
  return inside;
}
struct Grid {
  std::array<uint32_t,3> size{};
  Point origin{};
  double h;
  std::vector<uint8_t> barriers;
  // Initially flood-region IDs, finally interior-node index or -1 outside.
  std::vector<int32_t> nodes;
  size_t count() const { return size_t(size[0])*size[1]*size[2]; }
  uint32_t index(uint32_t x,uint32_t y,uint32_t z) const {
    return x+size[0]*(y+size[1]*z);
  }
  std::array<uint32_t,3> coordinates(uint32_t i) const {
    return {i%size[0],i/size[0]%size[1],i/(size[0]*size[1])};
  }
  Point point(uint32_t i) const {
    auto q=coordinates(i);return {origin[0]+q[0]*h,origin[1]+q[1]*h,origin[2]+q[2]*h};
  }
  std::optional<uint32_t> neighbor(uint32_t i,int direction) const {
    auto q=coordinates(i);int axis=direction/2,sign=direction%2?1:-1;
    if((sign<0 && !q[axis]) || (sign>0 && q[axis]+1==size[axis]))return {};
    uint32_t stride=axis==0?1:axis==1?size[0]:size[0]*size[1];
    auto n=uint32_t(int64_t(i)+sign*int64_t(stride));
    if((barriers[i]&8) || (barriers[n]&8))return {};
    uint32_t edge=sign>0?i:n;
    if(barriers[edge]&(1<<axis))return {};
    return n;
  }
};
Grid grid_for(const Mesh &m,float h,std::array<float,3> phase,size_t maximum,Memory &memory) {
  Point lo{INFINITY,INFINITY,INFINITY},hi{-INFINITY,-INFINITY,-INFINITY};
  for(auto p:m.positions)for(int k=0;k<3;++k){auto v=as_point(p)[k];lo[k]=std::min(lo[k],v);hi[k]=std::max(hi[k],v);}
  Grid g;g.h=h;size_t total=1;
  for(int k=0;k<3;++k) {
    g.origin[k]=lo[k]-(1.+phase[k])*h;
    double n=std::ceil((hi[k]-lo[k])/h)+4;
    if(!std::isfinite(n) || n<4 || n>maximum || total>maximum/size_t(n))
      throw std::length_error("Volume binding grid budget exceeded; choose an explicit coarser cell size");
    g.size[k]=uint32_t(n);total*=size_t(n);
  }
  // Include maximum flood stack and component classification storage before
  // allocating. Later stages remain conservatively charged for this peak.
  memory.add(total,11);g.barriers.resize(total);g.nodes.assign(total,-1);return g;
}
void rasterize(const Mesh &m,Grid &g,Work &work) {
  for(size_t t=0;t<m.indices.size();t+=3) {
    work.step();std::array<std::array<long double,3>,3> q;
    for(int k=0;k<3;++k)for(int a=0;a<3;++a)
      q[k][a]=(as_point(m.positions[m.indices[t+k]])[a]-g.origin[a])/g.h;
    for(int axis=0;axis<3;++axis) {
      int u=(axis+1)%3,v=(axis+2)%3;
      long double ax=q[1][u]-q[0][u],ay=q[1][v]-q[0][v];
      long double bx=q[2][u]-q[0][u],by=q[2][v]-q[0][v],det=ax*by-ay*bx;
      if(det==0)continue;
      auto lower=[&](int a){return std::max(0,int(std::ceil(std::min({q[0][a],q[1][a],q[2][a]})))-1);};
      auto upper=[&](int a){return std::min(int(g.size[a])-1,int(std::floor(std::max({q[0][a],q[1][a],q[2][a]})))+1);};
      const auto eps=256*std::numeric_limits<long double>::epsilon();
      for(int j=lower(v);j<=upper(v);++j)for(int i=lower(u);i<=upper(u);++i) {
        work.step();long double x=i-q[0][u],y=j-q[0][v];
        long double a=(x*by-y*bx)/det,b=(ax*y-ay*x)/det;
        long double e=eps*(1+std::abs(a)+std::abs(b));
        if(a<-e || b<-e || a+b>1+e)continue;
        long double at=q[0][axis]+a*(q[1][axis]-q[0][axis])+b*(q[2][axis]-q[0][axis]);
        auto floor=int(std::floor(at));if(floor<0 || floor+1>=int(g.size[axis]))
          throw std::logic_error("Volume barrier escaped its padded grid");
        std::array<uint32_t,3> xyz{};xyz[u]=i;xyz[v]=j;xyz[axis]=floor;
        g.barriers[g.index(xyz[0],xyz[1],xyz[2])]|=1<<axis;
        // A grid node on a surface cannot be a legal path through that surface.
        auto rounded=std::round(at);
        if(std::abs(at-rounded)<1e-7L) {
          xyz[axis]=uint32_t(rounded);g.barriers[g.index(xyz[0],xyz[1],xyz[2])]|=8;
        }
      }
    }
  }
}
struct Interior {
  std::vector<uint32_t> grid,component;
  size_t components=0;
};
Interior classify(const Mesh &m,const Components &parts,Grid &g,Work &work,Memory &memory) {
  std::vector<uint32_t> stack;stack.reserve(g.count());std::vector<uint8_t> flags;
  for(uint32_t i=0;i<g.count();++i) {
    work.step();if((g.barriers[i]&8) || g.nodes[i]>=0)continue;
    int32_t region=int32_t(flags.size());flags.push_back(material_inside(m,parts,g.point(i),work));
    g.nodes[i]=region;stack.push_back(i);
    while(!stack.empty()) {
      auto at=stack.back();stack.pop_back();
      for(int d=0;d<6;++d) {work.step();auto n=g.neighbor(at,d);if(n&&g.nodes[*n]<0){g.nodes[*n]=region;stack.push_back(*n);}}
    }
  }
  memory.add(flags.size(),sizeof(uint32_t));
  size_t count=0;for(auto r:g.nodes){work.step();if(r>=0 && flags[r])++count;}
  memory.add(count,2*sizeof(uint32_t));
  Interior out;out.grid.reserve(count);out.component.reserve(count);
  std::vector<uint32_t> region(flags.size(),UINT32_MAX);
  for(size_t i=0;i<flags.size();++i)if(flags[i])region[i]=uint32_t(out.components++);
  for(uint32_t i=0;i<g.count();++i) {
    work.step();auto r=g.nodes[i];
    if(r>=0 && flags[r]) {g.nodes[i]=int32_t(out.grid.size());out.grid.push_back(i);out.component.push_back(region[r]);}
    else g.nodes[i]=-1;
  }
  if(out.grid.empty())throw std::runtime_error("No resolved interior cells; volume binding needs a finer explicit cell size");
  return out;
}
Point closest_segment(Point p,Point a,Point b) {
  auto v=b-a,d=p-a;double squared=dot3(v,v);
  double t=squared?std::clamp(dot3(d,v)/squared,0.,1.):0;
  for(int k=0;k<3;++k)a[k]+=t*v[k];
  return a;
}
struct Label { double distance=INFINITY;uint32_t bone=UINT32_MAX; };
bool better(Label a,Label b) {return a.distance==b.distance?a.bone<b.bone:a.distance<b.distance;}
void insert_label(std::span<Label> row,Label value) {
  for(auto &old:row)if(old.bone==value.bone){if(better(value,old))old=value;std::sort(row.begin(),row.end(),better);return;}
  if(better(value,row.back())) {row.back()=value;std::sort(row.begin(),row.end(),better);}
}
struct Slot : Label { uint32_t heap=UINT32_MAX;bool settled=false; };
class Pending {
  std::span<Slot> slots;
  size_t k;
  Work &work;
  std::vector<uint32_t> heap;
  bool less(uint32_t a,uint32_t b) {
    work.step();if(slots[a].distance!=slots[b].distance)return slots[a].distance<slots[b].distance;
    if(slots[a].bone!=slots[b].bone)return slots[a].bone<slots[b].bone;
    return a/k<b/k;
  }
  void swap(size_t a,size_t b){std::swap(heap[a],heap[b]);slots[heap[a]].heap=uint32_t(a);slots[heap[b]].heap=uint32_t(b);}
  void up(size_t at){while(at){size_t parent=(at-1)/2;if(!less(heap[at],heap[parent]))break;swap(at,parent);at=parent;}}
  void down(size_t at){while(at*2+1<heap.size()){size_t child=at*2+1;if(child+1<heap.size()&&less(heap[child+1],heap[child]))++child;if(!less(heap[child],heap[at]))break;swap(at,child);at=child;}}
public:
  Pending(std::span<Slot> s,size_t count,Work&w):slots(s),k(count),work(w){heap.reserve(slots.size());}
  bool empty()const{return heap.empty();}
  void relax(uint32_t node,Label value) {
    work.step();uint32_t at=uint32_t(node*k),worst=at;
    for(size_t i=0;i<k;++i){work.step();auto id=uint32_t(node*k+i);if(slots[id].bone==value.bone){at=id;goto existing;}if(better(slots[worst],slots[id]))worst=id;}
    at=worst;
existing:
    if(!better(value,slots[at]))return;
    if(slots[at].settled)throw std::logic_error("Volume shortest-path settled-label invariant failed");
    slots[at].distance=value.distance;slots[at].bone=value.bone;
    if(slots[at].heap==UINT32_MAX){slots[at].heap=uint32_t(heap.size());heap.push_back(at);}
    up(slots[at].heap);
  }
  uint32_t pop() {
    work.step();auto result=heap.front(),last=heap.back();heap.pop_back();slots[result].heap=UINT32_MAX;
    if(!heap.empty()){heap[0]=last;slots[last].heap=0;down(0);}
    slots[result].settled=true;return result;
  }
};
constexpr auto directions=[] {
  std::array<std::array<int,3>,26> result{};size_t at=0;
  for(int z=-1;z<=1;++z)for(int y=-1;y<=1;++y)for(int x=-1;x<=1;++x)
    if(x||y||z)result[at++]={x,y,z};
  return result;
}();
std::optional<uint32_t> offset(const Grid &g,uint32_t at,std::array<int,3>d) {
  auto p=g.coordinates(at);std::array<uint32_t,3> q;
  for(int a=0;a<3;++a){int64_t n=int64_t(p[a])+d[a];if(n<0||n>=g.size[a])return {};q[a]=uint32_t(n);}
  return g.index(q[0],q[1],q[2]);
}
void validate_resolution(const Mesh &m,const Components &parts,const Grid &g,
                         const SurfaceQuery &query,const SurfaceQueryOptions &qo,
                         Work &work) {
  std::vector<uint32_t> first(parts.count,UINT32_MAX);
  std::vector<Point> reference(parts.count);
  std::vector<double> volume(parts.count),compensation(parts.count),absolute(parts.count);
  double maximum=0;
  for(size_t t=0;t<parts.triangle.size();++t) {
    work.step();auto id=parts.triangle[t];
    if(first[id]==UINT32_MAX){first[id]=uint32_t(t);reference[id]=as_point(m.positions[m.indices[t*3]]);}
    auto a=as_point(m.positions[m.indices[t*3]])-reference[id];
    auto b=as_point(m.positions[m.indices[t*3+1]])-reference[id];
    auto c=as_point(m.positions[m.indices[t*3+2]])-reference[id];
    double v=dot3(a,cross3(b,c)),y=v-compensation[id],next=volume[id]+y;
    compensation[id]=(next-volume[id])-y;volume[id]=next;absolute[id]+=std::abs(v);
    for(auto p:{a,b,c})maximum=std::max(maximum,norm(p));
  }
  std::vector<int> sign(parts.count);
  auto triangle=[&](size_t t){return std::array{as_point(m.positions[m.indices[t*3]]),
    as_point(m.positions[m.indices[t*3+1]]),as_point(m.positions[m.indices[t*3+2]])};};
  auto face_center=[](const std::array<Point,3>&p){Point q{};for(int a=0;a<3;++a)q[a]=(p[0][a]+p[1][a]+p[2][a])/3;return q;};
  auto epsilon=[&](Point p){return std::max(g.h*1e-5,32*double(std::numeric_limits<float>::epsilon())*
    std::max({1.,std::abs(p[0]),std::abs(p[1]),std::abs(p[2])}));};
  for(size_t id=0;id<parts.count;++id) {
    work.step();if(!std::isfinite(volume[id]) || std::abs(volume[id])<=absolute[id]*1e-12)
      throw std::invalid_argument("Volume binding requires a nonzero consistently oriented closed solid");
    auto p=triangle(first[id]);auto normal=cross3(p[1]-p[0],p[2]-p[0]);double length=norm(normal);
    if(!length)throw std::invalid_argument("Volume binding contains a degenerate face");
    auto center=face_center(p);double eps=epsilon(center);int outward=volume[id]>0?1:-1;
    if(eps>g.h*.1)throw std::runtime_error("Volume cell size is below the geometry's coordinate precision");
    auto outside=center;for(int a=0;a<3;++a)outside[a]+=outward*normal[a]/length*eps;
    // Outside the component but inside other material identifies a cavity
    // boundary. Its material side is opposite an ordinary outer shell.
    sign[id]=outward*(material_inside(m,parts,outside,work)?-1:1);
  }
  for(size_t t=0;t<parts.triangle.size();++t) {
    work.step();auto p=triangle(t);auto normal=cross3(p[1]-p[0],p[2]-p[0]);double length=norm(normal);
    if(!length)throw std::invalid_argument("Volume binding contains a degenerate face");
    auto center=face_center(p),start=center;double eps=epsilon(center);
    Point inward{};for(int a=0;a<3;++a){inward[a]=-sign[parts.triangle[t]]*normal[a]/length;start[a]+=eps*inward[a];}
    auto hit=query.raycast(as_vec(start),as_vec(inward),float(maximum*2+g.h),qo);
    if(!hit)throw std::runtime_error("Volume binding could not prove a bounded material thickness");
    if(double(hit->distance)+eps<2*g.h)
      throw std::runtime_error("Volume cell size does not resolve a thin region; choose a finer explicit cell size");
  }
}
} // namespace

SkinWeights volume_bind_native(const Mesh &input,const Skeleton &s,
                              const VolumeBindingOptions &o,
                              const std::function<bool()> &cancelled,
                              VolumeBindingReport *report) {
  Work work{0,o.binding.maximum_work,cancelled};work.check();
  if(!o.binding.maximum_work || o.binding.maximum_work>500'000'000 ||
     !std::isfinite(o.cell_size) || o.cell_size<1e-5f || o.cell_size>10 ||
     std::any_of(o.grid_phase.begin(),o.grid_phase.end(),[](float p){return !std::isfinite(p)||p<0||p>1;}) ||
     !o.maximum_grid_points || o.maximum_grid_points>16'000'000 ||
     o.maximum_bytes<4096 || o.maximum_bytes>512u*1024u*1024u ||
     !std::isfinite(o.binding.maximum_seed_distance) || o.binding.maximum_seed_distance<=0 ||
     o.binding.maximum_seed_distance>10000 || !std::isfinite(o.binding.seed_margin) ||
     o.binding.seed_margin<0 || o.binding.seed_margin>o.binding.maximum_seed_distance ||
     !std::isfinite(o.binding.falloff) || o.binding.falloff<1 || o.binding.falloff>8)
    throw std::invalid_argument("Invalid volume binding resolution, distance or resource bounds");
  if(input.positions.size()>2'000'000 || input.indices.size()>6'000'000)
    throw std::length_error("Volume binding mesh budget exceeded");
  if(input.positions.empty() || input.indices.empty())
    throw std::invalid_argument("Volume binding requires a nonempty closed mesh");
  work.step(input.positions.size()+input.indices.size());
  input.validate();s.validate();normalize_weights({{0,1}},s.bones.size(),{},o.binding.limits);
  Memory memory{0,o.maximum_bytes};const size_t nt=input.indices.size()/3;
  size_t mesh_bytes=input.positions.size()*sizeof(Vec3)+input.normals.size()*sizeof(Vec3)+
    input.uv.size()*sizeof(std::array<float,2>)+input.indices.size()*sizeof(uint32_t)+
    input.uv_pins.size()+input.uv_seams.size();
  memory.add(2,mesh_bytes);memory.add(nt,384);memory.add(input.positions.size(),96);
  // Conservative red-black-tree comparison bounds and BVH construction work
  // are charged before creating their detached containers/copies.
  work.step(input.positions.size()*(4*std::bit_width(input.positions.size()+1)+1));
  work.step(input.indices.size()*(4*std::bit_width(input.indices.size()+1)+1));
  Mesh m=input;Point center{};
  for(int a=0;a<3;++a){double lo=INFINITY,hi=-INFINITY;for(auto p:m.positions){auto v=as_point(p)[a];lo=std::min(lo,v);hi=std::max(hi,v);}center[a]=(lo+hi)*.5;}
  for(auto &p:m.positions){work.step();p=as_vec(as_point(p)-center);}
  auto parts=components(m,work);auto g=grid_for(m,o.cell_size,o.grid_phase,o.maximum_grid_points,memory);
  rasterize(m,g,work);auto inside=classify(m,parts,g,work,memory);
  const size_t n=inside.grid.size(),k=o.binding.limits.max_influences;
  memory.add(n,k*(sizeof(Slot)+sizeof(uint32_t))+sizeof(uint32_t));
  std::vector<Slot> labels(n*k);
  memory.add(inside.components,sizeof(uint8_t)+sizeof(double));
  memory.add(inside.components*s.bones.size(),sizeof(uint32_t));
  memory.add(s.bones.size(),2*sizeof(Point));
  std::vector<uint8_t> seeded(inside.components);
  std::vector<double> component_scale(inside.components,1e-5);
  std::vector<uint32_t> bone_seed(inside.components*s.bones.size(),UINT32_MAX);
  std::vector<std::array<Point,2>> bone_segments(s.bones.size());
  work.step(nt*(16*std::bit_width(nt+1)));
  SurfaceQuery query(m,[&]{work.step();work.check();return false;});
  SurfaceQueryOptions qo;qo.cancelled=[&]{work.step();work.check();return false;};
  qo.triangle_filter=[&](uint32_t){work.step();return true;};
  validate_resolution(m,parts,g,query,qo,work);
  auto visible=[&](Point a,Point b) {
    auto delta=b-a;double distance=norm(delta);
    if(distance<g.h*1e-6)return true;
    double trim=std::min(distance*.001,g.h*1e-5);
    for(int c=0;c<3;++c)a[c]+=delta[c]*(trim/distance);
    return !query.raycast(as_vec(a),as_vec(delta),float(distance-2*trim),qo);
  };
  std::vector<uint32_t> links(n);
  for(uint32_t v=0;v<n;++v)for(size_t d=0;d<directions.size();++d) {
    work.step();auto next=offset(g,inside.grid[v],directions[d]);if(!next||*next<=inside.grid[v])continue;
    int32_t id=g.nodes[*next];if(id<0||inside.component[id]!=inside.component[v])continue;
    auto delta=directions[d];int axes=std::abs(delta[0])+std::abs(delta[1])+std::abs(delta[2]);
    if(axes==1) {
      int axis=delta[0]?0:delta[1]?1:2;auto adjacent=g.neighbor(inside.grid[v],axis*2+(delta[axis]>0));
      if(!adjacent||*adjacent!=*next)continue;
    }else {
      auto a=g.point(inside.grid[v]),b=g.point(*next),direction=b-a;
      // A diagonal is not inferred from its endpoints or from axis edges.
      // Test the actual full interior segment, including cavity/thin walls.
      if(query.raycast(as_vec(a),as_vec(direction),float(norm(direction)),qo))continue;
    }
    links[v]|=uint32_t(1)<<d;links[id]|=uint32_t(1)<<(25-d);
  }
  Pending queue(labels,k,work);
  auto worlds=s.world();bool deform=false;
  double radius=std::min(double(o.binding.maximum_seed_distance),g.h+o.binding.seed_margin);
  for(uint32_t bone=0;bone<s.bones.size();++bone)if(s.bones[bone].deform) {
    deform=true;const auto &w=worlds[bone];Point a{w[12]-center[0],w[13]-center[1],w[14]-center[2]},b=a;
    for(int c=0;c<3;++c)b[c]+=double(w[4+c])*s.bones[bone].length;
    bone_segments[bone]={a,b};
    for(uint32_t v=0;v<n;++v) {
      work.step();auto p=g.point(inside.grid[v]),q=closest_segment(p,a,b);double distance=norm(p-q);
      if(distance<=radius && visible(p,q)) {
        queue.relax(v,{distance,bone});
        seeded[inside.component[v]]=1;
        component_scale[inside.component[v]]=std::max(component_scale[inside.component[v]],norm(b-a)*.02);
        auto &anchor=bone_seed[inside.component[v]*s.bones.size()+bone];
        if(anchor==UINT32_MAX)anchor=v;
      }
    }
  }
  if(!deform)throw std::runtime_error("Volume binding requires a deforming bone");
  for(size_t c=0;c<seeded.size();++c)if(!seeded[c])
    throw std::runtime_error("No internal deform-bone seed for volume component "+std::to_string(c));
  while(!queue.empty()) {
    work.step();auto at=queue.pop();auto item=labels[at];uint32_t node=uint32_t(at/k);
    for(size_t d=0;d<directions.size();++d)if(links[node]&(uint32_t(1)<<d)) {
      work.step();auto next=offset(g,inside.grid[node],directions[d]);auto id=g.nodes[*next];
      auto direction=directions[d];double cost=g.h*std::sqrt(double(direction[0]*direction[0]+direction[1]*direction[1]+direction[2]*direction[2]));
      queue.relax(uint32_t(id),{item.distance+cost,item.bone});
    }
  }
  // A pruned source is dominated by K distinct sources at this node. Every
  // continuation adds the same nonnegative edge cost to all K, so it cannot
  // enter the first K at any later node by following that continuation.
  memory.add(m.positions.size(),sizeof(VertexWeights)+k*sizeof(Influence));
  memory.add(k,sizeof(Label));
  SkinWeights out;out.vertices.reserve(m.positions.size());
  for(auto vertex:m.positions) {
    work.step();Point p=as_point(vertex);std::vector<Label> row(k);
    std::array<int,3> q;for(int a=0;a<3;++a)q[a]=int(std::floor((p[a]-g.origin[a])/g.h));
    size_t support=0;uint32_t volume_component=UINT32_MAX;
    for(int z=q[2]-1;z<=q[2]+2;++z)for(int y=q[1]-1;y<=q[1]+2;++y)for(int x=q[0]-1;x<=q[0]+2;++x) {
      work.step();if(x<0||y<0||z<0||x>=int(g.size[0])||y>=int(g.size[1])||z>=int(g.size[2]))continue;
      auto gi=g.index(x,y,z);auto id=g.nodes[gi];if(id<0)continue;
      auto at=g.point(gi);double distance=norm(at-p);if(distance>g.h*2 || !visible(p,at))continue;
      if(volume_component!=UINT32_MAX && volume_component!=inside.component[id])
        throw std::runtime_error("A surface vertex sees unresolved disconnected volume regions; choose a finer explicit cell size");
      volume_component=inside.component[id];
      ++support;for(size_t c=0;c<k;++c){auto l=labels[size_t(id)*k+c];if(l.bone!=UINT32_MAX)insert_label(row,{l.distance+distance,l.bone});}
    }
    if(!support || row.front().bone==UINT32_MAX)
      throw std::runtime_error("A surface vertex has no resolved same-volume support; choose a finer explicit cell size");
    // An actual visible bone-to-vertex segment attains the Euclidean lower
    // bound exactly, avoiding grid anisotropy near a source. Its bone endpoint
    // must first be certified from an interior anchor of this same volume;
    // visibility starting at a surface vertex alone cannot prove that an
    // external bone is inside. All sources get this terminal shortcut, so a
    // source pruned from the grid's K labels is not lost from the final rank.
    for(uint32_t bone=0;bone<s.bones.size();++bone) {
      work.step();auto anchor=bone_seed[volume_component*s.bones.size()+bone];
      if(anchor==UINT32_MAX)continue;
      auto endpoint=closest_segment(p,bone_segments[bone][0],bone_segments[bone][1]);
      if(visible(g.point(inside.grid[anchor]),endpoint) && visible(p,endpoint))
        insert_label(row,{norm(p-endpoint),bone});
    }
    // Physical falloff must not change merely because the numerical grid is
    // refined. Use the legacy2%-bone-length policy, scoped to actual sources
    // inside this volume, rather than using cell_size as a regularizer.
    VertexWeights weights;weights.reserve(k);double nearest=row.front().distance,scale=component_scale[volume_component];
    for(auto l:row)if(l.bone!=UINT32_MAX)
      weights.push_back({l.bone,float(std::pow((nearest+scale)/(l.distance+scale),o.binding.falloff))});
    out.vertices.push_back(normalize_weights(std::move(weights),s.bones.size(),{},o.binding.limits));
  }
  out.validate(m.positions.size(),s.bones.size());work.check();
  if(report)*report={g.count(),n,inside.components,work.used,memory.planned};
  return out;
}
} // namespace dome::detail
