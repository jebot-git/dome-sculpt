// SPDX-License-Identifier: GPL-3.0-or-later
// Design references: pinned Blender meshlaplacian.cc interior-grid edge
// barriers/flood regions and armature_skinning.cc deform-bone filtering.
// This is an original interior geodesic solver, not Blender's heat solver.
#include "volume-binding.hpp"
#include "volume-exact.hpp"
#include "volume-topology.hpp"
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
  size_t peak = 0;
  void add(size_t count, size_t bytes) {
    if (count > (maximum - planned) / bytes)
      throw std::length_error("Volume binding memory budget exceeded");
    planned += count * bytes;peak=std::max(peak,planned);
  }
  void release(size_t count,size_t bytes){planned-=count*bytes;}
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
  std::vector<uint32_t> vertex;
  std::vector<Vec3> points;
  std::vector<std::array<uint32_t,2>> edges;
  std::vector<std::array<uint32_t,3>> adjacent;
  size_t count = 0;
};
Components components(const Mesh &m, Work &work) {
  const size_t nt = m.indices.size()/3;
  std::map<std::array<float,3>,uint32_t> positions;
  std::vector<uint32_t> vertex(m.positions.size()), parent(nt);
  std::vector<Vec3> points;points.reserve(m.positions.size());
  for (size_t i = 0; i < vertex.size(); ++i) {
    work.step(); auto p=m.positions[i];
    auto [at,added]=positions.emplace(std::array{p.x,p.y,p.z},uint32_t(positions.size()));
    if(added)points.push_back(p);
    vertex[i]=at->second;
  }
  std::iota(parent.begin(),parent.end(),0);
  auto root=[&](uint32_t a) {
    while (a!=parent[a]) {work.step();parent[a]=parent[parent[a]];a=parent[a];}
    return a;
  };
  Components out;out.adjacent.assign(nt,{UINT32_MAX,UINT32_MAX,UINT32_MAX});
  struct Edge { uint32_t face,local; bool direction; unsigned count; };
  std::map<std::array<uint32_t,2>,Edge> edges;
  for (uint32_t t=0;t<nt;++t) for (int k=0;k<3;++k) {
    work.step();auto a=vertex[m.indices[t*3+k]],b=vertex[m.indices[t*3+(k+1)%3]];
    if(a==b)throw std::invalid_argument("Volume binding requires nondegenerate faces");
    bool direction=a<b;auto key=std::array{std::min(a,b),std::max(a,b)};
    auto [at,added]=edges.emplace(key,Edge{t,uint32_t(k),direction,1});
    if(!added) {
      if(at->second.count++!=1 || at->second.direction==direction)
        throw std::invalid_argument("Volume binding requires consistently oriented closed manifold components");
      auto x=root(t),y=root(at->second.face);parent[x]=y;
      out.adjacent[t][k]=at->second.face;out.adjacent[at->second.face][at->second.local]=t;
    }
  }
  out.edges.reserve(edges.size());
  for(const auto &[edge,e]:edges) {
    work.step();if(e.count!=2)
      throw std::invalid_argument("Volume binding requires closed components; open edges remain unbound");
    out.edges.push_back(edge);
  }
  // Closed paired edges alone do not prove a manifold vertex: two otherwise
  // connected sheets can meet at one welded point. Its incident triangle link
  // must be one cycle, not two disjoint fans.
  std::vector<uint32_t> degree(points.size()),first(points.size(),UINT32_MAX);
  for(uint32_t t=0;t<nt;++t)for(int k=0;k<3;++k){
    work.step();auto v=vertex[m.indices[t*3+k]];++degree[v];first[v]=t;
  }
  for(uint32_t v=0;v<points.size();++v){
    work.step();if(!degree[v])continue;
    uint32_t current=first[v],previous=UINT32_MAX,visited=0;
    do {
      work.step();uint32_t next=UINT32_MAX;
      for(int edge=0;edge<3;++edge){
        work.step();auto a=vertex[m.indices[current*3+edge]],b=vertex[m.indices[current*3+(edge+1)%3]];
        if(a!=v&&b!=v)continue;
        auto adjacent=out.adjacent[current][edge];
        if(adjacent!=previous){next=adjacent;break;}
      }
      if(next==UINT32_MAX||++visited>degree[v])
        throw std::invalid_argument("Volume binding requires one manifold triangle fan at every welded vertex");
      previous=current;current=next;
    }while(current!=first[v]);
    if(visited!=degree[v])
      throw std::invalid_argument("Volume binding cannot join disjoint triangle fans at a coincident vertex");
  }
  out.triangle.resize(nt);
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
  out.count=names.size();out.vertex=std::move(vertex);out.points=std::move(points);return out;
}
// Integer winding in each closed component is orientation-independent. Nested
// components use odd/even material fill, so a sealed inner cavity stays empty.
std::optional<bool> material_classification(const Mesh &m,const Components &parts,Point p,Work &work) {
  std::vector<double> sum(parts.count),compensation(parts.count);
  for(size_t t=0;t<parts.triangle.size();++t) {
    work.step();auto a=as_point(m.positions[m.indices[t*3]])-p;
    auto b=as_point(m.positions[m.indices[t*3+1]])-p;
    auto c=as_point(m.positions[m.indices[t*3+2]])-p;
    double la=norm(a),lb=norm(b),lc=norm(c);
    if(!la || !lb || !lc)return {};
    double value=2*std::atan2(dot3(a,cross3(b,c)),
      la*lb*lc+dot3(a,b)*lc+dot3(b,c)*la+dot3(c,a)*lb);
    auto id=parts.triangle[t];double y=value-compensation[id],next=sum[id]+y;
    compensation[id]=(next-sum[id])-y;sum[id]=next;
  }
  bool inside=false;
  for(auto angle:sum) {
    double winding=std::abs(angle)/(4*std::numbers::pi);
    if(!std::isfinite(winding) || std::min(winding,std::abs(winding-1))>1e-5)
      return {};
    if(winding>.5)inside=!inside;
  }
  return inside;
}
bool material_inside(const Mesh &m,const Components &parts,Point p,Work &work) {
  auto value=material_classification(m,parts,p,work);
  if(!value)throw std::runtime_error("Volume classification is ambiguous or the closed shell intersects itself");
  return *value;
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
// A multi-axis Eikonal update spans a simplex, not just its edges. Mark every
// cube touched by the actual surface so no mixed stencil can cut a subcell
// cavity or thin wall whose boundary happens to miss the grid edges.
void mark_cut_cubes(const Mesh &m,Grid &g,Work &work) {
  struct Box {std::array<uint32_t,3> lo,hi;};
  auto intersects=[&](const std::array<Point,3>&p,const Box &box) {
    Point center{},half{};std::array<Point,3> q;
    for(int a=0;a<3;++a){center[a]=g.origin[a]+(double(box.lo[a])+box.hi[a]+1)*g.h*.5;half[a]=(double(box.hi[a])-box.lo[a]+1)*g.h*.5;}
    for(int i=0;i<3;++i)q[i]=p[i]-center;
    auto separated=[&](Point axis){work.step();double r=0;for(int a=0;a<3;++a)r+=std::abs(axis[a])*half[a];double lo=INFINITY,hi=-INFINITY;for(auto v:q){auto projection=dot3(v,axis);lo=std::min(lo,projection);hi=std::max(hi,projection);}double epsilon=128*std::numeric_limits<double>::epsilon()*(r+std::max(std::abs(lo),std::abs(hi)));return lo>r+epsilon||hi<-r-epsilon;};
    const std::array<Point,3> axes{Point{1,0,0},Point{0,1,0},Point{0,0,1}};
    for(auto axis:axes)if(separated(axis))return false;
    if(separated(cross3(q[1]-q[0],q[2]-q[0])))return false;
    for(int i=0;i<3;++i)for(auto axis:axes)if(separated(cross3(q[(i+1)%3]-q[i],axis)))return false;
    return true;
  };
  for(size_t t=0;t<m.indices.size();t+=3) {
    work.step();std::array<Point,3> p;for(int k=0;k<3;++k)p[k]=as_point(m.positions[m.indices[t+k]]);
    Box root;
    for(int a=0;a<3;++a){double lo=INFINITY,hi=-INFINITY;for(auto v:p){lo=std::min(lo,v[a]);hi=std::max(hi,v[a]);}root.lo[a]=uint32_t(std::max(0,int(std::floor((lo-g.origin[a])/g.h))-1));root.hi[a]=uint32_t(std::min(int(g.size[a])-2,int(std::floor((hi-g.origin[a])/g.h))+1));}
    std::array<Box,64> stack;size_t count=1;stack[0]=root;
    while(count){work.step();auto box=stack[--count];if(!intersects(p,box))continue;int axis=0;for(int a=1;a<3;++a)if(box.hi[a]-box.lo[a]>box.hi[axis]-box.lo[axis])axis=a;
      if(box.lo[axis]==box.hi[axis]){g.barriers[g.index(box.lo[0],box.lo[1],box.lo[2])]|=16;continue;}
      if(count+2>stack.size())throw std::length_error("Volume surface raster depth exceeded");
      auto other=box;auto middle=box.lo[axis]+(box.hi[axis]-box.lo[axis])/2;box.hi[axis]=middle;other.lo[axis]=middle+1;stack[count++]=box;stack[count++]=other;
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
double eikonal_value(uint32_t node,const Grid &g,const Interior &inside,
                     std::span<const Slot> field,Work &work) {
  struct Upwind {double distance;int axis,sign;};
  std::array<Upwind,3> values;size_t count=0;
  for(int axis=0;axis<3;++axis){Upwind best{INFINITY,axis,0};for(int sign:{-1,1}){
    work.step();auto neighbor=g.neighbor(inside.grid[node],axis*2+(sign>0));if(!neighbor)continue;
    auto id=g.nodes[*neighbor];if(id>=0&&field[id].settled&&field[id].distance<best.distance)best={field[id].distance,axis,sign};
  }if(std::isfinite(best.distance))values[count++]=best;}
  if(!count)return INFINITY;
  if(count>1){work.step();if(values[1].distance<values[0].distance)std::swap(values[0],values[1]);}
  if(count>2){work.step();if(values[2].distance<values[1].distance)std::swap(values[1],values[2]);work.step();if(values[1].distance<values[0].distance)std::swap(values[0],values[1]);}
  auto clear=[&](size_t used){auto p=g.coordinates(inside.grid[node]);std::array<int,3> sign{};for(size_t i=0;i<used;++i)sign[values[i].axis]=values[i].sign;
    for(unsigned variant=0;variant<8;++variant){work.step();std::array<uint32_t,3> q;bool valid=true;for(int a=0;a<3;++a){int64_t base=p[a];if(sign[a]<0||(!sign[a]&&(variant&(1u<<a))))--base;if(base<0||base+1>=g.size[a]){valid=false;break;}q[a]=uint32_t(base);}if(valid&&!(g.barriers[g.index(q[0],q[1],q[2])]&16))return true;}
    return false;
  };
  double first=values[0].distance,best=first+g.h,sum=0,squared=0;
  for(size_t used=1;used<=count;++used){work.step();double delta=values[used-1].distance-first;sum+=delta;squared+=delta*delta;if(used==1||values[used-1].distance>best||!clear(used))continue;double discriminant=sum*sum-used*(squared-g.h*g.h);if(discriminant<0)continue;double value=first+(sum+std::sqrt(discriminant))/used;if(value>=values[used-1].distance)best=std::min(best,value);}
  return best;
}
struct Arc { uint32_t to;double cost; };
struct BoundaryGraph {
  std::vector<uint32_t> component;
  std::vector<size_t> starts;
  std::vector<Arc> arcs;
};
template<class Visible>
BoundaryGraph boundary_graph(const Components &parts,const Grid &g,const Interior &inside,
                             Visible &&visible,Work &work,Memory &memory) {
  struct Connection {uint32_t from,to;double cost;};
  size_t n=inside.grid.size(),v=parts.points.size(),total=n+v;
  memory.add(v,sizeof(uint32_t));memory.add(total+1,2*sizeof(size_t));
  BoundaryGraph graph;graph.component.assign(v,UINT32_MAX);
  std::vector<Connection> connections;const size_t maximum=64*v+parts.edges.size();
  auto add=[&](Connection c){work.step();
    if(connections.size()==connections.capacity()){
      size_t before=connections.capacity(),next=std::min(maximum,std::max(size_t(256),before*2));
      if(next<=before)throw std::length_error("Volume boundary graph edge budget exceeded");
      memory.add(next,sizeof(Connection));connections.reserve(next);memory.release(before,sizeof(Connection));
    }
    connections.push_back(c);
  };
  for(uint32_t at=0;at<v;++at) {
    work.step();auto p=as_point(parts.points[at]);std::array<int,3> q;
    for(int a=0;a<3;++a)q[a]=int(std::floor((p[a]-g.origin[a])/g.h));
    size_t support=0;
    for(int z=q[2]-1;z<=q[2]+2;++z)for(int y=q[1]-1;y<=q[1]+2;++y)for(int x=q[0]-1;x<=q[0]+2;++x){
      work.step();if(x<0||y<0||z<0||x>=int(g.size[0])||y>=int(g.size[1])||z>=int(g.size[2]))continue;
      auto gi=g.index(x,y,z);auto id=g.nodes[gi];if(id<0)continue;
      auto point=g.point(gi);double distance=norm(point-p);if(distance>g.h*2||!visible(p,point))continue;
      if(graph.component[at]!=UINT32_MAX&&graph.component[at]!=inside.component[id])
        throw std::runtime_error("A surface vertex sees unresolved disconnected volume regions; choose a finer explicit cell size");
      graph.component[at]=inside.component[id];++support;
      add({uint32_t(n+at),uint32_t(id),distance});
    }
    if(!support)throw std::runtime_error("A surface vertex has no resolved same-volume support; choose a finer explicit cell size");
  }
  for(auto edge:parts.edges) {
    work.step();if(graph.component[edge[0]]!=graph.component[edge[1]])
      throw std::runtime_error("A surface edge crosses unresolved volume regions; choose a finer explicit cell size");
    // This edge belongs to the actual input triangle surface, so its complete
    // segment lies in the material closure, even at a reflex corner. It is not
    // inferred from endpoint proximity or from an unrelated source mesh.
    add({uint32_t(n+edge[0]),uint32_t(n+edge[1]),norm(as_point(parts.points[edge[0]])-as_point(parts.points[edge[1]]))});
  }
  memory.add(2*connections.size(),sizeof(Arc));graph.starts.resize(total+1);
  for(auto c:connections){work.step();++graph.starts[c.from+1];++graph.starts[c.to+1];}
  for(size_t i=1;i<graph.starts.size();++i){work.step();graph.starts[i]+=graph.starts[i-1];}
  graph.arcs.resize(graph.starts.back());auto cursor=graph.starts;
  for(auto c:connections){work.step();graph.arcs[cursor[c.from]++]={c.to,c.cost};graph.arcs[cursor[c.to]++]={c.from,c.cost};}
  auto capacity=connections.capacity();std::vector<Connection>().swap(connections);memory.release(capacity,sizeof(Connection));
  return graph;
}
struct Endpoint {
  Point free{};
  uint32_t a=UINT32_MAX,b=UINT32_MAX;
  double t=0;
};
Endpoint vertex_endpoint(uint32_t v){Endpoint e;e.a=e.b=v;return e;}
Endpoint edge_endpoint(uint32_t a,uint32_t b,double t){Endpoint e;e.a=a;e.b=b;e.t=t;if(t==0)e.b=a;if(t==1)e.a=b;return e;}
Point endpoint_value(const Endpoint &e,const Components &parts) {
  if(e.a==UINT32_MAX)return e.free;
  if(e.a>=parts.points.size()||e.b>=parts.points.size()||!std::isfinite(e.t)||e.t<0||e.t>1)
    throw std::logic_error("Invalid owning volume endpoint");
  auto p=as_point(parts.points[e.a]),q=as_point(parts.points[e.b]);
  for(int a=0;a<3;++a)p[a]+=e.t*(q[a]-p[a]);
  return p;
}
// Partition the ENTIRE segment at every possible surface event. Every open
// interval must be classified as material. Coplanar/uncertain contact is
// refused unless both endpoints are structurally owned by one actual triangle,
// whose convex closure independently proves the complete path.
bool segment_material(const Mesh &m,const Components &parts,const Endpoint &from,
                      const Endpoint &to,std::vector<long double> &events,
                      std::vector<volume_exact::Interval> &coverage,Work &work) {
  auto p=endpoint_value(from,parts),q=endpoint_value(to,parts);events.clear();events.push_back(0);events.push_back(1);
  coverage.clear();volume_exact::Arithmetic exact(work);
  auto add_coverage=[&](volume_exact::Interval piece){
    auto begin=std::lower_bound(coverage.begin(),coverage.end(),piece.enter,
      [&](const auto&existing,const auto&at){return exact.compare(existing.exit,at)<0;});
    auto end=begin;
    while(end!=coverage.end()&&exact.compare(end->enter,piece.exit)<=0){
      work.step();if(exact.compare(end->enter,piece.enter)<0)piece.enter=end->enter;
      if(exact.compare(end->exit,piece.exit)>0)piece.exit=end->exit;
      ++end;
    }
    // Charge actual bounded vector movement; capacity was reserved before any
    // path checks. The interval union itself is exact, with no gap tolerance.
    auto index=size_t(begin-coverage.begin());work.step(size_t(coverage.end()-begin));
    if(begin==end)coverage.insert(begin,std::move(piece));
    else {coverage.erase(begin+1,end);coverage[index]=std::move(piece);}
    return exact.compare(coverage.front().enter,exact.zero())<=0&&
      exact.compare(coverage.front().exit,exact.one())>=0;
  };
  auto raw3=[](Vec3 p){return std::array{p.x,p.y,p.z};};
  auto projected=[](Vec3 p,int drop){auto a=std::array{p.x,p.y,p.z};return std::array{a[(drop+1)%3],a[(drop+2)%3]};};
  auto exact_endpoint=[&](const Endpoint&e,int drop){
    if(e.a==UINT32_MAX){volume_exact::Point2 p;p.ordinary=true;p.free={e.free[(drop+1)%3],e.free[(drop+2)%3]};return p;}
    long double parameter=std::ldexp(static_cast<long double>(e.t),60);
    if(parameter!=std::floor(parameter)||parameter<0||parameter>volume_exact::parameter_scale)
      throw std::logic_error("A volume portal exceeds the supported exact dyadic parameter bound");
    return volume_exact::Point2{projected(parts.points[e.a],drop),projected(parts.points[e.b],drop),uint64_t(parameter)};
  };
  auto owns=[](const std::array<uint32_t,3>&ids,const Endpoint&e){if(e.a==UINT32_MAX)return false;return std::find(ids.begin(),ids.end(),e.a)!=ids.end()&&std::find(ids.begin(),ids.end(),e.b)!=ids.end();};
  using L=std::array<long double,3>;
  auto minus=[](L a,L b){for(int i=0;i<3;++i)a[i]-=b[i];return a;};
  auto cross=[](L a,L b){return L{a[1]*b[2]-a[2]*b[1],a[2]*b[0]-a[0]*b[2],a[0]*b[1]-a[1]*b[0]};};
  auto dot=[](L a,L b){return a[0]*b[0]+a[1]*b[1]+a[2]*b[2];};
  L lp{p[0],p[1],p[2]},lq{q[0],q[1],q[2]};
  for(size_t triangle=0;triangle<parts.triangle.size();++triangle) {
    work.step();std::array<L,3> v;std::array<uint32_t,3> ids;
    for(int k=0;k<3;++k){auto index=m.indices[triangle*3+k];ids[k]=parts.vertex[index];auto a=as_point(m.positions[index]);v[k]={a[0],a[1],a[2]};}
    bool start=owns(ids,from),end=owns(ids,to);if(start&&end)return true;
    bool overlap=true;
    for(int a=0;a<3;++a){auto lo=std::min({v[0][a],v[1][a],v[2][a]}),hi=std::max({v[0][a],v[1][a],v[2][a]});long double eps=256*std::numeric_limits<double>::epsilon()*std::max({std::abs(lo),std::abs(hi),std::abs(lp[a]),std::abs(lq[a]),1e-20L});if(std::max(lp[a],lq[a])<lo-eps||std::min(lp[a],lq[a])>hi+eps){overlap=false;break;}}
    if(!overlap)continue;
    auto normal=cross(minus(v[1],v[0]),minus(v[2],v[0])),a=minus(lp,v[0]),b=minus(lq,v[0]);
    long double x=dot(normal,a),y=dot(normal,b),scale=0;
    for(int c=0;c<3;++c)scale+=std::abs(normal[c])*(std::abs(a[c])+std::abs(b[c]));
    long double eps=256*std::numeric_limits<double>::epsilon()*scale;
    if(start)x=0;
    if(end)y=0;
    if(std::abs(x)<=eps&&std::abs(y)<=eps) {
      auto ta=raw3(m.positions[m.indices[triangle*3]]),tb=raw3(m.positions[m.indices[triangle*3+1]]),tc=raw3(m.positions[m.indices[triangle*3+2]]);
      // A constant-coordinate input plane has an exact cheaper predicate.
      // This uses actual binary32 equality, not a near-axis/angle threshold.
      // Every other plane still uses the bounded exact determinant.
      int constant=-1;for(int axis=0;axis<3;++axis){work.step();if(ta[axis]==tb[axis]&&ta[axis]==tc[axis]){constant=axis;break;}}
      auto on_plane=[&](const Endpoint&e){
        if(constant>=0){work.step();if(e.a==UINT32_MAX)return e.free[constant]==double(ta[constant]);return raw3(parts.points[e.a])[constant]==ta[constant]&&raw3(parts.points[e.b])[constant]==ta[constant];}
        if(e.a==UINT32_MAX)return exact.orient3(ta,tb,tc,e.free)==0;
        return exact.orient3(ta,tb,tc,raw3(parts.points[e.a]))==0&&exact.orient3(ta,tb,tc,raw3(parts.points[e.b]))==0;
      };
      if(!on_plane(from)||!on_plane(to))return false;
      int drop=constant;std::array<std::array<float,2>,3> flat;
      if(drop>=0)for(int k=0;k<3;++k)flat[k]=projected(m.positions[m.indices[triangle*3+k]],drop);
      for(int axis=0;axis<3&&drop<0;++axis){for(int k=0;k<3;++k)flat[k]=projected(m.positions[m.indices[triangle*3+k]],axis);if(exact.orient2(flat[0],flat[1],flat[2]).sign)drop=axis;}
      if(drop<0)return false;
      if(auto interval=exact.clip(flat,exact_endpoint(from,drop),exact_endpoint(to,drop)))
        if(add_coverage(std::move(*interval)))return true;
      continue;
    }
    if(start||end)continue;
    if((x>eps&&y>eps)||(x<-eps&&y<-eps))continue;
    if(std::abs(x-y)<=256*std::numeric_limits<double>::epsilon()*scale)return false;
    long double t=x/(x-y);
    if(t<0||t>1){if(std::abs(x)>eps&&std::abs(y)>eps)continue;t=std::clamp(t,0.L,1.L);}
    L hit;for(int c=0;c<3;++c)hit[c]=lp[c]+t*(lq[c]-lp[c]);
    int drop=0;for(int c=1;c<3;++c)if(std::abs(normal[c])>std::abs(normal[drop]))drop=c;
    int u=(drop+1)%3,w=(drop+2)%3;
    auto ax=v[1][u]-v[0][u],ay=v[1][w]-v[0][w],bx=v[2][u]-v[0][u],by=v[2][w]-v[0][w];
    auto hx=hit[u]-v[0][u],hy=hit[w]-v[0][w],det=ax*by-ay*bx;
    auto area_scale=std::abs(ax*by)+std::abs(ay*bx);
    if(std::abs(det)<=256*std::numeric_limits<double>::epsilon()*area_scale)return false;
    auto s=(hx*by-hy*bx)/det,r=(ax*hy-ay*hx)/det;
    auto tolerance=256*std::numeric_limits<double>::epsilon()*
      (area_scale*(1+std::abs(s)+std::abs(r))+std::abs(hx*by)+std::abs(hy*bx)+std::abs(ax*hy)+std::abs(ay*hx))/std::abs(det);
    if(s<-tolerance||r<-tolerance||s+r>1+tolerance)continue;
    if(t<=0||t>=1)return false; // Unowned endpoint contact is ambiguous.
    events.push_back(t);
  }
  if(!coverage.empty()) {
    // A full cover would already have returned above. Partial coplanar contact
    // stays conservative even when another open interval looks interior.
    return false;
  }
  std::sort(events.begin(),events.end(),[&](auto a,auto b){work.step();return a<b;});
  for(size_t i=1;i<events.size();++i) {
    work.step();if(events[i]==events[i-1])continue;
    auto t=(events[i]+events[i-1])*.5L;Point sample;
    for(int a=0;a<3;++a)sample[a]=double(lp[a]+t*(lq[a]-lp[a]));
    auto inside=material_classification(m,parts,sample,work);if(!inside||!*inside)return false;
  }
  return true;
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
  memory.add(input.positions.size(),2*sizeof(uint32_t));
  auto parts=components(m,work);memory.release(input.positions.size(),2*sizeof(uint32_t));
  volume_topology::validate(m,work,memory);
  auto g=grid_for(m,o.cell_size,o.grid_phase,o.maximum_grid_points,memory);
  rasterize(m,g,work);memory.add(64,6*sizeof(uint32_t));mark_cut_cubes(m,g,work);
  auto inside=classify(m,parts,g,work,memory);
  const size_t n=inside.grid.size(),k=o.binding.limits.max_influences;
  memory.add(n,sizeof(uint32_t));
  memory.add(inside.components,sizeof(uint8_t)+sizeof(double)+sizeof(uint32_t));
  std::vector<uint8_t> seeded(inside.components);
  std::vector<double> component_scale(inside.components,1e-5);
  std::vector<uint32_t> bone_seed(inside.components,UINT32_MAX);
  work.step(nt*(16*std::bit_width(nt+1)));
  SurfaceQuery query(m,[&]{work.step();work.check();return false;});
  SurfaceQueryOptions qo;qo.cancelled=[&]{work.step();work.check();return false;};
  qo.triangle_filter=[&](uint32_t){work.step();return true;};
  validate_resolution(m,parts,g,query,qo,work);
  auto first_hit=[&](Point a,Point b)->std::optional<SurfaceHit> {
    auto delta=b-a;double distance=norm(delta);
    if(distance<g.h*1e-6)return {};
    double trim=std::min(distance*.001,g.h*1e-5);
    for(int c=0;c<3;++c)a[c]+=delta[c]*(trim/distance);
    return query.raycast(as_vec(a),as_vec(delta),float(distance-2*trim),qo);
  };
  auto visible=[&](Point a,Point b){return !first_hit(a,b);};
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
  auto boundary=boundary_graph(parts,g,inside,visible,work,memory);
  memory.add(nt+2,sizeof(long double));
  memory.add(nt,sizeof(volume_exact::Interval));
  std::vector<long double> segment_events;segment_events.reserve(nt+2);
  std::vector<volume_exact::Interval> segment_coverage;segment_coverage.reserve(nt);
  struct Portal {uint32_t a,b;double t,cost;};
  memory.add(nt,3*sizeof(uint32_t)+3*sizeof(Portal));
  std::vector<uint32_t> marks(nt),patch,todo;patch.reserve(nt);todo.reserve(nt);
  std::vector<Portal> portals;portals.reserve(3*nt);uint32_t epoch=0;
  auto portal_distance=[&](uint32_t vertex,uint32_t triangle,Point bone_a,Point bone_b,Point bone_anchor)->double {
    work.step();auto component=boundary.component[vertex];
    if(boundary.component[parts.vertex[m.indices[triangle*3]]]!=component)return INFINITY;
    auto p=as_point(parts.points[vertex]);patch.clear();todo.clear();portals.clear();
    if(++epoch==0){for(auto &mark:marks){work.step();mark=0;}epoch=1;}
    auto reference=as_point(m.positions[m.indices[triangle*3]]);
    auto plane=cross3(as_point(m.positions[m.indices[triangle*3+1]])-reference,as_point(m.positions[m.indices[triangle*3+2]])-reference);
    auto coplanar=[&](uint32_t face){work.step();for(int k=0;k<3;++k){auto d=as_point(m.positions[m.indices[face*3+k]])-reference;double value=dot3(plane,d),scale=0;for(int c=0;c<3;++c)scale+=std::abs(plane[c]*d[c]);if(std::abs(value)>256*std::numeric_limits<double>::epsilon()*scale)return false;}return true;};
    marks[triangle]=epoch;todo.push_back(triangle);
    while(!todo.empty()){work.step();auto face=todo.back();todo.pop_back();patch.push_back(face);for(auto next:parts.adjacent[face])if(next!=UINT32_MAX&&marks[next]!=epoch&&coplanar(next)){marks[next]=epoch;todo.push_back(next);}}
    // Candidate discovery follows the actual planar topology patch, not an
    // arbitrary triangulation diagonal. It grants no path authority: each
    // chosen real edge still needs the independent whole-segment proof.
    for(auto face:patch)for(int edge=0;edge<3;++edge) {
      work.step();auto next=parts.adjacent[face][edge];if(next!=UINT32_MAX&&marks[next]==epoch)continue;
      auto va=parts.vertex[m.indices[face*3+edge]],vb=parts.vertex[m.indices[face*3+(edge+1)%3]];
      auto a=as_point(parts.points[va]),b=as_point(parts.points[vb]),direction=b-a;
      auto point=[&](double t){Point r=a;for(int c=0;c<3;++c)r[c]+=t*direction[c];return r;};
      double lo=0,hi=1;
      for(int iteration=0;iteration<56;++iteration){work.step();auto r=point((lo+hi)*.5),q=closest_segment(r,bone_a,bone_b);auto x=r-p,y=r-q;double lx=norm(x),ly=norm(y);double derivative=(lx?dot3(x,direction)/lx:0)+(ly?dot3(y,direction)/ly:0);if(derivative<0)lo=(lo+hi)*.5;else hi=(lo+hi)*.5;}
      double t=(lo+hi)*.5;auto r=point(t);portals.push_back({va,vb,t,norm(p-r)+norm(r-closest_segment(r,bone_a,bone_b))});
    }
    std::sort(portals.begin(),portals.end(),[&](const Portal&a,const Portal&b){work.step();return a.cost<b.cost;});
    double best=INFINITY;
    for(auto candidate:portals) {
      auto va=candidate.a,vb=candidate.b;auto a=as_point(parts.points[va]),b=as_point(parts.points[vb]),direction=b-a;
      auto point=[&](double t){Point r=a;for(int c=0;c<3;++c)r[c]+=t*direction[c];return r;};
      for(double t:{candidate.t,0.,1.}) {
        work.step();auto r=point(t),q=closest_segment(r,bone_a,bone_b);
        double cost=norm(p-r)+norm(r-q);if(cost>=best)continue;
        Endpoint anchor;anchor.free=bone_anchor;Endpoint target;target.free=q;
        auto portal=edge_endpoint(va,vb,t),source=vertex_endpoint(vertex);
        if(segment_material(m,parts,anchor,target,segment_events,segment_coverage,work)&&
           segment_material(m,parts,source,portal,segment_events,segment_coverage,work)&&
           segment_material(m,parts,portal,target,segment_events,segment_coverage,work))best=cost;
      }
    }
    return best;
  };
  const size_t total_nodes=n+parts.points.size();
  if(total_nodes>UINT32_MAX)throw std::length_error("Volume label index budget exceeded");
  memory.add(total_nodes,sizeof(Slot)+sizeof(uint32_t));
  memory.add(parts.points.size(),k*sizeof(Label));
  std::vector<Slot> field(total_nodes);
  std::vector<Label> labels(parts.points.size()*k);
  auto worlds=s.world();bool deform=false;
  double radius=std::min(double(o.binding.maximum_seed_distance),g.h+o.binding.seed_margin);
  for(uint32_t bone=0;bone<s.bones.size();++bone)if(s.bones[bone].deform) {
    deform=true;const auto &w=worlds[bone];Point a{w[12]-center[0],w[13]-center[1],w[14]-center[2]},b=a;
    for(int c=0;c<3;++c)b[c]+=double(w[4+c])*s.bones[bone].length;
    for(auto &slot:field){work.step();slot=Slot{};}
    for(auto &anchor:bone_seed){work.step();anchor=UINT32_MAX;}
    Pending queue(field,1,work);
    for(uint32_t v=0;v<n;++v) {
      work.step();auto p=g.point(inside.grid[v]),q=closest_segment(p,a,b);double distance=norm(p-q);
      if(distance<=radius && visible(p,q) && material_classification(m,parts,q,work)==std::optional<bool>(true)) {
        queue.relax(v,{distance,bone});
        seeded[inside.component[v]]=1;
        component_scale[inside.component[v]]=std::max(component_scale[inside.component[v]],norm(b-a)*.02);
        auto &anchor=bone_seed[inside.component[v]];
        if(anchor==UINT32_MAX)anchor=v;
      }
    }
    // Exact visible source-to-boundary paths are additional initial values.
    // The bone endpoint first needs a genuine same-volume interior witness.
    for(uint32_t v=0;v<parts.points.size();++v) {
      work.step();auto component=boundary.component[v];auto anchor=bone_seed[component];
      if(anchor==UINT32_MAX)continue;
      auto p=as_point(parts.points[v]),endpoint=closest_segment(p,a,b);
      auto bone_anchor=g.point(inside.grid[anchor]);
      if(!visible(bone_anchor,endpoint)||material_classification(m,parts,endpoint,work)!=std::optional<bool>(true))continue;
      auto hit=first_hit(p,endpoint);
      if(!hit)queue.relax(uint32_t(n+v),{norm(p-endpoint),bone});
      else {
        auto distance=portal_distance(v,hit->triangle,a,b,bone_anchor);
        if(std::isfinite(distance))queue.relax(uint32_t(n+v),{distance,bone});
      }
    }
    while(!queue.empty()) {
      work.step();auto node=queue.pop();auto item=field[node];
      for(size_t d=0;node<n&&d<directions.size();++d)if(links[node]&(uint32_t(1)<<d)) {
        work.step();auto next=offset(g,inside.grid[node],directions[d]);auto id=g.nodes[*next];
        auto direction=directions[d];double cost=g.h*std::sqrt(double(direction[0]*direction[0]+direction[1]*direction[1]+direction[2]*direction[2]));
        queue.relax(uint32_t(id),{item.distance+cost,item.bone});
      }
      for(size_t edge=boundary.starts[node];edge<boundary.starts[node+1];++edge) {
        work.step();auto e=boundary.arcs[edge];queue.relax(e.to,{item.distance+e.cost,item.bone});
      }
      if(node<n)for(int direction=0;direction<6;++direction) {
        work.step();auto next=g.neighbor(inside.grid[node],direction);if(!next)continue;
        auto id=g.nodes[*next];if(id<0||field[id].settled)continue;
        double distance=eikonal_value(uint32_t(id),g,inside,field,work);
        if(std::isfinite(distance))queue.relax(uint32_t(id),{distance,bone});
      }
    }
    // Complete each nonlinear field independently. Graph K-label dominance
    // is not assumed for this upwind PDE discretization.
    for(size_t v=0;v<parts.points.size();++v){work.step();auto value=field[n+v].distance;
      if(std::isfinite(value))insert_label(std::span<Label>(labels).subspan(v*k,k),{value,bone});}
  }
  if(!deform)throw std::runtime_error("Volume binding requires a deforming bone");
  for(size_t c=0;c<seeded.size();++c)if(!seeded[c])
    throw std::runtime_error("No internal deform-bone seed for volume component "+std::to_string(c));
  memory.add(m.positions.size(),sizeof(VertexWeights)+k*sizeof(Influence));
  memory.add(parts.points.size(),sizeof(VertexWeights)+k*sizeof(Influence));
  memory.add(k,sizeof(Label));
  work.step(parts.points.size()*k*16+m.positions.size()*k);
  std::vector<VertexWeights> vertex_rows;vertex_rows.reserve(parts.points.size());
  SkinWeights out;out.vertices.reserve(m.positions.size());
  for(uint32_t vertex=0;vertex<parts.points.size();++vertex) {
    work.step();std::vector<Label> row(k);
    for(size_t c=0;c<k;++c){auto l=labels[vertex*k+c];if(l.bone!=UINT32_MAX)insert_label(row,l);}
    if(row.front().bone==UINT32_MAX)throw std::logic_error("Volume boundary node lost its seeded path");
    // Physical falloff must not change merely because the numerical grid is
    // refined. Use the legacy2%-bone-length policy, scoped to actual sources
    // inside this volume, rather than using cell_size as a regularizer.
    VertexWeights weights;weights.reserve(k);double nearest=row.front().distance,scale=component_scale[boundary.component[vertex]];
    for(auto l:row)if(l.bone!=UINT32_MAX)
      weights.push_back({l.bone,float(std::pow((nearest+scale)/(l.distance+scale),o.binding.falloff))});
    vertex_rows.push_back(normalize_weights(std::move(weights),s.bones.size(),{},o.binding.limits));
  }
  for(auto vertex:parts.vertex){work.step();out.vertices.push_back(vertex_rows[vertex]);}
  out.validate(m.positions.size(),s.bones.size());work.check();
  if(report)*report={g.count(),n,inside.components,work.used,memory.peak};
  return out;
}
} // namespace dome::detail
