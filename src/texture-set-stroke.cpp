// SPDX-License-Identifier: GPL-3.0-or-later
#include "texture-set-stroke.hpp"
#include "texture-set-paint-access.hpp"
#include "document-authority.hpp"
#include "dome/source-placement.hpp"
#include <bit>
namespace dome::detail {
namespace {
template<class A,class B>bool owner(const std::shared_ptr<A>&a,const std::shared_ptr<B>&b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);}
bool scalar(float a,float b){return std::bit_cast<uint32_t>(a)==std::bit_cast<uint32_t>(b);}
bool transform(CanvasTransform a,CanvasTransform b){return scalar(a.position.x,b.position.x)&&scalar(a.position.y,b.position.y)&&scalar(a.position.z,b.position.z)&&scalar(a.orientation.w,b.orientation.w)&&scalar(a.orientation.x,b.orientation.x)&&scalar(a.orientation.y,b.orientation.y)&&scalar(a.orientation.z,b.orientation.z)&&scalar(a.scale,b.scale);}
bool material(const Material&a,const Material&b){
 if(a.name!=b.name||a.toon!=b.toon||a.double_sided!=b.double_sided||a.alpha!=b.alpha||a.unlit!=b.unlit||!scalar(a.alpha_cutoff,b.alpha_cutoff)||!scalar(a.metallic,b.metallic)||!scalar(a.roughness,b.roughness))return false;
 for(size_t k=0;k<4;++k)if(!scalar(a.color[k],b.color[k]))return false;for(size_t k=0;k<3;++k)if(!scalar(a.emissive[k],b.emissive[k]))return false;return true;
}
void stop(const TextureSetLimits&l){if(l.cancelled&&l.cancelled())throw std::runtime_error("Texture-set stroke admission cancelled");}
void charge(size_t&n,size_t count,size_t size,const TextureSetLimits&l){if(n>l.maximum_bytes||(size&&count>(l.maximum_bytes-n)/size))throw std::length_error("Texture-set stroke metadata exceeds its staging bound");n+=count*size;}
}
TextureSetStrokeLease::TextureSetStrokeLease(SculptObject before,std::vector<SceneGroup>groups)
 :before_(std::move(before)),groups_(std::move(groups)),set_(before_.texture_sets?before_.texture_sets->active():0),paint_(texture_set_view(before_,set_).painted){volume_geometry_=before_.volume->geometry_owner();volume_revision_=before_.volume->revision();volume_layer_=before_.volume->active_layer();voxel_=before_.volume->voxel_size();band_=before_.volume->band();before_.volume.reset();}
