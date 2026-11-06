// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "texture-set-stroke.hpp"
#include "texture-set-controls.hpp"
#include "texture-set-surface.hpp"
#include "document-authority.hpp"
#include "texture-set-range.hpp"
#include "dome/material-visibility.hpp"
#include <algorithm>
#include <cmath>
namespace dome {
void Document::edit_texture_set(TextureSetEdit edit,AssetSource expected){
 std::lock_guard lock(model_mutex_);
 queue_texture_set_locked(std::move(edit),expected);
}
void Document::queue_texture_set_locked(TextureSetEdit edit,AssetSource expected){
 if(edit.action<TextureSetAction::Create||edit.action>TextureSetAction::ToggleLayer||edit.faces.size()>2'000'000||edit.name.size()>256||!std::isfinite(edit.value)||
    (edit.resolution&&(edit.resolution<64||edit.resolution>2048||(edit.resolution&(edit.resolution-1)))))throw std::invalid_argument("Invalid texture-set request");
 if(edit.action==TextureSetAction::Material)edit.material.validate();
 require_authority_locked(expected);
 if((expected.generation&&expected.generation!=state_.generation)||(expected.revision&&expected.revision!=state_.revision)||(expected.object&&expected.object!=scene_.active().id))throw std::logic_error("The texture-set source changed; reopen its controls");
 finish_locked();require_authority_locked(expected);
 if((expected.generation&&expected.generation!=state_.generation)||(expected.revision&&expected.revision!=state_.revision)||(expected.object&&expected.object!=scene_.active().id))throw std::logic_error("The texture-set recipient changed while finishing its earlier gesture");
 const auto object=scene_.active().id;require_object_editable_locked(object);
 if(!detail::texture_set_plain_fields(scene_.active()))throw std::logic_error("Texture sets are not yet available for this prepared material");
 auto lease=detail::capture_texture_set_recipient(scene_,object);auto changed=std::make_shared<bool>(false);
 const auto source=AssetSource{state_.generation,state_.revision,object,state_.edit_authority};
 const char*label=edit.action==TextureSetAction::Create?"Create texture set":edit.action==TextureSetAction::AssignFaces?"Assign material faces":edit.action==TextureSetAction::Select?"Select texture set":edit.action==TextureSetAction::Rename?"Rename texture set":edit.action==TextureSetAction::Material?"Edit texture-set material":"Edit texture-set layer";
 queue_scene_operation_locked(label,[object,edit=std::move(edit),changed](Scene&scene,const auto&cancelled){
  TextureSetLimits limits;limits.cancelled=cancelled;
  switch(edit.action){
  case TextureSetAction::Create:dome::create_texture_set(scene,object,edit.faces,edit.name,edit.resolution,limits);*changed=true;break;
  case TextureSetAction::AssignFaces:*changed=assign_texture_set_faces(scene,object,edit.set,edit.faces,limits);break;
  case TextureSetAction::Select:if(cancelled())throw std::runtime_error("Texture-set selection cancelled");*changed=select_texture_set(scene,object,edit.set);break;
  case TextureSetAction::Rename:*changed=rename_texture_set(scene,object,edit.set,edit.name,limits);break;
  case TextureSetAction::AddLayer:case TextureSetAction::DeleteLayer:case TextureSetAction::NextLayer:case TextureSetAction::ToggleLayer:
   *changed=dome::edit_texture_set(scene,object,edit.set,[&](PaintedMesh&p,Material&){
    if(p.active_layer()!=edit.layer)throw std::invalid_argument("The selected texture-set layer changed");
    const auto&layers=p.layers();const auto at=std::find_if(layers.begin(),layers.end(),[&](const auto&l){return l.id==edit.layer;});
    if(at==layers.end())throw std::invalid_argument("Unknown selected texture-set layer");
    if(edit.action==TextureSetAction::AddLayer)p.add_layer("Paint "+std::to_string(layers.size()+1));
    else if(edit.action==TextureSetAction::DeleteLayer)p.delete_layer();
    else if(edit.action==TextureSetAction::NextLayer){if(layers.size()<2)return false;p.select_layer(layers[(size_t(at-layers.begin())+1)%layers.size()].id);}
    else p.layer_properties(at->name,!at->visible,at->opacity);return true;
   },limits);break;
  case TextureSetAction::LayerOpacity:
  case TextureSetAction::LayerOrder:
   *changed=dome::edit_texture_set(scene,object,edit.set,[&](PaintedMesh&p,Material&){
    if(p.active_layer()!=edit.layer)throw std::invalid_argument("The selected texture-set layer changed");
    const auto&layers=p.layers();const auto at=std::find_if(layers.begin(),layers.end(),[&](const auto&l){return l.id==edit.layer;});
    if(at==layers.end())throw std::invalid_argument("Unknown selected texture-set layer");
    if(edit.action==TextureSetAction::LayerOpacity){if(edit.value<0||edit.value>1)throw std::invalid_argument("Invalid texture-set layer opacity");if(at->opacity==edit.value)return false;p.layer_properties(at->name,at->visible,edit.value);}
    else{const int target=int(std::lround(edit.value));if(target<0||size_t(target)>=layers.size())throw std::invalid_argument("Invalid texture-set layer order");int index=int(at-layers.begin());if(index==target)return false;while(index!=target){const int direction=target>index?1:-1;p.move_layer(direction);index+=direction;}}
    return true;
   },limits);break;
  case TextureSetAction::Material:*changed=dome::edit_texture_set(scene,object,edit.set,[&](PaintedMesh&,Material&m){if(detail::same_texture_material(m,edit.material))return false;m=edit.material;return true;},limits);break;
  }
 },source,{}, {},changed,{},0,0,{},{},0,{},std::move(lease));
}

bool Document::texture_set_command_enabled_locked(SpatialCommand command)const{
 if(!texture_set_command(command)||state_.asset_busy||state_.recovery_busy||control_before_||action_active_locked()||state_.polygon||state_.retopology||state_.cut||uv_||!object_editable_locked(scene_.active().id)||!detail::texture_set_plain_fields(scene_.active()))return false;
 const auto&o=scene_.active();if(o.polygon&&!o.texture_sets)return false;
 if(command==SpatialCommand::TexturePrevious||command==SpatialCommand::TextureNext)return o.texture_sets&&o.texture_sets->additional().size();
 if(command==SpatialCommand::TextureCreate)return !state_.texture_faces.empty()&&(!o.texture_sets||o.texture_sets->additional().size()<63);
 if(command==SpatialCommand::TextureAssign)return o.texture_sets&&!state_.texture_faces.empty();
 if(command==SpatialCommand::TextureClearFaces)return !state_.texture_faces.empty();return true;
}
void Document::texture_set_action(SpatialCommand command){
 std::lock_guard lock(model_mutex_);finish_locked();if(!texture_set_command_enabled_locked(command))throw std::logic_error("Texture-set action is unavailable");
 texture_set_command_locked(command);publish_locked(false,false);
}
void Document::texture_set_command_locked(SpatialCommand command){
 if(!texture_set_command_enabled_locked(command))return;const auto&o=scene_.active();
 if(command==SpatialCommand::TexturePickFaces){state_.texture_face_select=!state_.texture_face_select;texture_face_geometry_=o.painted->mesh_owner();texture_face_object_=o.id;texture_face_pressed_={true,true};input_.cancel();state_.selection_mode=false;state_.object_grab_mode=false;return;}
 if(command==SpatialCommand::TextureClearFaces){state_.texture_faces.clear();return;}
 TextureSetEdit edit;
 if(command==SpatialCommand::TexturePrevious||command==SpatialCommand::TextureNext){
  std::vector<TextureSetId>ids{0};for(const auto&s:o.texture_sets->additional())ids.push_back(s.id);const auto at=std::find(ids.begin(),ids.end(),o.texture_sets->active());
  const size_t index=size_t(at-ids.begin()),count=ids.size();edit.action=TextureSetAction::Select;edit.set=ids[(index+count+(command==SpatialCommand::TextureNext?1:-1))%count];
 }else{if(texture_face_geometry_!=o.painted->mesh_owner()||texture_face_geometry_.owner_before(o.painted->mesh_owner())||o.painted->mesh_owner().owner_before(texture_face_geometry_))throw std::logic_error("Texture face selection belongs to an earlier geometry");
  edit.faces=state_.texture_faces;edit.set=o.texture_sets?o.texture_sets->active():0;
  edit.action=command==SpatialCommand::TextureCreate?TextureSetAction::Create:TextureSetAction::AssignFaces;
  edit.name="Material "+std::to_string(o.texture_sets?o.texture_sets->next_id():1);
 }
 queue_texture_set_locked(std::move(edit));
}
bool Document::texture_face_pointer(Vec3 origin,Vec3 direction,bool additive,double seconds){
 std::lock_guard lock(model_mutex_);if(!state_.texture_face_select)throw std::logic_error("Enable texture face selection first");
 const bool found=texture_face_pointer_locked(origin,direction,additive,seconds);publish_locked(false,false);return found;
}
bool Document::texture_face_pointer_locked(Vec3 origin,Vec3 direction,bool additive,double seconds){
 if(!std::isfinite(seconds)||!finite(origin)||!finite(direction)||length(direction)<1e-8f)throw std::invalid_argument("Invalid material-visible face ray");
 const auto&o=scene_.active();if(!texture_set_command_enabled_locked(SpatialCommand::TexturePickFaces))return false;
 const auto snapshot=mesh_.load();if(!snapshot->picking||snapshot->generation!=state_.generation||snapshot->revision!=state_.revision||snapshot->view_epoch!=state_.view_epoch)return false;
 const auto hit=snapshot->picking->raycast(origin,direction,100,seconds);if(!hit||snapshot->picking->material_identity(hit->triangle).object!=o.id)return false;
 const size_t corner=size_t(hit->triangle)*3;const auto range=std::find_if(snapshot->ranges.begin(),snapshot->ranges.end(),[&](const auto&r){return corner>=r.first&&corner-r.first<r.count;});if(range==snapshot->ranges.end())throw std::logic_error("Face ray lost its actual material range");
 if(o.texture_sets){if(!range->texture_set_proof)throw std::logic_error("Set face ray has no membership authority");range->texture_set_proof->authenticate(*range);
  const auto&plan=range->texture_set_proof->plan();if(plan->collection()!=o.texture_sets||plan->collection().owner_before(o.texture_sets)||o.texture_sets.owner_before(plan->collection()))throw std::logic_error("Set face ray belongs to an earlier membership");
 }
 const uint32_t triangle=snapshot->picking->logical_triangle(hit->triangle);if(triangle>=o.painted->mesh().indices.size()/3)throw std::logic_error("Face ray invented a logical triangle");
 const uint64_t face=o.texture_sets?o.texture_sets->topology()->triangle_faces()[triangle]:uint64_t(triangle)+1;
 auto selected=additive&&texture_face_geometry_==o.painted->mesh_owner()?state_.texture_faces:std::vector<uint64_t>{};
 const auto at=std::lower_bound(selected.begin(),selected.end(),face);if(at!=selected.end()&&*at==face){if(additive)selected.erase(at);}else{if(selected.size()>=65536)throw std::length_error("Texture face selection exceeds 65536 faces");selected.insert(at,face);}
 texture_face_geometry_=o.painted->mesh_owner();texture_face_object_=o.id;state_.texture_faces=std::move(selected);return true;
}
} // namespace dome
