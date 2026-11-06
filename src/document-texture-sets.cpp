// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "texture-set-stroke.hpp"
#include "texture-set-surface.hpp"
#include "document-authority.hpp"
namespace dome {
void Document::edit_texture_set(TextureSetEdit edit,AssetSource expected){
 if(edit.action<TextureSetAction::Create||edit.action>TextureSetAction::Material||edit.faces.size()>2'000'000||edit.name.size()>256||
    (edit.resolution&&(edit.resolution<64||edit.resolution>2048||(edit.resolution&(edit.resolution-1)))))throw std::invalid_argument("Invalid texture-set request");
 if(edit.action==TextureSetAction::Material)edit.material.validate();
 std::lock_guard lock(model_mutex_);require_authority_locked(expected);
 if((expected.generation&&expected.generation!=state_.generation)||(expected.revision&&expected.revision!=state_.revision)||(expected.object&&expected.object!=scene_.active().id))throw std::logic_error("The texture-set source changed; reopen its controls");
 finish_locked();const auto object=scene_.active().id;require_object_editable_locked(object);
 if(!detail::texture_set_plain_fields(scene_.active()))throw std::logic_error("Texture sets are not yet available for this prepared material");
 auto lease=detail::capture_texture_set_recipient(scene_,object);auto changed=std::make_shared<bool>(false);
 const auto source=AssetSource{state_.generation,state_.revision,object,state_.edit_authority};
 const char*label=edit.action==TextureSetAction::Create?"Create texture set":edit.action==TextureSetAction::AssignFaces?"Assign material faces":edit.action==TextureSetAction::Select?"Select texture set":edit.action==TextureSetAction::Rename?"Rename texture set":"Edit texture-set material";
 queue_scene_operation_locked(label,[object,edit=std::move(edit),changed](Scene&scene,const auto&cancelled){
  TextureSetLimits limits;limits.cancelled=cancelled;
  switch(edit.action){
  case TextureSetAction::Create:dome::create_texture_set(scene,object,edit.faces,edit.name,edit.resolution,limits);*changed=true;break;
  case TextureSetAction::AssignFaces:*changed=assign_texture_set_faces(scene,object,edit.set,edit.faces,limits);break;
  case TextureSetAction::Select:if(cancelled())throw std::runtime_error("Texture-set selection cancelled");*changed=select_texture_set(scene,object,edit.set);break;
  case TextureSetAction::Rename:*changed=rename_texture_set(scene,object,edit.set,edit.name,limits);break;
  case TextureSetAction::Material:*changed=dome::edit_texture_set(scene,object,edit.set,[&](PaintedMesh&,Material&m){if(detail::same_material(m,edit.material))return false;m=edit.material;return true;},limits);break;
  }
 },source,{}, {},changed,{},0,0,{},{},0,{},std::move(lease));
}
} // namespace dome