std::shared_ptr<const TextureSetStrokeLease> capture_texture_set_recipient(const Scene&scene,uint64_t object,const TextureSetLimits&limits){
 limits.validate();stop(limits);const auto&o=scene.object(object);
 if(scene.active().id!=object||!o.mesh_active||!o.painted||!o.visible||o.locked||o.painted->in_stroke())throw std::invalid_argument("Texture-set stroke requires its actual visible editable selected field");
 if(o.texture_sets)o.texture_sets->validate(o,false,limits);const auto field=active_texture_set(o);
 auto groups=scene.group_path(o.group);size_t bytes=sizeof(TextureSetStrokeLease);
 charge(bytes,2,TextureSetPaintAccess::snapshot_bytes(*o.painted),limits);charge(bytes,3,TextureSetPaintAccess::snapshot_bytes(*field.painted),limits);
 charge(bytes,groups.size(),sizeof(SceneGroup)+sizeof(HumanoidControls)+512,limits);
 auto before=o;before.painted=o.painted->snapshot();
 // Creator controls are intentionally mutable authoring metadata in Scene.
 // Capture their complete retained value rather than sharing that mutable alias.
 for(auto&g:groups)if(g.humanoid)g.humanoid=std::make_shared<const HumanoidControls>(*g.humanoid);
 stop(limits);std::shared_ptr<const TextureSetStrokeLease>lease(new const TextureSetStrokeLease(std::move(before),std::move(groups)));lease->self_=lease;return lease;
}
std::shared_ptr<const TextureSetStrokeLease> capture_texture_set_stroke(const Scene&scene,uint64_t object,const TextureSetLimits&limits){
 if(!scene.object(object).texture_sets)throw std::invalid_argument("A selected texture-set stroke needs its actual membership collection");
 return capture_texture_set_recipient(scene,object,limits);
}
void TextureSetStrokeLease::authenticate_owner(const std::shared_ptr<const TextureSetStrokeLease>&lease)const{
 const auto expected=self_.lock();if(!expected||!owner(expected,lease))throw std::invalid_argument("Texture-set stroke borrowed a foreign recipient lease");
}
void TextureSetStrokeLease::authenticate_scene(const Scene&scene)const{
 if(scene.active().id!=before_.id)throw std::invalid_argument("The selected texture-set object changed");const auto&o=scene.object(before_.id);
 if(!owner(o.texture_sets,before_.texture_sets)||(o.texture_sets?o.texture_sets->active():0)!=set_||o.name!=before_.name||o.id!=before_.id||o.group!=before_.group||o.visible!=before_.visible||o.locked!=before_.locked||o.mesh_active!=before_.mesh_active||
    !transform(o.transform,before_.transform)||!material(o.material,before_.material)||!owner(o.deformation,before_.deformation)||!owner(o.appearance,before_.appearance)||!owner(o.material_bindings,before_.material_bindings)||!owner(o.retopology,before_.retopology)||!owner(o.polygon,before_.polygon)||!owner(o.affine_edit,before_.affine_edit)||!owner(o.shape_source,before_.shape_source)||!owner(o.rig_source,before_.rig_source)||!owner(o.bake_recipe,before_.bake_recipe)||
    !o.volume||!owner(o.volume->geometry_owner(),volume_geometry_)||o.volume->revision()!=volume_revision_||o.volume->active_layer()!=volume_layer_||!scalar(o.volume->voxel_size(),voxel_)||!scalar(o.volume->band(),band_)||
    !o.painted||!TextureSetPaintAccess::same_current(*o.painted,*before_.painted))throw std::invalid_argument("Texture-set stroke recipient fields/source/geometry/pose changed");
 const auto path=scene.group_path(o.group);if(path.size()!=groups_.size())throw std::invalid_argument("Texture-set stroke placement path changed");
 for(size_t i=0;i<path.size();++i){const auto&a=path[i],&b=groups_[i];if(a.id!=b.id||a.parent!=b.parent||a.name!=b.name||!transform(a.transform,b.transform)||!owner(a.affine_edit,b.affine_edit)||!owner(a.source_placement,b.source_placement)||!same_humanoid_controls(a.humanoid,b.humanoid))throw std::invalid_argument("Texture-set stroke source/placement/group metadata changed");}
}
void adopt_texture_set_stroke(Scene&scene,const std::shared_ptr<const TextureSetStrokeLease>&lease,const PaintedMesh&draft,const TextureSetLimits&limits){
 limits.validate();stop(limits);if(!lease)throw std::invalid_argument("Texture-set stroke has no current recipient lease");lease->authenticate_owner(lease);lease->authenticate_scene(scene);
 if(!TextureSetPaintAccess::stroke_structure(*lease->paint(),draft))throw std::invalid_argument("Texture-set stroke draft changed its geometry/source/material/layer structure");
 auto sealed=draft.snapshot();
 // edit_texture_set constructs another detached recipient and seals its field.
 // No cached Scene, borrowed mutable collection or live target is overwritten.
 edit_texture_set(scene,lease->object(),lease->set(),[&](PaintedMesh&p,Material&){TextureSetPaintAccess::swap_stroke(p,*sealed);return true;},limits);
}
} // namespace dome::detail
