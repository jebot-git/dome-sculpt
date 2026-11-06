// SPDX-License-Identifier: GPL-3.0-or-later
#include "sculpt-scene-chunks.hpp"
#include "affine-surface.hpp"
#include "dome/polygon-scene.hpp"
#include <bit>

namespace dome::detail {
namespace {
void stop(const LocalSurfaceCache::Cancel &cancel) {
  if(cancel&&cancel())throw std::runtime_error("Sculpt scene chunk operation cancelled");
}
bool owner_same(const Volume::GeometryOwner &a,const Volume::GeometryOwner &b) {
  return a.get()==b.get()&&!a.owner_before(b)&&!b.owner_before(a);
}
bool scalar_same(float a,float b) {return std::bit_cast<uint32_t>(a)==std::bit_cast<uint32_t>(b);}
Vec3 vector(ob::Vec3 p) {return {p.x,p.y,p.z};}
ob::Vec3 obvector(Vec3 p) {return {p.x,p.y,p.z};}
}
SculptMaskSnapshot::SculptMaskSnapshot(const Volume &v)
    :values_(v.mask_values().owner()),background_(v.mask_default()),voxel_(v.voxel_size()) {}
float SculptMaskSnapshot::sample(Vec3 p) const {
  if(!finite(p)||std::max({std::abs(p.x),std::abs(p.y),std::abs(p.z)})>1001)
    throw std::invalid_argument("Invalid sculpt mask sample position");
  const auto values=Volume::VoxelView(values_);
  if(values.size()==0)return background_;
  auto at=[&](Int3 q){auto i=values.find(q);return i==values.end()?background_:i->second;};
  p=p/voxel_;
  Int3 q{int(std::floor(p.x)),int(std::floor(p.y)),int(std::floor(p.z))};
  const auto f=p-Vec3{float(q.x),float(q.y),float(q.z)};
  auto mix=[](float a,float b,float t){return a+(b-a)*t;};
  float planes[2];
  for(int z=0;z<2;++z) {
    float rows[2];
    for(int y=0;y<2;++y)rows[y]=mix(at(q+Int3{0,y,z}),at(q+Int3{1,y,z}),f.x);
    planes[z]=mix(rows[0],rows[1],f.y);
  }
  return std::clamp(mix(planes[0],planes[1],f.z),0.f,1.f);
}
bool SculptMaskSnapshot::same(const SculptMaskSnapshot &b) const {
  return owner_same(values_,b.values_)&&scalar_same(background_,b.background_)&&scalar_same(voxel_,b.voxel_);
}
size_t SculptMaskSnapshot::retained_bytes() const {return sizeof(SculptMaskSnapshot)+128+values_->retained_bytes();}
Vec3 SculptObjectChunks::placed_position(Vec3 p) const {
  return affine?detail::placed_position(*affine,p):vector(ob::transform(world,obvector(p)));
}
Vec3 SculptObjectChunks::placed_normal(Vec3 p) const {
  return affine?detail::placed_normal(*affine,p):vector(ob::rotate(world.orientation,obvector(p)));
}
bool SculptObjectChunks::reflected() const {return affine&&affine->orientation()==AffineOrientation::Reversing;}
std::vector<ObjectSurface> SculptScenePreviewAccess::surfaces(const Scene &scene,
    const LocalSurfaceCache::Cancel &cancel,const Scene::SculptMesher &mesher,
    const std::function<bool(const SculptObject &)> &capture) {
  return scene.surfaces_impl(false,true,cancel,mesher,&capture);
}
const SculptObjectChunks *SculptSceneChunks::find(uint64_t object) const {
  auto i=std::find_if(objects_.begin(),objects_.end(),[&](const auto &o){return o.object==object;});
  return i==objects_.end()?nullptr:&*i;
}
bool SculptSceneChunks::captures(uint64_t object) const {
  for(const auto &cache:caches_)if(cache.object==object)
    return cache.geometry->representation()==SculptSurfaceChunks::Representation::Chunks;
  return false;
}
SculptScenePreview SculptSceneChunks::capture(const Scene &scene,SculptSceneChunkContext context,
    const Ptr &before,SculptSceneChunkLimits limits,LocalSurfaceCache::Cancel cancel) {
  stop(cancel);
  if(scene.objects().size()>256||scene.groups().size()>128)throw std::length_error("Sculpt scene object/group limit exceeded");
  // Object metadata has the existing fixed Scene quota. Chunk geometry itself
  // stays in persistent directories; no per-object vector of all chunks.
  size_t metadata=sizeof(SculptSceneChunks)+256+
      scene.objects().size()*(sizeof(SculptObjectChunks)+sizeof(Cache)+sizeof(std::pair<uint64_t,SculptSurfaceChunks::Ptr>));
  auto add=[&](size_t n){
    if(metadata>limits.retained_bytes||n>limits.retained_bytes-metadata)
      throw std::length_error("Sculpt scene retained metadata limit exceeded");
    metadata+=n;
  };
  for(const auto &object:scene.objects()) {
    add(object.name.size()+128);add(object.material.name.size()+128);
    if(context.mask_view&&!object.mesh_active&&object.id==scene.active().id)
      add(sizeof(SculptMaskSnapshot)+128+object.volume->mask_values().owner()->retained_bytes());
  }
  size_t group_scratch=scene.groups().size()*sizeof(SceneGroup);
  for(const auto &group:scene.groups()) {
    if(group.name.size()+128>SIZE_MAX-group_scratch)throw std::length_error("Sculpt group scratch overflow");
    group_scratch+=group.name.size()+128;
  }
  if(metadata>limits.working_bytes||metadata>limits.retained_bytes)
    throw std::length_error("Sculpt scene metadata limit exceeded");
  auto out=std::shared_ptr<SculptSceneChunks>(new SculptSceneChunks);out->context_=context;out->limits_=limits;
  out->objects_.reserve(scene.objects().size());out->caches_.reserve(scene.objects().size());
  out->bytes_=metadata;
  const size_t prior=before?before->bytes_:0;
  if(prior>limits.working_bytes-metadata||group_scratch>limits.working_bytes-metadata-prior)
    throw std::length_error("Sculpt scene retained working limit exceeded");
  std::vector<std::pair<uint64_t,SculptSurfaceChunks::Ptr>> legacy_repairs;
  legacy_repairs.reserve(scene.objects().size());
  auto capture_volume=[&](const SculptObject &o) {
    stop(cancel);
    // Polygon39 and all painted current/source/normal domains retain their
    // exact original adapter. Dormant source metadata on an ordinary Volume
    // is preserved in Scene; it is not relabelled as current generated UVs.
    if(o.mesh_active||(o.polygon&&o.polygon->active))return false;
    o.material.validate();
    const auto remaining=limits.retained_bytes-out->bytes_;
    if(prior>limits.working_bytes-out->bytes_)throw std::length_error("Sculpt scene working limit exceeded");
    auto object_limits=limits.geometry;
    object_limits.retained_bytes=std::min(object_limits.retained_bytes,remaining);
    if(group_scratch>limits.working_bytes-out->bytes_-prior)throw std::length_error("Sculpt group working limit exceeded");
    object_limits.working_bytes=std::min(object_limits.working_bytes,limits.working_bytes-out->bytes_-prior-group_scratch);
    object_limits.extraction.bytes=std::min(object_limits.extraction.bytes,object_limits.retained_bytes);
    SculptSurfaceChunks::Ptr old;
    if(before&&before->context_.generation==context.generation)
      for(const auto &entry:before->caches_)if(entry.object==o.id){old=entry.geometry;break;}
    auto geometry=old?old->update(*o.volume,object_limits,cancel):SculptSurfaceChunks::build(*o.volume,object_limits,cancel);
    // A reused owner's original limits never override this caller's reduced
    // aggregate cap. Its complete immutable footprint must still fit.
    if(geometry->report().retained_bytes>remaining)
      throw std::length_error("Sculpt scene retained geometry limit exceeded");
    out->bytes_+=geometry->report().retained_bytes;
    out->caches_.push_back({o.id,geometry});
    if(geometry->representation()!=SculptSurfaceChunks::Representation::Chunks) {
      legacy_repairs.push_back({o.id,geometry});return false;
    }
    if(geometry->report().indices==0)return true;
    SculptObjectChunks view;view.object=o.id;view.name=o.name;view.material=o.material;
    view.local=o.transform;const auto groups=scene.group_path(o.group);
    view.geometry=std::move(geometry);view.source_dormant=source_is_dormant(o);
    const bool affine=bool(o.affine_edit)||std::any_of(groups.begin(),groups.end(),[](const auto &g){return g.source_placement||g.affine_edit;});
    if(affine)view.affine=placement_frame(groups,view.local,o.affine_edit);
    else view.world=scene.world_transform(o.id);
    if(context.mask_view&&o.id==scene.active().id) {
      auto mask=std::make_shared<const SculptMaskSnapshot>(*o.volume);
      if(before)if(const auto *previous=before->find(o.id);previous&&previous->mask&&previous->mask->same(*mask))mask=previous->mask;
      view.mask=std::move(mask);
    }
    out->objects_.push_back(std::move(view));return true;
  };
  auto legacy_mesher=[&](uint64_t id,const Volume &volume) {
    stop(cancel);
    for(const auto &[object,geometry]:legacy_repairs)if(object==id) {
      const auto other=out->bytes_-geometry->report().retained_bytes;
      if(other>limits.working_bytes)throw std::length_error("Legacy sculpt scene working limit exceeded");
      return geometry->materialize(limits.working_bytes-other,cancel);
    }
    return surface_mesh(volume,cancel);
  };
  auto legacy=SculptScenePreviewAccess::surfaces(scene,cancel,legacy_mesher,capture_volume);
  stop(cancel);return {std::move(legacy),std::move(out)};
}
Mesh SculptSceneChunks::materialize(uint64_t object,LocalSurfaceCache::Cancel cancel) const {
  const auto *view=find(object);
  if(!view)throw std::invalid_argument("Selected object is not in the actual sculpt chunk view");
  const auto other=bytes_-view->geometry->report().retained_bytes;
  if(other>limits_.working_bytes)throw std::length_error("Selected sculpt flatten working limit exceeded");
  auto mesh=view->geometry->materialize(limits_.working_bytes-other,cancel);
  for(size_t v=0;v<mesh.positions.size();++v) {
    if((v&255)==0)stop(cancel);
    mesh.positions[v]=view->placed_position(mesh.positions[v]);
    mesh.normals[v]=view->placed_normal(mesh.normals[v]);
  }
  if(view->reflected())for(size_t c=0;c<mesh.indices.size();c+=3) {
    if((c&255)==0)stop(cancel);std::swap(mesh.indices[c+1],mesh.indices[c+2]);
  }
  stop(cancel);return mesh;
}
} // namespace dome::detail
