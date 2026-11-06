// SPDX-License-Identifier: GPL-3.0-or-later
#include "sculpt-surface-chunks.hpp"
#include <bit>
#include <limits>
#include <queue>
#include <set>

namespace dome::detail {
bool SculptChunkBounds::empty() const { return minimum.x > maximum.x; }
void SculptChunkBounds::include(Vec3 p) {
  if (!finite(p)) throw std::invalid_argument("Nonfinite sculpt chunk bounds");
  minimum = {std::min(minimum.x,p.x),std::min(minimum.y,p.y),std::min(minimum.z,p.z)};
  maximum = {std::max(maximum.x,p.x),std::max(maximum.y,p.y),std::max(maximum.z,p.z)};
}
void SculptChunkBounds::include(const SculptChunkBounds &b) {
  if (!b.empty()) { include(b.minimum); include(b.maximum); }
}
namespace {
using Chunk = SculptSurfaceChunks::Chunk;
using Position = std::array<float,3>;
Position position(Vec3 p) { return {p.x,p.y,p.z}; }
struct Request {
  Int3 cell;
  uint32_t ordinal = 0;
  auto operator<=>(const Request &) const = default;
};
struct EdgeRecord {
  Request request;
  SurfaceEdgeVertex vertex;
};
struct PositionRequest {
  Position point;
  Request request;
  SurfaceEdge edge;
  auto operator<=>(const PositionRequest &) const = default;
};
PositionRequest key(const EdgeRecord &r) {
  return {position(r.vertex.position),r.request,r.vertex.edge};
}
struct Work {
  const SculptChunkLimits &limits;
  SculptSurfaceChunks::Cancel cancel;
  size_t bytes = 0, steps = 0;
  void tick() {
    if (steps >= limits.steps) throw std::length_error("Sculpt chunk work limit exceeded");
    ++steps;
    if ((steps & 63) == 0) stop();
  }
  void stop() const {
    if (cancel && cancel()) throw std::runtime_error("Sculpt chunk operation cancelled");
  }
  void charge(size_t n, size_t width = 1) {
    if (bytes > limits.working_bytes ||
        (width && n > (limits.working_bytes-bytes)/width))
      throw std::length_error("Sculpt chunk working byte limit exceeded");
    bytes += n*width;
  }
};
template<class T> SculptChunkBounds value_bounds(const T &) { return {}; }
SculptChunkBounds value_bounds(const Chunk &c) { return c ? c->bounds : SculptChunkBounds{}; }
template<class T> size_t value_bytes(const T &) { return 0; }
size_t value_bytes(const Chunk &c) { return c?c->bytes:0; }
size_t sum(size_t a,size_t b) {
  if(b>std::numeric_limits<size_t>::max()-a)throw std::length_error("Sculpt chunk accounting overflow");
  return a+b;
}

// Persistent AVL directory. Updates clone only the search/rotation path; a
// complete next manifest does not copy all previous tile/edge metadata. There
// is no parent pointer or global cache retaining obsolete manifests.
template<class K,class V> struct Directory {
  struct Node;
  using Ptr = std::shared_ptr<const Node>;
  struct Node {
    K key;
    V value;
    Ptr left,right;
    unsigned height;
    size_t count,payload_bytes;
    SculptChunkBounds bounds;
    Node(K k,V v,Ptr a,Ptr b) : key(std::move(k)),value(std::move(v)),
        left(std::move(a)),right(std::move(b)),
        height(1+std::max(h(left),h(right))),count(sum(1,sum(n(left),n(right)))),
        payload_bytes(sum(value_bytes(value),sum(left?left->payload_bytes:0,right?right->payload_bytes:0))),
        bounds(value_bounds(value)) {
      if(left)bounds.include(left->bounds);
      if(right)bounds.include(right->bounds);
    }
  };
  Ptr root;
  static unsigned h(const Ptr &p) { return p?p->height:0; }
  static size_t n(const Ptr &p) { return p?p->count:0; }
  static Ptr node(const K &k,const V &v,Ptr a,Ptr b,Work &w) {
    w.tick();
    // Requested node/control-block storage plus a conservative allocation
    // allowance. This is an owning working budget, not an RSS measurement.
    w.charge(sizeof(Node)+128);
    return std::make_shared<const Node>(k,v,std::move(a),std::move(b));
  }
  static Ptr balanced(const K &k,const V &v,Ptr a,Ptr b,Work &w) {
    if(h(a)>h(b)+1) {
      if(h(a->left)>=h(a->right))
        return node(a->key,a->value,a->left,node(k,v,a->right,b,w),w);
      auto c=a->right;
      return node(c->key,c->value,node(a->key,a->value,a->left,c->left,w),
                  node(k,v,c->right,b,w),w);
    }
    if(h(b)>h(a)+1) {
      if(h(b->right)>=h(b->left))
        return node(b->key,b->value,node(k,v,a,b->left,w),b->right,w);
      auto c=b->left;
      return node(c->key,c->value,node(k,v,a,c->left,w),
                  node(b->key,b->value,c->right,b->right,w),w);
    }
    return node(k,v,std::move(a),std::move(b),w);
  }
  static Ptr put(Ptr p,const K &k,const V &v,Work &w) {
    w.tick();
    if(!p)return node(k,v,{},{},w);
    if(k<p->key)return balanced(p->key,p->value,put(p->left,k,v,w),p->right,w);
    if(p->key<k)return balanced(p->key,p->value,p->left,put(p->right,k,v,w),w);
    return node(k,v,p->left,p->right,w);
  }
  static Ptr remove(Ptr p,const K &k,Work &w) {
    w.tick();
    if(!p)return {};
    if(k<p->key)return balanced(p->key,p->value,remove(p->left,k,w),p->right,w);
    if(p->key<k)return balanced(p->key,p->value,p->left,remove(p->right,k,w),w);
    if(!p->left)return p->right;
    if(!p->right)return p->left;
    auto next=p->right.get();while(next->left) { w.tick(); next=next->left.get(); }
    return balanced(next->key,next->value,p->left,remove(p->right,next->key,w),w);
  }
  const V *find(const K &k) const {
    auto p=root.get();
    while(p) { if(k<p->key)p=p->left.get();else if(p->key<k)p=p->right.get();else return &p->value; }
    return nullptr;
  }
  void put(const K &k,const V &v,Work &w) { root=put(root,k,v,w); }
  void erase(const K &k,Work &w) { root=remove(root,k,w); }
  template<class F> static void visit(const Ptr &p,F &fn,Work &w) {
    if(!p)return;
    visit(p->left,fn,w);w.tick();fn(p->key,p->value);visit(p->right,fn,w);
  }
  template<class F> void visit(F fn,Work &w) const { visit(root,fn,w); }
};
using Edges=Directory<SurfaceEdge,EdgeRecord>;
using Positions=Directory<PositionRequest,EdgeRecord>;
using Chunks=Directory<Int3,Chunk>;

const SurfaceCell *cell_at(const LocalSurfaceCache &cache,Int3 p) {
  auto tile=cache.tiles().find(LocalSurfaceCache::tile_coordinate(p));
  if(tile==cache.tiles().end())return nullptr;
  const auto &cells=tile->second->cells;
  auto i=std::lower_bound(cells.begin(),cells.end(),p,
                         [](const auto &c,Int3 q){return c.coordinate<q;});
  return i!=cells.end()&&i->coordinate==p?&*i:nullptr;
}
template<class F> void edge_cells(SurfaceEdge edge,F fn,Work &w) {
  const int av[]={edge.a.x,edge.a.y,edge.a.z},bv[]={edge.b.x,edge.b.y,edge.b.z};
  int64_t low[3],high[3];
  for(int i=0;i<3;++i) {
    if(std::abs(int64_t(av[i])-bv[i])>1)throw std::logic_error("Nonlocal extraction edge");
    low[i]=std::max(int64_t(av[i]),int64_t(bv[i]))-1;
    high[i]=std::min(av[i],bv[i]);
  }
  for(auto x=low[0];x<=high[0];++x)
    for(auto y=low[1];y<=high[1];++y)
      for(auto z=low[2];z<=high[2];++z) {
        w.tick();
        if(x<INT32_MIN||y<INT32_MIN||z<INT32_MIN||x>INT32_MAX||y>INT32_MAX||z>INT32_MAX)continue;
        fn(Int3{int(x),int(y),int(z)});
      }
}
std::optional<EdgeRecord> first_request(const LocalSurfaceCache &cache,
                                       SurfaceEdge edge,Work &w) {
  std::optional<EdgeRecord> result;
  edge_cells(edge,[&](Int3 p){
    if(auto cell=cell_at(cache,p))
      for(size_t i=0;i<cell->vertices.size();++i) {
        w.tick();const auto &v=cell->vertices[i];
        if(v.edge==edge) {
          EdgeRecord r{{p,uint32_t(i)},v};
          if(!result||r.request<result->request)result=r;
          break;
        }
      }
  },w);
  return result;
}
const EdgeRecord &first_position(const Positions &positions,Position point) {
  const Positions::Node *best=nullptr;
  auto p=positions.root.get();
  while(p) {
    if(p->key.point<point)p=p->right.get();
    else { if(!(point<p->key.point))best=p;p=p->left.get(); }
  }
  if(!best)throw std::logic_error("Missing exact sculpt position owner");
  return best->value;
}
template<class F> void position_members(const Positions::Ptr &p,Position point,F &fn,Work &w) {
  if(!p)return;w.tick();
  if(!(p->key.point<point))position_members(p->left,point,fn,w);
  if(!(p->key.point<point)&&!(point<p->key.point))fn(p->value);
  if(!(point<p->key.point))position_members(p->right,point,fn,w);
}
template<class T> void scratch_insert(std::set<T> &values,const T &v,size_t maximum,Work &w) {
  w.tick();if(values.contains(v))return;
  if(values.size()>=maximum)throw std::length_error("Sculpt chunk dependency limit exceeded");
  w.charge(sizeof(T)+128);values.insert(v);
}
std::array<double,3> cross(Vec3 a,Vec3 b,Vec3 c) {
  double x=double(b.x)-a.x,y=double(b.y)-a.y,z=double(b.z)-a.z;
  double u=double(c.x)-a.x,v=double(c.y)-a.y,t=double(c.z)-a.z;
  return {y*t-z*v,z*u-x*t,x*v-y*u};
}
struct ExactRepair {};
bool same_chunk(const SculptSurfaceChunk &a,const SculptSurfaceChunk &b,Work &w) {
  if(a.coordinate!=b.coordinate||a.mesh.indices.size()!=b.mesh.indices.size()||
      a.triangles.size()!=b.triangles.size()||a.mesh.positions.size()!=b.mesh.positions.size()||
      a.mesh.normals.size()!=b.mesh.normals.size())return false;
  for(size_t i=0;i<a.mesh.indices.size();++i) {w.tick();if(a.mesh.indices[i]!=b.mesh.indices[i])return false;}
  for(size_t i=0;i<a.triangles.size();++i) {w.tick();if(a.triangles[i]!=b.triangles[i])return false;}
  auto same=[](Vec3 x,Vec3 y){return std::bit_cast<std::array<uint32_t,3>>(x)==std::bit_cast<std::array<uint32_t,3>>(y);};
  for(size_t i=0;i<a.mesh.positions.size();++i) {
    w.tick();if(!same(a.mesh.positions[i],b.mesh.positions[i])||!same(a.mesh.normals[i],b.mesh.normals[i]))return false;
  }
  return true;
}
Chunk assemble(const SurfaceTile &tile,const Edges &edges,const Positions &positions,Work &w) {
  size_t maximum=0;
  for(const auto &cell:tile.cells) {
    w.tick();
    if(cell.indices.size()>w.limits.extraction.indices-maximum)
      throw std::length_error("Sculpt chunk index limit exceeded");
    maximum+=cell.indices.size();
  }
  if(maximum>UINT32_MAX)throw std::length_error("Sculpt chunk vertex index overflow");
  w.charge(sizeof(SculptSurfaceChunk)+128);
  w.charge(maximum,2*sizeof(Vec3)+sizeof(uint32_t));
  w.charge(maximum/3,sizeof(SculptTriangleOrigin));
  auto out=std::make_shared<SculptSurfaceChunk>();out->coordinate=tile.coordinate;
  out->mesh.positions.reserve(maximum);out->mesh.normals.reserve(maximum);
  out->mesh.indices.reserve(maximum);out->triangles.reserve(maximum/3);
  std::map<Position,uint32_t> vertices;
  for(const auto &cell:tile.cells) {
    for(size_t triangle=0;triangle<cell.indices.size()/3;++triangle) {
      w.tick();std::array<const EdgeRecord *,3> records;
      for(size_t c=0;c<3;++c) {
        const auto &request=cell.vertices.at(cell.indices.at(3*triangle+c));
        auto edge=edges.find(request.edge);
        if(!edge)throw std::logic_error("Missing sculpt edge owner");
        records[c]=&first_position(positions,position(edge->vertex.position));
      }
      const auto a=records[0]->vertex.position,b=records[1]->vertex.position,c=records[2]->vertex.position;
      if(position(a)==position(b)||position(b)==position(c)||position(c)==position(a))continue;
      const auto n=cross(a,b,c);
      if(n==std::array<double,3>{})throw ExactRepair{};
      const auto outward=cell.outward.at(triangle);
      if(n[0]*outward.x+n[1]*outward.y+n[2]*outward.z<0)std::swap(records[1],records[2]);
      for(const auto *record:records) {
        auto p=position(record->vertex.position);auto i=vertices.find(p);
        if(i==vertices.end()) {
          w.charge(sizeof(std::pair<const Position,uint32_t>)+128);
          const auto id=uint32_t(out->mesh.positions.size());
          i=vertices.emplace(p,id).first;
          out->mesh.positions.push_back(record->vertex.position);
          out->mesh.normals.push_back(record->vertex.normal);
          out->bounds.include(record->vertex.position);
        }
        out->mesh.indices.push_back(i->second);
      }
      out->triangles.push_back({cell.coordinate,uint32_t(triangle)});
    }
  }
  out->bytes=sizeof(SculptSurfaceChunk)+128+
      out->mesh.positions.capacity()*sizeof(Vec3)+out->mesh.normals.capacity()*sizeof(Vec3)+
      out->mesh.indices.capacity()*sizeof(uint32_t)+out->triangles.capacity()*sizeof(SculptTriangleOrigin);
  return out;
}
} // namespace

struct SculptSurfaceChunks::State {
  SculptChunkLimits limits;
  LocalSurfaceCache::Ptr cache;
  Edges edges;
  Positions positions;
  Chunks chunks;
  SculptChunkReport report;
  SculptChunkBounds bounds;
  std::weak_ptr<const State> base;
  std::vector<std::pair<Int3,Chunk>> changes;
  Representation representation=Representation::Chunks;
  static std::shared_ptr<const State> compile(LocalSurfaceCache::Ptr cache,
      SculptChunkLimits limits,std::shared_ptr<const State> before,const std::vector<Int3> &changed,
      size_t prior_bytes,Cancel cancel) {
    Work w{limits,std::move(cancel)};w.stop();
    // Both source owners coexist throughout a local operation. Count both
    // conservatively even where the sample-page implementation shares pages.
    w.charge(cache->stats().bytes);
    w.charge(prior_bytes);
    w.charge(changed.capacity(),sizeof(Int3));
    w.charge(sizeof(State)+sizeof(SculptSurfaceChunks)+256);
    auto out=std::make_shared<State>();out->cache=std::move(cache);out->limits=limits;
    out->base=before;
    out->report.kind=before?SculptChunkReport::Kind::Local:SculptChunkReport::Kind::Rebuild;
    out->report.extraction_directory_copy=bool(before);
    std::set<SurfaceEdge> dirty_edges;
    std::set<Position> dirty_positions;
    std::set<Int3> dirty_chunks;
    auto collect_tile=[&](const LocalSurfaceCache &source,Int3 tile){
      auto i=source.tiles().find(tile);if(i==source.tiles().end())return;
      for(const auto &cell:i->second->cells)for(const auto &v:cell.vertices)
        scratch_insert(dirty_edges,v.edge,limits.dependency_edges,w);
    };
    if(before) {
      out->edges=before->edges;out->positions=before->positions;out->chunks=before->chunks;
      out->report.vertices=before->report.vertices;out->report.indices=before->report.indices;
      for(auto tile:changed) {
        collect_tile(*before->cache,tile);collect_tile(*out->cache,tile);
        scratch_insert(dirty_chunks,tile,limits.dependency_chunks,w);
      }
    } else {
      for(const auto &[tile,packet]:out->cache->tiles()) {
        (void)packet;collect_tile(*out->cache,tile);
        scratch_insert(dirty_chunks,tile,limits.extraction.tiles,w);
      }
    }
    out->report.extracted_tiles=before?changed.size():out->cache->tiles().size();
    out->report.dependency_edges=dirty_edges.size();
    for(auto edge:dirty_edges) {
      w.tick();
      if(auto old=out->edges.find(edge)) {
        const auto old_key=key(*old);
        scratch_insert(dirty_positions,old_key.point,limits.dependency_edges,w);
        out->positions.erase(old_key,w);out->edges.erase(edge,w);
      }
      if(auto next=first_request(*out->cache,edge,w)) {
        if(!finite(next->vertex.position)||!finite(next->vertex.normal))
          throw std::invalid_argument("Nonfinite sculpt representative");
        scratch_insert(dirty_positions,position(next->vertex.position),limits.dependency_edges,w);
        out->edges.put(edge,*next,w);out->positions.put(key(*next),*next,w);
      }
    }
    if(before) {
      // Exact-position aliases need not be spatially adjacent at large float
      // coordinates. Revisit actual members in both owners, including an old
      // representative which disappeared, instead of assuming one tile halo.
      auto affected=[&](const EdgeRecord &record){
        edge_cells(record.vertex.edge,[&](Int3 cell){
          auto add=[&](const LocalSurfaceCache &source){
            if(auto packet=cell_at(source,cell))
              for(const auto &v:packet->vertices)if(v.edge==record.vertex.edge) {
                scratch_insert(dirty_chunks,LocalSurfaceCache::tile_coordinate(cell),limits.dependency_chunks,w);break;
              }
          };
          add(*before->cache);add(*out->cache);
        },w);
      };
      for(auto p:dirty_positions) {
        position_members(before->positions.root,p,affected,w);
        position_members(out->positions.root,p,affected,w);
      }
    }
    if(before) {
      w.charge(dirty_chunks.size(),sizeof(std::pair<Int3,Chunk>));
      out->changes.reserve(dirty_chunks.size());
    }
    try {
      for(auto tile:dirty_chunks) {
        w.tick();
        Chunk old_chunk;
        if(auto old=out->chunks.find(tile)) {
          old_chunk=*old;
          out->report.vertices-=(*old)->mesh.positions.size();
          out->report.indices-=(*old)->mesh.indices.size();out->chunks.erase(tile,w);
        }
        auto i=out->cache->tiles().find(tile);
        if(i==out->cache->tiles().end()) {
          if(before&&old_chunk)out->changes.push_back({tile,{}});
          continue;
        }
        auto chunk=assemble(*i->second,out->edges,out->positions,w);
        if(chunk->mesh.indices.empty()) {
          if(before&&old_chunk)out->changes.push_back({tile,{}});
          continue;
        }
        // Geometry-irrelevant source edits can still re-extract a halo. Keep
        // the exact prior owning packet when every corner/provenance bit is
        // unchanged, so GPU clients do not upload merely because Volume COW
        // ownership changed.
        if(old_chunk&&same_chunk(*old_chunk,*chunk,w))chunk=old_chunk;
        if(chunk->mesh.positions.size()>limits.extraction.vertices-out->report.vertices||
            chunk->mesh.indices.size()>limits.extraction.indices-out->report.indices)
          throw std::length_error("Sculpt manifest geometry limit exceeded");
        out->report.vertices+=chunk->mesh.positions.size();out->report.indices+=chunk->mesh.indices.size();
        out->chunks.put(tile,chunk,w);++out->report.assembled_chunks;
        if(before&&chunk!=old_chunk)out->changes.push_back({tile,chunk});
      }
    } catch(const ExactRepair &) {
      // No approximation, local orientation guess, or permanent guard lift.
      // The caller gets an explicit exact-legacy representation to flatten at
      // its cancellable operation/worker boundary. No partial chunk set leaks.
      out->representation=Representation::ExactLegacyRepair;
      out->edges={};out->positions={};out->chunks={};out->bounds={};
      out->changes.clear();out->base.reset();
      out->report.vertices=out->report.indices=0;
    }
    size_t retained=out->cache->stats().bytes;
    auto retain=[&](size_t n,size_t width){
      if(retained>limits.retained_bytes||n>(limits.retained_bytes-retained)/width)
        throw std::length_error("Sculpt manifest retained byte limit exceeded");
      retained+=n*width;
    };
    retain(1,sizeof(State)+sizeof(SculptSurfaceChunks)+256);
    retain(Edges::n(out->edges.root),sizeof(Edges::Node)+128);
    retain(Positions::n(out->positions.root),sizeof(Positions::Node)+128);
    retain(Chunks::n(out->chunks.root),sizeof(Chunks::Node)+128);
    retain(out->changes.capacity(),sizeof(std::pair<Int3,Chunk>));
    if(out->chunks.root)retain(1,out->chunks.root->payload_bytes);
    if(out->chunks.root)out->bounds=out->chunks.root->bounds;
    out->report.retained_bytes=retained;out->report.working_bytes=w.bytes;
    w.stop();return out;
  }
};

SculptSurfaceChunks::SculptSurfaceChunks(std::shared_ptr<const State> s):state_(std::move(s)) {}
SculptSurfaceChunks::Ptr SculptSurfaceChunks::build(const Volume &v,SculptChunkLimits limits,Cancel cancel) {
  Work forecast{limits,cancel};forecast.stop();
  forecast.charge(limits.extraction.bytes);
  forecast.charge(sizeof(State)+sizeof(SculptSurfaceChunks)+256);
  auto cache=LocalSurfaceCache::build(v,limits.extraction,cancel);
  auto state=State::compile(std::move(cache),limits,{}, {},0,cancel);
  return Ptr(new SculptSurfaceChunks(std::move(state)));
}
SculptSurfaceChunks::Ptr SculptSurfaceChunks::update(const Volume &v,Cancel cancel) const {
  Work forecast{state_->limits,cancel};forecast.stop();
  const auto &source=*state_->cache->source_owner();
  if(v.voxel_size()==source.voxel_size()&&v.band()==source.band()&&
      v.geometry_change(source.geometry_owner()).kind==Volume::GeometryChange::Kind::Unchanged)
    return shared_from_this();
  forecast.charge(state_->report.retained_bytes);
  // Extraction itself precedes compilation. Reserve its complete configured
  // envelope before any cache allocation; checking actual stats only after
  // extraction would be a late resource refusal. Include its bounded dirty
  // set/vector scratch, which is not retained by the resulting cache.
  forecast.charge(state_->limits.extraction.bytes);
  forecast.charge(state_->limits.extraction.dirty_tiles,sizeof(Int3)+128);
  forecast.charge(state_->limits.extraction.dirty_tiles,sizeof(Int3));
  std::vector<Int3> changed;changed.reserve(state_->limits.extraction.dirty_tiles);
  LocalSurfaceUpdateReport report;
  auto cache=state_->cache->update_tracked(v,cancel,&report,&changed);
  if(report.kind==LocalSurfaceUpdateReport::Kind::Reused)return shared_from_this();
  auto before=report.kind==LocalSurfaceUpdateReport::Kind::Local&&
      representation()==Representation::Chunks?state_:std::shared_ptr<const State>{};
  auto state=State::compile(std::move(cache),state_->limits,before,changed,
                            state_->report.retained_bytes,cancel);
  return Ptr(new SculptSurfaceChunks(std::move(state)));
}
SculptSurfaceChunks::Representation SculptSurfaceChunks::representation() const {return state_->representation;}
const SculptChunkReport &SculptSurfaceChunks::report() const {return state_->report;}
const LocalSurfaceCache::Owner &SculptSurfaceChunks::source_owner() const {return state_->cache->source_owner();}
const SculptChunkBounds &SculptSurfaceChunks::bounds() const {return state_->bounds;}
void SculptSurfaceChunks::for_each_chunk(const std::function<void(const Chunk &)> &fn,Cancel cancel) const {
  if(representation()!=Representation::Chunks)throw std::logic_error("Exact repair requires legacy surface materialization");
  Work w{state_->limits,std::move(cancel)};w.stop();
  state_->chunks.visit([&](const Int3 &,const Chunk &c){fn(c);},w);w.stop();
}
bool SculptSurfaceChunks::for_each_change(const Ptr &base,
    const std::function<void(Int3,const Chunk &)> &fn,Cancel cancel) const {
  Work w{state_->limits,std::move(cancel)};w.stop();
  if(!base)return false;
  if(base->state_==state_)return true;
  const auto expected=state_->base.lock();
  if(!expected||base->state_!=expected||representation()!=Representation::Chunks)return false;
  for(const auto &[key,chunk]:state_->changes) {w.tick();fn(key,chunk);}
  w.stop();return true;
}
void SculptSurfaceChunks::for_each_triangle(
    const std::function<void(const Chunk &,size_t,const std::array<Vec3,3> &,const std::array<Vec3,3> &)> &fn,
    Cancel cancel) const {
  Work w{state_->limits,cancel};w.stop();
  for_each_chunk([&](const Chunk &chunk){
    for(size_t t=0;t<chunk->triangles.size();++t) {
      w.tick();std::array<Vec3,3> p,n;
      for(size_t k=0;k<3;++k) {auto v=chunk->mesh.indices.at(3*t+k);p[k]=chunk->mesh.positions.at(v);n[k]=chunk->mesh.normals.at(v);}
      fn(chunk,t,p,n);
    }
  },cancel);w.stop();
}
Mesh SculptSurfaceChunks::materialize(Cancel cancel) const {
  if(representation()==Representation::ExactLegacyRepair)return state_->cache->materialize(std::move(cancel));
  Work w{state_->limits,std::move(cancel)};w.stop();w.charge(state_->report.retained_bytes);
  struct Cursor {Chunk chunk;size_t triangle;};
  auto later=[](const Cursor &a,const Cursor &b){return a.chunk->triangles[a.triangle]>b.chunk->triangles[b.triangle];};
  w.charge(Chunks::n(state_->chunks.root),sizeof(Cursor));
  std::vector<Cursor> storage;storage.reserve(Chunks::n(state_->chunks.root));
  std::priority_queue<Cursor,std::vector<Cursor>,decltype(later)> queue(later,std::move(storage));
  state_->chunks.visit([&](const Int3 &,const Chunk &c){queue.push({c,0});},w);
  w.charge(state_->report.vertices,2*sizeof(Vec3));w.charge(state_->report.indices,sizeof(uint32_t));
  Mesh out;out.positions.reserve(state_->report.vertices);out.normals.reserve(state_->report.vertices);out.indices.reserve(state_->report.indices);
  std::map<Position,uint32_t> vertices;
  while(!queue.empty()) {
    w.tick();auto cursor=queue.top();queue.pop();const auto &mesh=cursor.chunk->mesh;
    for(size_t k=0;k<3;++k) {
      const auto v=mesh.indices.at(3*cursor.triangle+k);auto p=position(mesh.positions.at(v));
      auto found=vertices.find(p);
      if(found==vertices.end()) {
        if(out.positions.size()>=UINT32_MAX)throw std::length_error("Flattened sculpt vertex index overflow");
        w.charge(sizeof(std::pair<const Position,uint32_t>)+128);
        found=vertices.emplace(p,uint32_t(out.positions.size())).first;
        out.positions.push_back(mesh.positions.at(v));out.normals.push_back(mesh.normals.at(v));
      }
      out.indices.push_back(found->second);
    }
    if(++cursor.triangle<cursor.chunk->triangles.size())queue.push(std::move(cursor));
  }
  w.stop();return out;
}
} // namespace dome::detail
