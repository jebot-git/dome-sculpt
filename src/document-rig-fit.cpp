// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/rig-fit.hpp"
#include "rig-fit-metric.hpp"
#include "document-transaction.hpp"
#include <nlohmann/json.hpp>
namespace dome {
void Document::fit_humanoid_rig(HumanLandmarks landmarks,RigFitOptions options,AssetSource expected){
 options.validate();std::lock_guard lock(model_mutex_);
 const auto id=scene_.active().id;
 if(!object_editable_locked(id)||!scene_.active().mesh_active||!scene_.active().painted)
  throw std::logic_error("Select an editable original painting mesh for humanoid fitting");
 if(scene_.active().deformation&&scene_.active().deformation->skeleton)
  throw std::logic_error("Use source-preserving rest fitting for the existing rig");
 const auto report=std::make_shared<std::string>();
 queue_scene_operation_locked("Fit humanoid rig",[id,landmarks=std::move(landmarks),options,report](Scene &scene,const auto &cancelled){
  auto fit=author_humanoid_fit(scene,id,landmarks,options,cancelled);
  *report="Humanoid rig fitted: "+std::to_string(fit.bones)+" bones"+(fit.bound?" with editable skin weights":"; use automatic skin bind to attach weights");
 },expected,report);
}
namespace {
Vec3 local_point(const AffineTransform &m,Vec3 p){auto q=m.inverse().point({p.x,p.y,p.z});return {float(q[0]),float(q[1]),float(q[2])};}
void validate_view(const RigFitView &view){
 if(view.parameter>=rig_fit_options_count(view)||view.role>=rig_fit_selection_count(view)||!std::isfinite(view.step)||view.step<.00001f||view.step>10)throw std::invalid_argument("Invalid selected landmark or world step");
 HumanLandmarks::decode(view.landmarks.encode());
 for(auto axis:{view.landmarks.left,view.landmarks.up,view.landmarks.front})if(std::abs(length(axis)-1)>1e-4f)throw std::invalid_argument("Body frame axes must be unit directions");
 if(length(cross(view.landmarks.left,view.landmarks.up)-view.landmarks.front)>1e-4f)throw std::invalid_argument("Body frame must be right-handed and orthonormal");
 if(view.rest_source){auto proposed=*view.rest_source->skeleton;rig_fit_rest_edits(view);for(size_t j=0;j<proposed.bones.size();++j){if(!finite(view.bone_heads[j])||!std::isfinite(view.bone_lengths[j])||view.bone_lengths[j]<=0||view.bone_lengths[j]>10000)throw std::invalid_argument("Invalid bone head or world length");proposed.bones[j].parent=view.bone_parents[j];}proposed.validate();}
 view.options.validate();
}
}
bool Document::rig_fit_source_matches_locked() const {
 const auto &v=state_.rig_fit;return v&&v->generation==state_.generation&&v->revision==state_.revision&&v->object==scene_.active().id&&v->edit_authority==state_.edit_authority&&object_editable_locked(v->object);
}
void Document::begin_rig_fit(){std::lock_guard lock(model_mutex_);begin_rig_fit_locked();}
void Document::begin_rig_fit_locked(){
 if(rig_fit_source_matches_locked())return;
 const auto &o=scene_.active();
 if(stopping_.load()||state_.asset_busy||state_.recovery_busy||state_.object_grabbing||state_.polygon||state_.cut||state_.retopology||uv_||native_stroke_||desktop_||action_active_locked()||control_before_||!object_editable_locked(o.id)||!o.mesh_active||!o.painted)
  throw std::logic_error("Select an idle editable painting mesh before placing humanoid landmarks");
 auto next=std::make_shared<RigFitView>();next->generation=state_.generation;next->revision=state_.revision;next->object=o.id;next->edit_authority=state_.edit_authority;next->world=scene_.world_matrix(o.id);
 if(o.deformation&&o.deformation->skeleton){next->rest_source=o.deformation;next->linked_objects=rig_rest_linked_objects(scene_,o.id);const auto heads=o.deformation->skeleton->segments();const auto rest=o.deformation->skeleton->world();for(size_t j=0;j<heads.size();++j){next->bone_heads.push_back(heads[j][0]);next->bone_parents.push_back(o.deformation->skeleton->bones[j].parent);next->bone_lengths.push_back(detail::rig_fit_world_length(next->world,rest[j],o.deformation->skeleton->bones[j].length));}}
 if(o.deformation&&o.deformation->landmarks)next->landmarks=*o.deformation->landmarks;
 else if(o.deformation&&o.deformation->skeleton&&o.deformation->humanoid){auto segments=o.deformation->skeleton->segments();for(auto &[role,joint]:o.deformation->humanoid->joints)next->landmarks.points.emplace(role,segments.at(joint)[0]);}
 Transaction transaction(*this);state_.rig_fit=std::move(next);state_.weights.enabled=false;state_.shape_sculpt=false;state_.message="Place and review every required humanoid landmark";publish_locked(false,false);transaction.commit(false);
}
void Document::configure_rig_fit(RigFitView view){
 validate_view(view);std::lock_guard lock(model_mutex_);
 if(!rig_fit_source_matches_locked()||state_.asset_busy||native_stroke_||desktop_||state_.object_grabbing||state_.rig_fit->placing)throw std::logic_error("The fitting source changed or is busy; reopen humanoid fitting");
 const auto &old=*state_.rig_fit;
 if(view.linked_objects!=old.linked_objects)throw std::logic_error("Captured linked target identities cannot be replaced");
 if(view.rest_source!=old.rest_source)throw std::logic_error("Fitting source skeleton authority cannot be replaced");
 if(view.generation!=old.generation||view.revision!=old.revision||view.object!=old.object||view.edit_authority!=old.edit_authority)throw std::logic_error("This landmark editor belongs to a previous source; reopen fitting");
 view.generation=old.generation;view.revision=old.revision;view.object=old.object;view.edit_authority=old.edit_authority;view.world=old.world;view.placing=view.armed=view.pressed=false;view.before_placement.reset();
 auto next=std::make_shared<const RigFitView>(std::move(view));Transaction transaction(*this);state_.rig_fit=std::move(next);publish_locked(false,false);transaction.commit(false);
}
void Document::accept_rig_fit(){std::lock_guard lock(model_mutex_);accept_rig_fit_locked();}
void Document::accept_rig_fit_locked(){
 if(!rig_fit_source_matches_locked()||state_.rig_fit->placing)throw std::logic_error("Finish landmark placement on the current fitting source");
 const auto view=state_.rig_fit;
 const auto report=std::make_shared<std::string>();const AssetSource expected{view->generation,view->revision,view->object,view->edit_authority};
 if(view->rest_source){
  const auto edits=rig_fit_rest_edits(*view);const auto linked=view->linked_rest;
  if(linked)for(auto id:view->linked_objects){if(scene_.object(id).locked||(state_.isolate_held&&state_.held_object&&state_.held_object!=id)||(state_.selection_focus&&std::find(state_.selected_objects.begin(),state_.selected_objects.end(),id)==state_.selected_objects.end()))throw std::logic_error("A linked rig part is read-only; release isolation or select all affected parts");}

  queue_scene_operation_locked(view->restore_source?"Restore original rig":"Fit existing rig",[view,edits,linked,report](Scene &scene,const auto &cancelled){RigRestOptions options;options.linked=linked;options.linked_objects=view->linked_objects;if(view->restore_source)restore_rig_source(scene,view->object,options,cancelled);else author_rig_rest(scene,view->object,edits,options,cancelled);*report=view->restore_source?"Original rig restored":"Existing rig fitted; skin pose and first source retained";},expected,report);return;
 }
 queue_scene_operation_locked("Fit humanoid rig",[view,report](Scene &scene,const auto &cancelled){auto result=author_humanoid_fit(scene,view->object,view->landmarks,view->options,cancelled);*report="Humanoid rig fitted: "+std::to_string(result.bones)+" bones";},expected,report);
}
void Document::cancel_rig_fit(AssetSource expected){std::lock_guard lock(model_mutex_);if(!state_.rig_fit)return;
 if(expected.generation&&(expected.generation!=state_.rig_fit->generation||expected.revision!=state_.rig_fit->revision||expected.object!=state_.rig_fit->object||expected.edit_authority!=state_.rig_fit->edit_authority))return;Transaction transaction(*this);state_.rig_fit.reset();contact_rearmed_={};state_.selection_release={true,true};input_.cancel();publish_locked(false,false);transaction.commit(false);}
bool Document::rig_fit_command_enabled_locked(SpatialCommand c) const {
 if(c==SpatialCommand::RigFitBegin)return !stopping_.load()&&!state_.asset_busy&&!state_.object_grabbing&&!state_.recovery_busy&&!native_stroke_&&!desktop_&&!control_before_&&!action_active_locked()&&!state_.polygon&&!state_.cut&&!state_.retopology&&!uv_&&scene_.active().mesh_active&&scene_.active().painted&&object_editable_locked(scene_.active().id);
 if(c==SpatialCommand::RigFitCancel)return bool(state_.rig_fit)&&!state_.asset_busy;
 if(!rig_fit_source_matches_locked()||state_.asset_busy||state_.object_grabbing)return false;
 if(state_.rig_fit->placing)return c==SpatialCommand::RigFitPlace;
 if(state_.rig_fit->settings&&c>=SpatialCommand::RigFitYDown&&c<=SpatialCommand::RigFitZUp)return false;
 if(c==SpatialCommand::RigFitRemove&&state_.rig_fit->rest_source)return false;
 if(c==SpatialCommand::RigFitApply)return true;
 return true;
}
void Document::rig_fit_action(SpatialCommand c){if(!rig_fit_command(c))throw std::invalid_argument("Expected humanoid fitting command");std::lock_guard lock(model_mutex_);rig_fit_command_locked(c);}
void Document::rig_fit_command_locked(SpatialCommand c){
 if(!rig_fit_command_enabled_locked(c))return;
 if(c==SpatialCommand::RigFitBegin){begin_rig_fit_locked();input_.retain_page(rig_fit_page,input_.command_device()?input_.command_device():1,0);publish_locked(false,false);return;}
 if(c==SpatialCommand::RigFitApply){accept_rig_fit_locked();return;}
 Transaction transaction(*this);
 if(c==SpatialCommand::RigFitCancel){state_.rig_fit.reset();contact_rearmed_={};input_.cancel();}
 else {auto next=std::make_shared<RigFitView>(*state_.rig_fit);const auto name=rig_fit_selection_label(*next);
  if(c==SpatialCommand::RigFitPrevious||c==SpatialCommand::RigFitNext){if(next->settings)next->parameter=(next->parameter+rig_fit_options_count(*next)+(c==SpatialCommand::RigFitNext?1:-1))%rig_fit_options_count(*next);else next->role=(next->role+rig_fit_selection_count(*next)+(c==SpatialCommand::RigFitNext?1:-1))%rig_fit_selection_count(*next);}
  else if(c==SpatialCommand::RigFitOptions)next->settings=!next->settings;
  else if(next->settings&&(c==SpatialCommand::RigFitXDown||c==SpatialCommand::RigFitXUp))adjust_rig_fit_parameter(*next,c==SpatialCommand::RigFitXUp?1:-1);
  else if(c==SpatialCommand::RigFitRemove){if(human_roles()[next->role].required)state_.message="Required landmark removed; place it again before applying";next->landmarks.points.erase(name);}
  else if(c==SpatialCommand::RigFitPlace){
   if(next->settings)next->settings=false;
   else if(next->placing){next->placing=false;next->before_placement.reset();}
   else {next->before_placement=rig_fit_selection_point(*next);next->placing=true;next->armed=next->pressed=false;next->owner=input_.command_device()?input_.command_device():1;input_.cancel();contact_rearmed_={};}
  }else {auto point=rig_fit_selection_point(*next);if(!point)throw std::logic_error("Place this landmark first");Vec3 delta{};const int axis=(int(c)-int(SpatialCommand::RigFitXDown))/2;const float sign=(int(c)-int(SpatialCommand::RigFitXDown))%2?1:-1;if(axis<0||axis>2)throw std::logic_error("Unknown landmark adjustment");if(axis==0)delta.x=sign*next->step;else if(axis==1)delta.y=sign*next->step;else delta.z=sign*next->step;
   auto v=next->world.inverse().direction({delta.x,delta.y,delta.z});rig_fit_selection_point(*next,*point+Vec3{float(v[0]),float(v[1]),float(v[2])});}
  validate_view(*next);state_.rig_fit=std::move(next);
 }
 publish_locked(false,false);transaction.commit(false);
}
bool Document::process_rig_fit_spatial_locked(const SpatialFrame &f){
 if(!state_.rig_fit||!state_.rig_fit->placing)return false;
 auto next=std::make_shared<RigFitView>(*state_.rig_fit);const size_t hand=next->owner?next->owner-1:0;const auto &h=f.hands.at(hand);
 const bool valid=rig_fit_source_matches_locked()&&f.focused&&f.head_valid&&ob::finite(f.head)&&std::isfinite(f.time)&&f.time>=0&&h.tracked.valid&&ob::finite(h.tracked.grip)&&std::isfinite(h.tracked.trigger);
 bool finish=false,cancel=!valid||h.joystick;
 const auto name=rig_fit_selection_label(*next);
 if(valid&&!cancel){if(h.tracked.trigger<=.02f){if(next->pressed)finish=true;else next->armed=true;}else if(next->armed){next->pressed=true;auto p=ob::transform(ob::inverse(state_.canvas),h.tracked.grip.position);rig_fit_selection_point(*next,local_point(next->world,{p.x,p.y,p.z}));}}
 if(cancel||finish){if(cancel){rig_fit_selection_point(*next,next->before_placement);}next->placing=false;next->before_placement.reset();contact_rearmed_={};state_.selection_release={true,true};if(valid){input_.retain_page(rig_fit_page,uint32_t(hand+1),2);state_.panel={h.tracked.grip.position,f.head.orientation,1};state_.panel.position=state_.panel.position+ob::rotate(f.head.orientation,{0,.08f,-.18f});}else input_.cancel();}
 Transaction transaction(*this);state_.rig_fit=std::move(next);state_.hands=valid?f.hands:std::array<SpatialHand,2>{};publish_locked(false,false);transaction.commit(false);return true;
}
} // namespace dome
