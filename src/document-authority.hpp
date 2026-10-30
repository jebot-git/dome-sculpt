// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include <tuple>
namespace dome::detail {
inline bool same_transform(CanvasTransform a,CanvasTransform b){return a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z&&a.orientation.w==b.orientation.w&&a.orientation.x==b.orientation.x&&a.orientation.y==b.orientation.y&&a.orientation.z==b.orientation.z&&a.scale==b.scale;}
inline bool same_material(const Material&a,const Material&b){return std::tie(a.name,a.color,a.metallic,a.roughness,a.emissive,a.toon,a.double_sided,a.alpha,a.alpha_cutoff,a.unlit)==std::tie(b.name,b.color,b.metallic,b.roughness,b.emissive,b.toon,b.double_sided,b.alpha,b.alpha_cutoff,b.unlit);}
// Immutable/COW authoring owners plus monotonic authoring revisions defend
// protected objects without serializing/hashing every geometry or texture byte.
inline void require_isolated_scene(const Scene &before,const Scene &after,uint64_t target){
 if(before.objects().size()!=after.objects().size()||before.groups().size()!=after.groups().size())throw std::logic_error("Held-object isolation forbids changing other objects or groups");
 for(size_t i=0;i<before.groups().size();++i){const auto&a=before.groups()[i];const auto&b=after.groups()[i];if(a.id!=b.id||a.parent!=b.parent||a.name!=b.name||a.humanoid!=b.humanoid||!same_transform(a.transform,b.transform))throw std::logic_error("Held-object isolation preserves all group authority");}
 for(size_t i=0;i<before.objects().size();++i){const auto&a=before.objects()[i];const auto&b=after.objects()[i];if(a.id!=b.id)throw std::logic_error("Held-object isolation preserves object identities");if(a.id==target)continue;
  if(a.name!=b.name||a.group!=b.group||a.visible!=b.visible||a.locked!=b.locked||a.mesh_active!=b.mesh_active||!same_transform(a.transform,b.transform)||!same_material(a.material,b.material)||a.deformation!=b.deformation||a.appearance!=b.appearance||a.material_bindings!=b.material_bindings||a.retopology!=b.retopology||a.volume->geometry_owner()!=b.volume->geometry_owner()||a.volume->revision()!=b.volume->revision()||a.volume->voxel_size()!=b.volume->voxel_size()||a.volume->band()!=b.volume->band()||bool(a.painted)!=bool(b.painted)||(a.painted&&(a.painted->mesh_owner()!=b.painted->mesh_owner()||a.painted->revision()!=b.painted->revision())))throw std::logic_error("Asset work attempted to edit a read-only ghost object");
 }
}
} // namespace dome::detail
