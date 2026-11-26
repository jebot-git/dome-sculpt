// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/creation.hpp"
#include "document-transaction.hpp"
#include "spatial-history-levels.hpp"
#include "text-utf8.hpp"
namespace dome {
namespace {
void expected_creator_source(AssetSource source,const HumanoidCreatorView&view){
 if((source.generation&&source.generation!=view.generation)||(source.revision&&source.revision!=view.revision)||
    (source.object&&source.object!=view.object)||(source.edit_authority&&source.edit_authority!=view.edit_authority))
  throw std::logic_error("This creator belongs to a previous document source");
}
std::optional<HumanoidCreatorSettings> selected_creator_settings(const Scene&scene){
 std::optional<HumanoidCreatorSettings> result;
 for(const auto &group:scene.group_path(scene.active().group)){
  if(!group.humanoid||(group.humanoid->baseline!="vroid-beta-female"&&group.humanoid->baseline!="vroid-beta-male"))continue;
  HumanoidCreatorSettings value;value.controls=*group.humanoid;value.preset.reset();
  value.name=text::valid_utf8(group.name)?text::utf8_prefix(group.name,118)+" variation":"Humanoid variation";
  value.validate();result=std::move(value);
 }
 return result;
}
}
bool Document::creator_source_matches_locked() const {
 const auto &v=state_.creator;return v&&v->generation==state_.generation&&v->revision==state_.revision&&
  v->object==scene_.active().id&&v->edit_authority==state_.edit_authority;
}
void Document::create_humanoid_avatar(HumanoidControls controls,std::string name,AssetSource expected,bool keep){
 std::lock_guard lock(model_mutex_);create_humanoid_avatar_locked(std::move(controls),std::move(name),expected,keep);
}
void Document::create_humanoid_avatar_locked(HumanoidControls controls,std::string name,AssetSource expected,bool keep){
 HumanoidCreatorSettings settings;settings.controls=controls;settings.name=name;settings.validate();
 const size_t count=(controls.hair_length||controls.hair_volume||controls.fringe)?20:19;
 if(keep&&(scene_.objects().size()>256-count||scene_.groups().size()>=128))
  throw std::logic_error("The scene has no room for another complete avatar");
 const bool initial=initial_creator_.generation==state_.generation&&initial_creator_.content==content_&&
   !initial_creator_.navigated&&undo_.empty()&&redo_.empty();
 auto work=initial?std::make_shared<AssetWork>():std::shared_ptr<AssetWork>{};
 queue_scene_operation_locked("Create humanoid",[controls=std::move(controls),name=std::move(name),keep](Scene&scene,const auto&cancelled){
  if(!keep)scene=Scene{};
  const auto empty=scene.active().id;
  CanvasTransform transform{};transform.position.y=-controls.height*.5f;
  append_humanoid(scene,controls,name,transform,256,cancelled);
  if(!keep)scene.erase(empty);
 },expected,{},work);
 if(initial)initial_creator_.pending=std::move(work);
}
void Document::begin_humanoid_creator(){std::lock_guard lock(model_mutex_);begin_humanoid_creator_locked();}
void Document::begin_humanoid_creator_locked(){
 if(creator_source_matches_locked())return;
 if(!creator_command_enabled_locked(SpatialCommand::CreatorBegin))
  throw std::logic_error("Finish the current edit and release focused or held editing before creating an avatar");
 if(creator_serial_==UINT64_MAX||state_.input_epoch==UINT64_MAX)throw std::overflow_error("Creator session identity exhausted");
 auto view=std::make_shared<HumanoidCreatorView>();view->settings.controls=humanoid_baseline_defaults("vroid-beta-female");
 view->session=creator_serial_+1;view->generation=state_.generation;view->revision=state_.revision;
 view->object=scene_.active().id;view->edit_authority=state_.edit_authority;
 view->can_copy_selected=bool(selected_creator_settings(scene_));
 const auto owner=input_.command_device()?input_.command_device():1;
 Transaction transaction(*this);state_.creator=std::move(view);++state_.input_epoch;
 input_.cancel();input_.retain_page(humanoid_creator_page,owner,0);transaction.rebind();
 state_.selection_release={true,true};state_.error.clear();state_.message="Creator draft: Apply creates a new avatar; existing edits are kept";
 publish_locked(false,false);transaction.commit(false);++creator_serial_;
}
void Document::configure_humanoid_creator(HumanoidCreatorSettings settings,uint64_t session,AssetSource expected){
 std::lock_guard lock(model_mutex_);configure_humanoid_creator_locked(std::move(settings),session,expected);
}
void Document::configure_humanoid_creator_locked(HumanoidCreatorSettings settings,uint64_t session,AssetSource expected){
 settings.validate();
 if(!creator_source_matches_locked()||state_.asset_busy||!session||session!=state_.creator->session)
  throw std::logic_error("Reopen the creator on the current document");
 expected_creator_source(expected,*state_.creator);
 if(control_before_)throw std::logic_error("Finish the creator value slider first");
 auto view=std::make_shared<HumanoidCreatorView>(*state_.creator);update_humanoid_creator(*view,std::move(settings));
 Transaction transaction(*this);state_.creator=std::move(view);publish_locked(false,false);transaction.commit(false);
}
void Document::end_humanoid_creator(bool apply,uint64_t session,AssetSource expected){
 std::lock_guard lock(model_mutex_);end_humanoid_creator_locked(apply,session,expected);
}
void Document::end_humanoid_creator_locked(bool apply,uint64_t session,AssetSource expected){
 const auto view=state_.creator;if(!view)return;
 if(session&&session!=view->session)throw std::logic_error("This creator session has been retired");
 expected_creator_source(expected,*view);
 if(state_.input_epoch==UINT64_MAX)throw std::overflow_error("Creator input identity exhausted");
 if(apply){
  if(!creator_source_matches_locked()||state_.asset_busy||control_before_||view->slider_before)
   throw std::logic_error("Finish the current creator value before applying");
  view->settings.validate();
  // The ordered worker owns its own transaction. Restore the complete draft
  // and input namespace if queue admission refuses before publication.
  auto previous=state_;auto previous_input=input_;
  state_.creator.reset();++state_.input_epoch;input_.cancel();
  try{create_humanoid_avatar_locked(view->settings.controls,view->settings.name,
    {view->generation,view->revision,view->object,view->edit_authority},true);}
  catch(...){state_=std::move(previous);input_=std::move(previous_input);throw;}
  contact_rearmed_=shape_rearmed_={};state_.selection_release={true,true};return;
 }
 Transaction transaction(*this);
 if(state_.controls.parameter==VrParameter::CreatorValue){control_before_.reset();control_changed_=false;state_.controls={};}
 cancel_initial_creator_fit_locked(); // Cancel never triggers a delayed prior fit.
 state_.creator.reset();++state_.input_epoch;input_.cancel();transaction.rebind();navigation_.reset();
 contact_rearmed_=shape_rearmed_={};state_.selection_release={true,true};state_.error.clear();
 state_.message="Creator cancelled; original scene unchanged";publish_locked(false,false);transaction.commit(false);
}
bool Document::creator_history_locked(bool redo){
 if(!creator_source_matches_locked()||state_.asset_busy)return false;
 if(control_before_){if(redo)return false;finish_creator_control_locked(false);return true;}
 auto view=std::make_shared<HumanoidCreatorView>(*state_.creator);
 const bool changed=humanoid_creator_history(*view,redo);if(!changed)return false;
 Transaction transaction(*this);state_.creator=std::move(view);publish_locked(false,false);transaction.commit(false);return true;
}
bool Document::creator_command_enabled_locked(SpatialCommand command) const {
 if(command==SpatialCommand::CreatorBegin){
  if(creator_source_matches_locked())return true;
  return !stopping_&&!state_.asset_busy&&!state_.recovery_busy&&!state_.object_grabbing&&!navigation_.grabbing()&&
   !control_before_&&!native_stroke_&&!desktop_&&!action_active_locked()&&!state_.polygon&&!state_.cut&&
   !state_.retopology&&!uv_&&!state_.rig_fit&&!state_.expressions&&!import_view_&&
   (!state_.projection||!state_.projection->placing)&&!state_.selection_focus&&!(state_.isolate_held&&state_.held_object)&&
   scene_.objects().size()<=236&&scene_.groups().size()<128&&object_editable_locked(scene_.active().id);
 }
 if(command==SpatialCommand::CreatorCancel||command==SpatialCommand::CancelWork)return bool(state_.creator);
 if(!creator_source_matches_locked()||state_.asset_busy)return false;
 if(command==SpatialCommand::Undo)return bool(state_.creator->slider_before)||!state_.creator->undo.empty();
 if(command==SpatialCommand::Redo)return !control_before_&&!state_.creator->redo.empty();
 if(control_before_)return false;
 if(command==SpatialCommand::CreatorCopySettings)return state_.creator->can_copy_selected;
 return creator_command(command)||command==SpatialCommand::Center;
}
void Document::humanoid_creator_action(SpatialCommand command){
 if(!creator_command(command))throw std::invalid_argument("Expected a humanoid creator command");
 std::lock_guard lock(model_mutex_);creator_command_locked(command);
}
void Document::creator_command_locked(SpatialCommand command){
 if(!creator_command_enabled_locked(command))return;
 if(command==SpatialCommand::CreatorBegin){begin_humanoid_creator_locked();return;}
 if(command==SpatialCommand::CreatorApply||command==SpatialCommand::CreatorCancel||command==SpatialCommand::CancelWork){end_humanoid_creator_locked(command==SpatialCommand::CreatorApply);return;}
 if(command==SpatialCommand::Undo||command==SpatialCommand::Redo){creator_history_locked(command==SpatialCommand::Redo);return;}
 auto view=std::make_shared<HumanoidCreatorView>(*state_.creator);
 if(command==SpatialCommand::CreatorPrevious||command==SpatialCommand::CreatorNext){
  view->parameter=(view->parameter+humanoid_creator_parameter_count+(command==SpatialCommand::CreatorNext?1:-1))%humanoid_creator_parameter_count;
 }else{
  auto settings=view->settings;
  if(command==SpatialCommand::CreatorCopySettings){auto selected=selected_creator_settings(scene_);if(!selected)throw std::logic_error("The selected group has no genuine CC0 creator settings");settings=std::move(*selected);}
  else if(command==SpatialCommand::CreatorBaseline){settings.controls=humanoid_baseline_defaults(settings.controls.baseline=="vroid-beta-female"?"vroid-beta-male":"vroid-beta-female");settings.preset=HumanoidPreset::Neutral;}
  else {const auto preset=command==SpatialCommand::CreatorNeutral?HumanoidPreset::Neutral:command==SpatialCommand::CreatorToon?HumanoidPreset::Toon:HumanoidPreset::Anime;
   settings.controls=humanoid_preset(preset,settings.controls.baseline);settings.preset=preset;}
  update_humanoid_creator(*view,std::move(settings));
 }
 Transaction transaction(*this);state_.creator=std::move(view);state_.error.clear();publish_locked(false,false);transaction.commit(false);
}
void Document::finish_creator_control_locked(bool accept){
 if(state_.controls.parameter!=VrParameter::CreatorValue||!control_before_)return;
 if(!creator_source_matches_locked()||!state_.creator->slider_before)throw std::logic_error("Creator slider source was retired");
 auto view=std::make_shared<HumanoidCreatorView>(*state_.creator);
 const auto before=*view->slider_before;auto after=view->settings;view->settings=before;view->slider_before.reset();
 if(accept)update_humanoid_creator(*view,std::move(after));
 Transaction transaction(*this);state_.creator=std::move(view);control_before_.reset();control_changed_=false;state_.controls={};input_.complete_control();
 publish_locked(false,false);transaction.commit(false);
}
void Document::creator_control_locked(VrControlEvent event){
 if(event.parameter!=VrParameter::CreatorValue)return;
 if(event.type==VrControlEventType::Begin){
  if(!creator_source_matches_locked()||state_.asset_busy||control_before_)throw std::logic_error("Open the current idle creator value first");
  auto view=std::make_shared<HumanoidCreatorView>(*state_.creator);view->slider_before=view->settings;
  const auto parameter=humanoid_creator_parameter(view->settings.controls,view->parameter);
  VrControlState controls;controls.parameter=VrParameter::CreatorValue;controls.title=parameter.label;
  controls.minimum=parameter.minimum;controls.maximum=parameter.maximum;controls.value[0]=parameter.value;
  Transaction transaction(*this);state_.creator=std::move(view);state_.controls=std::move(controls);
  control_before_=scene_;control_object_=scene_.active().id;control_source_={state_.generation,state_.revision,control_object_,state_.edit_authority};
  control_changed_=false;publish_locked(false,false);transaction.commit(false);return;
 }
 if(!creator_source_matches_locked()||!control_before_||state_.controls.parameter!=VrParameter::CreatorValue)return;
 if(event.type==VrControlEventType::Confirm||event.type==VrControlEventType::Cancel){finish_creator_control_locked(event.type==VrControlEventType::Confirm);return;}
 if(event.component!=0||!std::isfinite(event.value)||event.type==VrControlEventType::Select)return;
 const auto &control=state_.controls;
 const float value=std::clamp(event.type==VrControlEventType::Absolute?control.minimum+std::clamp(event.value,0.f,1.f)*(control.maximum-control.minimum):control.value[0]+event.value*.5f*(control.maximum-control.minimum),control.minimum,control.maximum);
 if(value==control.value[0])return;
 auto view=std::make_shared<HumanoidCreatorView>(*state_.creator);set_humanoid_creator_parameter(view->settings.controls,view->parameter,value);view->settings.preset.reset();
 Transaction transaction(*this);state_.creator=std::move(view);state_.controls.value[0]=value;control_changed_=true;publish_locked(false,false);transaction.commit(false);
}
} // namespace dome
