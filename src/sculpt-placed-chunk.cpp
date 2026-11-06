// SPDX-License-Identifier: GPL-3.0-or-later
#include "sculpt-placed-chunk.hpp"
#include "affine-surface.hpp"
#include <bit>
namespace dome::detail {
namespace {
void stop(const LocalSurfaceCache::Cancel &fn) {
  if(fn&&fn())throw std::runtime_error("Placed sculpt chunk operation cancelled");
}
bool same(float a,float b) {return std::bit_cast<uint32_t>(a)==std::bit_cast<uint32_t>(b);}
bool same(CanvasTransform a,CanvasTransform b) {
  return same(a.position.x,b.position.x)&&same(a.position.y,b.position.y)&&same(a.position.z,b.position.z)&&
      same(a.orientation.x,b.orientation.x)&&same(a.orientation.y,b.orientation.y)&&
      same(a.orientation.z,b.orientation.z)&&same(a.orientation.w,b.orientation.w)&&same(a.scale,b.scale);
}
bool matrix_same(const std::array<double,16> &a,const std::array<double,16> &b) {
  return std::bit_cast<std::array<uint64_t,16>>(a)==std::bit_cast<std::array<uint64_t,16>>(b);
}
size_t mesh_bytes(const Mesh &m) {
  size_t bytes=sizeof(Mesh)+128;
  auto add=[&](size_t n,size_t width){
    if(n>(SIZE_MAX-bytes)/width)throw std::length_error("Placed sculpt mesh byte overflow");
    bytes+=n*width;
  };
  add(m.positions.capacity(),sizeof(Vec3));add(m.normals.capacity(),sizeof(Vec3));
  add(m.indices.capacity(),sizeof(uint32_t));return bytes;
}
}
bool SculptPlacedChunk::matches(const SculptObjectChunks &object,const SculptSurfaceChunks::Chunk &source) const {
  if(source_.get()!=source.get()||source_.owner_before(source)||source.owner_before(source_))return false;
  return placement_matches(object);
}
bool SculptPlacedChunk::placement_matches(const SculptObjectChunks &object) const {
  if(!affine_)return false;
  if(object.affine)return matrix_same(affine_->world().matrix(),object.affine->world().matrix())&&
      matrix_same(affine_->inverse().matrix(),object.affine->inverse().matrix())&&affine_->orientation()==object.affine->orientation();
  return same(world_,object.world)&&matrix_same(affine_->world().matrix(),affine_similarity(object.world).matrix());
}
SculptPlacedChunk::Ptr SculptPlacedChunk::capture(const SculptSceneChunks::Ptr &scene,uint64_t object,
    const SculptSurfaceChunks::Chunk &source,const Ptr &before,bool require_query,
    SculptPlacedChunkLimits limits,LocalSurfaceCache::Cancel cancel) {
  stop(cancel);
  if(!limits.retained_bytes||!limits.working_bytes)throw std::invalid_argument("Invalid placed sculpt chunk limits");
  const auto *view=scene?scene->find(object):nullptr;
  if(!view||!view->geometry->owns(source))throw std::invalid_argument("Placed chunk lost its actual scene/geometry owner");
  if(source->mesh.indices.empty())throw std::invalid_argument("An empty chunk cannot own placed query geometry");
  const bool reuse=before&&before->matches(*view,source);
  if(reuse&&(!require_query||before->query_)) {
    if(before->bytes_>limits.retained_bytes||before->bytes_>limits.working_bytes)
      throw std::length_error("Retained placed chunk exceeds requested limits");
    return before;
  }
  // Keep the original valid local float representation. Absolute world float
  // placement can collapse distinct vertices even under a simple translation.
  // GPU placement and binary64 affine query metrics consume the real frame.
  const bool same_source=before&&before->source_==source&&!before->source_.owner_before(source)&&!source.owner_before(before->source_);
  if(!same_source)source->mesh.validate();
  const bool reuse_query=require_query&&same_source&&before->query_;
  const auto query_bytes=require_query?SurfaceQuery::immutable_working_bytes(source->mesh):0;
  const auto own=sizeof(SculptPlacedChunk)+128;
  if(source->bytes>limits.retained_bytes||own>limits.retained_bytes-source->bytes||query_bytes>limits.retained_bytes-source->bytes-own)
    throw std::length_error("Placed sculpt chunk retained byte limit exceeded");
  const auto retained=source->bytes+own+query_bytes;
  const auto previous=before?before->bytes_:0;
  if(retained>limits.working_bytes||previous>limits.working_bytes-retained)
    throw std::length_error("Placed sculpt chunk working byte limit exceeded");
  auto out=std::shared_ptr<SculptPlacedChunk>(new SculptPlacedChunk);
  out->source_=source;out->geometry_=same_source?before->geometry_:std::shared_ptr<const Mesh>(source,&source->mesh);
  out->world_=view->world;out->affine_=view->affine?view->affine:std::optional<AffineInteractionFrame>(AffineInteractionFrame(affine_similarity(view->world)));
  out->reflected_=view->reflected();out->bytes_=retained;
  // A transformed local box is conservative for every actual source vertex.
  // Outward float neighbours retain coverage of double authored placement.
  for(size_t corner=0;corner<8;++corner){
    stop(cancel);const auto &box=source->bounds;
    const Vec3 local{corner&1?box.maximum.x:box.minimum.x,corner&2?box.maximum.y:box.minimum.y,corner&4?box.maximum.z:box.minimum.z};
    const auto p=out->affine_->world().point({local.x,local.y,local.z});
    const Vec3 lower{std::nextafter(float(p[0]),-INFINITY),std::nextafter(float(p[1]),-INFINITY),std::nextafter(float(p[2]),-INFINITY)};
    const Vec3 upper{std::nextafter(float(p[0]),INFINITY),std::nextafter(float(p[1]),INFINITY),std::nextafter(float(p[2]),INFINITY)};
    if(!finite(lower)||!finite(upper))throw std::overflow_error("Placed bounds exceed finite GPU coordinates");
    out->bounds_.include(lower);out->bounds_.include(upper);
  }
  // Preserve the existing query workspace guard on actual vertices. A loose
  // transformed box corner is not permission to reject a valid rotated shape.
  const auto &box=out->bounds_;
  if(require_query&&std::max({std::abs(box.minimum.x),std::abs(box.minimum.y),std::abs(box.minimum.z),std::abs(box.maximum.x),std::abs(box.maximum.y),std::abs(box.maximum.z)})>100)
    for(size_t v=0;v<source->mesh.positions.size();++v){
      if((v&255)==0)stop(cancel);const auto p=view->placed_position(source->mesh.positions[v]);
      if(std::max({std::abs(p.x),std::abs(p.y),std::abs(p.z)})>100)throw std::runtime_error("Mesh outside 100m authoring workspace");
    }
  if(reuse_query)out->query_=before->query_;
  else if(require_query)out->query_=std::shared_ptr<const SurfaceQuery>(new SurfaceQuery(SurfaceQuery::ImmutableGeometry{},out->geometry_,cancel));
  stop(cancel);return out;
}
} // namespace dome::detail
