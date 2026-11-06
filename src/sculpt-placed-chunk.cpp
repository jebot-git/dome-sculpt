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
  if(bool(affine_)!=bool(object.affine))return false;
  if(!affine_)return same(world_,object.world);
  return matrix_same(affine_->world().matrix(),object.affine->world().matrix())&&
      matrix_same(affine_->inverse().matrix(),object.affine->inverse().matrix())&&
      affine_->orientation()==object.affine->orientation();
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
  const auto query_bytes=require_query?SurfaceQuery::immutable_working_bytes(source->mesh):0;
  const auto own=sizeof(SculptPlacedChunk)+128;
  size_t retained=source->bytes;
  auto retain=[&](size_t n,size_t width=1){
    if(retained>limits.retained_bytes||n>(limits.retained_bytes-retained)/width)
      throw std::length_error("Placed sculpt chunk retained byte limit exceeded");
    retained+=n*width;
  };
  retain(own);
  if(reuse)retain(mesh_bytes(*before->geometry_));
  else {
    retain(sizeof(Mesh)+128);retain(source->mesh.positions.size(),2*sizeof(Vec3));
    retain(source->mesh.indices.size(),sizeof(uint32_t));
  }
  retain(query_bytes);
  size_t working=0;
  auto charge=[&](size_t n,size_t width=1){
    if(working>limits.working_bytes||n>(limits.working_bytes-working)/width)
      throw std::length_error("Placed sculpt chunk working byte limit exceeded");
    working+=n*width;
  };
  charge(source->bytes);if(before)charge(before->bytes_);
  charge(sizeof(SculptPlacedChunk)+128);
  charge(query_bytes);
  std::shared_ptr<const Mesh> geometry;
  SculptChunkBounds bounds;
  if(reuse) {geometry=before->geometry_;bounds=before->bounds_;}
  else {
    charge(sizeof(Mesh)+128);
    charge(source->mesh.positions.size(),2*sizeof(Vec3));charge(source->mesh.indices.size(),sizeof(uint32_t));
    Mesh mesh;mesh.positions.reserve(source->mesh.positions.size());mesh.normals.reserve(source->mesh.normals.size());
    mesh.indices=source->mesh.indices;
    for(size_t v=0;v<source->mesh.positions.size();++v) {
      if((v&255)==0)stop(cancel);
      const auto p=view->placed_position(source->mesh.positions[v]);
      mesh.positions.push_back(p);mesh.normals.push_back(view->placed_normal(source->mesh.normals.at(v)));bounds.include(p);
    }
    if(view->reflected())for(size_t c=0;c<mesh.indices.size();c+=3) {
      if((c&255)==0)stop(cancel);std::swap(mesh.indices[c+1],mesh.indices[c+2]);
    }
    mesh.validate();geometry=std::make_shared<const Mesh>(std::move(mesh));
  }
  // Capacity is deterministic for the qualified standard library, but still
  // verify the owned result rather than treating requested capacity as proof.
  const auto actual_mesh=mesh_bytes(*geometry);
  const auto expected_mesh=reuse?mesh_bytes(*before->geometry_):
      sizeof(Mesh)+128+source->mesh.positions.size()*2*sizeof(Vec3)+source->mesh.indices.size()*sizeof(uint32_t);
  if(actual_mesh>expected_mesh)throw std::length_error("Placed mesh capacity exceeded its prospective envelope");
  auto out=std::shared_ptr<SculptPlacedChunk>(new SculptPlacedChunk);
  out->source_=source;out->geometry_=std::move(geometry);out->bounds_=bounds;
  out->world_=view->world;out->affine_=view->affine;out->reflected_=view->reflected();out->bytes_=retained;
  if(out->affine_)out->tangent_=placed_tangent(*out->affine_,{1,0,0,1});
  else {
    const auto t=ob::rotate(out->world_.orientation,{1,0,0});out->tangent_={t.x,t.y,t.z,1};
  }
  if(require_query)out->query_=std::shared_ptr<const SurfaceQuery>(new SurfaceQuery(
      SurfaceQuery::ImmutableGeometry{},out->geometry_,cancel));
  stop(cancel);return out;
}
} // namespace dome::detail
