// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "dome/humanoid-creator.hpp"
#include <bit>
#include <tuple>
namespace dome::detail {
inline bool same_transform(CanvasTransform a,CanvasTransform b){return a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z&&a.orientation.w==b.orientation.w&&a.orientation.x==b.orientation.x&&a.orientation.y==b.orientation.y&&a.orientation.z==b.orientation.z&&a.scale==b.scale;}
inline bool same_material(const Material&a,const Material&b){return std::tie(a.name,a.color,a.metallic,a.roughness,a.emissive,a.toon,a.double_sided,a.alpha,a.alpha_cutoff,a.unlit)==std::tie(b.name,b.color,b.metallic,b.roughness,b.emissive,b.toon,b.double_sided,b.alpha,b.alpha_cutoff,b.unlit);}
// Scene::snapshot detaches creator controls. Compare their complete retained
// state, not the address of that intentionally independent copy. Keep exact
// binary32 identity; no padding reads or approximate geometry/style equality.
inline bool same_humanoid_controls(const std::shared_ptr<const HumanoidControls>&a,
                                   const std::shared_ptr<const HumanoidControls>&b){
 if(bool(a)!=bool(b))return false;
 if(!a)return true;
 if(a->baseline!=b->baseline)return false;
 auto scalars=[](const HumanoidControls&h){return std::array{
   h.height,h.anime,h.build,h.shoulder_width,h.waist_width,h.leg_length,
   h.arm_length,h.head_width,h.head_height,h.eye_size,h.eye_spacing,
   h.nose_size,h.mouth_width,h.jaw_width,h.hair_length,h.hair_volume,h.fringe};};
 const auto x=scalars(*a),y=scalars(*b);
 for(size_t i=0;i<x.size();++i)
   if(std::bit_cast<uint32_t>(x[i])!=std::bit_cast<uint32_t>(y[i]))return false;
 const std::array colors_a{a->skin_color,a->iris_color,a->hair_color};
 const std::array colors_b{b->skin_color,b->iris_color,b->hair_color};
 for(size_t i=0;i<colors_a.size();++i)for(size_t c=0;c<4;++c)
   if(std::bit_cast<uint32_t>(colors_a[i][c])!=std::bit_cast<uint32_t>(colors_b[i][c]))return false;
 return true;
}
inline bool same_object_authority(const SculptObject &a,const SculptObject &b){
 return !(a.name!=b.name||a.group!=b.group||a.visible!=b.visible||a.locked!=b.locked||a.mesh_active!=b.mesh_active||!same_transform(a.transform,b.transform)||!same_material(a.material,b.material)||a.deformation!=b.deformation||a.appearance!=b.appearance||a.material_bindings!=b.material_bindings||a.retopology!=b.retopology||a.polygon!=b.polygon||a.shape_source!=b.shape_source||a.rig_source!=b.rig_source||a.affine_edit!=b.affine_edit||a.volume->geometry_owner()!=b.volume->geometry_owner()||a.volume->revision()!=b.volume->revision()||a.volume->voxel_size()!=b.volume->voxel_size()||a.volume->band()!=b.volume->band()||bool(a.painted)!=bool(b.painted)||(a.painted&&(a.painted->mesh_owner()!=b.painted->mesh_owner()||a.painted->revision()!=b.painted->revision())));
}
// Immutable/COW authoring owners plus monotonic authoring revisions defend
// protected objects without serializing/hashing every geometry or texture byte.
inline void require_isolated_scene(const Scene &before,const Scene &after,uint64_t target){
 if(before.objects().size()!=after.objects().size()||before.groups().size()!=after.groups().size())throw std::logic_error("Held-object isolation forbids changing other objects or groups");
 for(size_t i=0;i<before.groups().size();++i){const auto&a=before.groups()[i];const auto&b=after.groups()[i];if(a.id!=b.id||a.parent!=b.parent||a.name!=b.name||!same_humanoid_controls(a.humanoid,b.humanoid)||a.source_placement!=b.source_placement||a.affine_edit!=b.affine_edit||!same_transform(a.transform,b.transform))throw std::logic_error("Held-object isolation preserves all group authority");}
 for(size_t i=0;i<before.objects().size();++i){const auto&a=before.objects()[i];const auto&b=after.objects()[i];if(a.id!=b.id)throw std::logic_error("Held-object isolation preserves object identities");if(a.id==target)continue;
  if(!same_object_authority(a,b))throw std::logic_error("Asset work attempted to edit a read-only ghost object");
 }
}
} // namespace dome::detail
