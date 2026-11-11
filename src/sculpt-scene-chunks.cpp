// SPDX-License-Identifier: GPL-3.0-or-later
#include "sculpt-scene-chunks.hpp"
#include "sculpt-placed-chunks.hpp"
#include "sculpt-chunk-masks.hpp"
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
  return scene.surfaces_impl(false,true,cancel,mesher,true,&capture,true);
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
SculptScenePreview SculptSceneChunks::capture_geometry(const Scene &scene,SculptSceneChunkContext context,
    const Ptr &before,SculptSceneChunkLimits limits,LocalSurfaceCache::Cancel cancel) {
  stop(cancel);
  if(scene.objects().size()>256||scene.groups().size()>128)throw std::length_error("Sculpt scene object/group limit exceeded");
  // Object metadata has the existing fixed Scene quota. Chunk geometry itself
  // stays in persistent directories; no per-object vector of all chunks.
  size_t metadata=sizeof(SculptSceneChunks)+256+
      scene.objects().size()*(sizeof(SculptObjectChunks)+sizeof(Cache)+sizeof(std::pair<uint64_t,SculptSurfaceChunks::Ptr>)+sizeof(uint64_t));
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
  out->order_.reserve(scene.objects().size());
  for(const auto &object:scene.objects())out->order_.push_back(object.id);
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
    // Duplicated Volumes share their immutable COW geometry, even when their
    // object IDs, placement, material or masks differ. Share only the complete
    // authenticated local geometry owner; placed/query/scalar owners below
    // remain object-specific. Equal values or recycled raw addresses do not
    // authorize reuse.
    SculptSurfaceChunks::Ptr geometry;
    for(const auto &entry:out->caches_) {
      stop(cancel);
      const auto &source=*entry.geometry->source_owner();
      if(owner_same(source.geometry_owner(),o.volume->geometry_owner())&&
          scalar_same(source.voxel_size(),o.volume->voxel_size())&&
          scalar_same(source.band(),o.volume->band())) {
        geometry=entry.geometry;break;
      }
    }
    if(geometry) {
      const size_t shared=geometry->report().retained_bytes;
      if(shared>out->bytes_||out->bytes_>limits.working_bytes||
          prior>limits.working_bytes-out->bytes_||
          group_scratch>limits.working_bytes-out->bytes_-prior)
        throw std::length_error("Shared sculpt geometry lacks its actual scene reservation");
      auto shared_limits=limits.geometry;
      shared_limits.retained_bytes=std::min(shared_limits.retained_bytes,remaining+shared);
      shared_limits.working_bytes=std::min(shared_limits.working_bytes,
          limits.working_bytes-out->bytes_-prior-group_scratch+shared);
      shared_limits.extraction.bytes=std::min(shared_limits.extraction.bytes,shared_limits.retained_bytes);
      // Retain all ordinary per-operation limits, including reduced callers.
      // The exact owner/context above makes update return its original owner.
      geometry=geometry->update(*o.volume,shared_limits,cancel);
    } else {
    SculptSurfaceChunks::Ptr old;
    if(before&&before->context_.generation==context.generation)
      for(const auto &entry:before->caches_) {
        stop(cancel);
        const auto &source=*entry.geometry->source_owner();
        if(owner_same(source.geometry_owner(),o.volume->geometry_owner())&&
            scalar_same(source.voxel_size(),o.volume->voxel_size())&&
            scalar_same(source.band(),o.volume->band())) {old=entry.geometry;break;}
        if(entry.object==o.id)old=entry.geometry;
      }
    // The nested producer reserves its genuine previous owner. Keep all other
    // previous Scene allocations outside that scope, without reserving this
    // same immutable directory twice. Different COW roots receive no credit.
    const size_t old_geometry=old?old->report().retained_bytes:0;
    if(old_geometry>prior)throw std::logic_error("Previous sculpt geometry exceeds its actual Scene reservation");
    const auto prior_other=prior-old_geometry;
    if(prior_other>limits.working_bytes-out->bytes_)throw std::length_error("Sculpt scene working limit exceeded");
    auto object_limits=limits.geometry;
    object_limits.retained_bytes=std::min(object_limits.retained_bytes,remaining);
    if(group_scratch>limits.working_bytes-out->bytes_-prior_other)throw std::length_error("Sculpt group working limit exceeded");
    object_limits.working_bytes=std::min(object_limits.working_bytes,limits.working_bytes-out->bytes_-prior_other-group_scratch);
    object_limits.extraction.bytes=std::min(object_limits.extraction.bytes,object_limits.retained_bytes);
    geometry=old?old->update(*o.volume,object_limits,cancel):SculptSurfaceChunks::build(*o.volume,object_limits,cancel);
    // A reused owner's original limits never override this caller's reduced
    // aggregate cap. Its complete immutable footprint must still fit.
    if(geometry->report().retained_bytes>remaining)
      throw std::length_error("Sculpt scene retained geometry limit exceeded");
    out->bytes_+=geometry->report().retained_bytes;
    }
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
SculptScenePreview SculptSceneChunks::capture(const Scene &scene,SculptSceneChunkContext context,
    const Ptr &before,SculptSceneChunkLimits limits,LocalSurfaceCache::Cancel cancel) {
  auto preview=capture_geometry(scene,context,before,limits,cancel);
  const auto geometry=preview.chunks;
  // Geometry metadata is fully immutable before a placed/query factory reads
  // it. Build a separate final owner afterward; never mutate a published const
  // geometry view while asking another factory to authenticate that view.
  size_t clone=sizeof(SculptSceneChunks)+256+
      geometry->objects_.size()*(sizeof(SculptObjectChunks)+sizeof(SculptPlacedChunks::Ptr)+sizeof(SculptChunkMasks::Ptr))+
      geometry->caches_.size()*sizeof(Cache)+geometry->order_.size()*sizeof(uint64_t);
  for(const auto &object:geometry->objects_) {
    if(object.name.size()+object.material.name.size()+256>SIZE_MAX-clone)
      throw std::length_error("Sculpt view metadata copy overflow");
    clone+=object.name.size()+object.material.name.size()+256;
  }
  const size_t prior=before?before->bytes_:0;
  if(geometry->bytes_>limits.working_bytes||prior>limits.working_bytes-geometry->bytes_||
      clone>limits.working_bytes-geometry->bytes_-prior)
    throw std::length_error("Sculpt placed view working envelope exceeded");
  size_t retained=geometry->bytes_;
  std::vector<SculptPlacedChunks::Ptr> placed;
  std::vector<SculptChunkMasks::Ptr> masks; masks.reserve(geometry->objects_.size());
  if(geometry->objects_.size()>limits.working_bytes/sizeof(SculptPlacedChunks::Ptr))
    throw std::length_error("Sculpt placed owner vector byte limit exceeded");
  placed.reserve(geometry->objects_.size());
  for(const auto &object:geometry->objects_) {
    stop(cancel);
    if(retained>limits.retained_bytes||retained>limits.working_bytes-prior-clone)
      throw std::length_error("Sculpt placed view remaining budget exceeded");
    const auto *previous=before?before->find(object.object):nullptr;
    const auto previous_placed=previous?previous->placed:SculptPlacedChunks::Ptr{};
    const size_t current_geometry=object.geometry->report().retained_bytes;
    const size_t previous_scope=previous_placed?previous_placed->report().retained_bytes:0;
    if(current_geometry>retained||previous_scope>prior)
      throw std::logic_error("Placed scope lacks its actual Scene owner reservation");
    // Full standalone placed guards include their current geometry and prior
    // directory. The Scene holds those exact scopes already: its remaining
    // budget excludes only allocations outside this nested factory.
    const auto retained_other=retained-current_geometry;
    const auto prior_other=prior-previous_scope;
    if(retained_other>limits.retained_bytes||retained_other>limits.working_bytes||
        prior_other>limits.working_bytes-retained_other||clone>limits.working_bytes-retained_other-prior_other)
      throw std::length_error("Sculpt placed external owner envelope exceeded");
    SculptPlacedChunksLimits own;
    own.retained_bytes=std::min(own.retained_bytes,limits.retained_bytes-retained_other);
    own.working_bytes=std::min(own.working_bytes,limits.working_bytes-retained_other-prior_other-clone);
    if(!own.retained_bytes||!own.working_bytes)throw std::length_error("No sculpt placed capacity remains");
    auto result=SculptPlacedChunks::capture(geometry,object.object,previous_placed,context.queries,own,cancel);
    if(result->geometry()!=object.geometry||result->placement_bytes()>limits.retained_bytes-retained)
      throw std::length_error("Sculpt placed result exceeded its actual prospective envelope");
    // Geometry is already present exactly once in retained. The added layer
    // owns only placed wrappers/BVHs/index nodes, not a second copied Mesh.
    retained+=result->placement_bytes();placed.push_back(std::move(result));
    SculptChunkMasks::Ptr scalar;
    if(object.mask) {
      const auto previous_masks=previous?previous->masks:SculptChunkMasks::Ptr{};
      const size_t old_scalar=previous_masks?previous_masks->retained_bytes():0;
      if(old_scalar>prior)throw std::logic_error("Previous scalar scope exceeds its actual Scene reservation");
      if(retained>=limits.retained_bytes||retained>=limits.working_bytes-(prior-old_scalar)-clone)
        throw std::length_error("No sculpt scalar capacity remains");
      scalar=SculptChunkMasks::capture(object.geometry,object.mask,previous_masks,
          limits.retained_bytes-retained,limits.working_bytes-(prior-old_scalar)-clone-retained,cancel);
      if(scalar->retained_bytes()>limits.retained_bytes-retained)throw std::length_error("Sculpt scalar result exceeds aggregate budget");
      retained+=scalar->retained_bytes();
    }
    masks.push_back(std::move(scalar));
  }
  stop(cancel);
  auto final=std::shared_ptr<SculptSceneChunks>(new SculptSceneChunks(*geometry));
  for(size_t i=0;i<placed.size();++i){final->objects_[i].placed=std::move(placed[i]);final->objects_[i].masks=std::move(masks[i]);}
  final->bytes_=retained;preview.chunks=std::move(final);stop(cancel);return preview;
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
    if((c&255)==0)stop(cancel);
    std::swap(mesh.indices[c+1],mesh.indices[c+2]);
  }
  stop(cancel);return mesh;
}
} // namespace dome::detail
