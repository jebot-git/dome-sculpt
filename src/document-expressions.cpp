// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/humanoid-creator.hpp"
#include "document-transaction.hpp"
#include <limits>
namespace dome {
namespace {
uint64_t next_epoch(uint64_t value){if(value==UINT64_MAX)throw std::overflow_error("Expression preview identity exhausted");return value+1;}
void expected_source(AssetSource expected,const VrmExpressionView &view){
 if((expected.generation&&expected.generation!=view.generation)||(expected.revision&&expected.revision!=view.revision)||
    (expected.object&&expected.object!=view.object)||(expected.edit_authority&&expected.edit_authority!=view.edit_authority))
  throw std::logic_error("This expression editor belongs to a previous source");
}
}
bool Document::expression_source_matches_locked() const {
 const auto &view=state_.expressions;if(!view||view->generation!=state_.generation||view->revision!=state_.revision||view->object!=scene_.active().id||view->edit_authority!=state_.edit_authority)return false;
 const auto &o=scene_.active();return o.visible&&!o.locked&&o.mesh_active&&o.painted&&!state_.selection_focus&&!(state_.isolate_held&&state_.held_object);
}
bool Document::can_preview_expressions() const {std::lock_guard lock(model_mutex_);return expression_command_enabled_locked(SpatialCommand::ExpressionBegin);}
void Document::begin_expression_preview(){std::lock_guard lock(model_mutex_);begin_expression_preview_locked();}
void Document::begin_expression_preview_locked(){
 if(expression_source_matches_locked())return;
 const auto &object=scene_.active();
 if(stopping_||state_.asset_busy||state_.recovery_busy||state_.object_grabbing||navigation_.grabbing()||control_before_||native_stroke_||desktop_||action_active_locked()||state_.polygon||state_.cut||state_.retopology||uv_||state_.rig_fit||(state_.projection&&state_.projection->placing)||state_.selection_focus||(state_.isolate_held&&state_.held_object)||!object_editable_locked(object.id)||!object.mesh_active||!object.painted||!object.appearance||!object.appearance->asset||object.appearance->asset->avatar_version!=ImportedAvatarVersion::Vrm1)
  throw std::logic_error("Select an idle editable original VRM1 source and release focused/held editing before previewing expressions");
 auto view=std::make_shared<VrmExpressionView>();view->generation=state_.generation;view->revision=state_.revision;view->object=object.id;view->edit_authority=state_.edit_authority;view->session=view->request=next_epoch(state_.view_epoch);view->baseline_=std::make_shared<const Scene>(scene_.snapshot());
 Transaction transaction(*this);state_.expressions=std::move(view);state_.view_epoch=state_.expressions->request;state_.input_epoch=next_epoch(state_.input_epoch);input_.cancel();transaction.rebind();state_.weights.enabled=state_.shape_sculpt=false;state_.error.clear();state_.message="Reading original expressions; preview is not applied";publish_locked(false,false);transaction.commit(false);
}
void Document::configure_expression_preview(size_t entry,float weight,uint64_t session,AssetSource expected){std::lock_guard lock(model_mutex_);configure_expression_preview_locked(entry,weight,session,expected);}
void Document::configure_expression_preview_locked(size_t entry,float weight,uint64_t session,AssetSource expected){
 if(!std::isfinite(weight)||weight<0||weight>1)throw std::invalid_argument("Expression weight must be between zero and one");
 if(!expression_source_matches_locked()||state_.asset_busy||state_.object_grabbing)throw std::logic_error("Reopen expressions on the current idle source");
 const auto &old=*state_.expressions;if(!session||session!=old.session)throw std::logic_error("This expression editor belongs to a retired preview session");expected_source(expected,old);if(entry>=old.entries.size())throw std::out_of_range("Select an actual source expression");
 if(old.entries[entry].weight==weight&&old.error.empty()){
  if(old.selected==entry)return;
  auto selected=std::make_shared<VrmExpressionView>(old);selected->selected=entry;Transaction transaction(*this);state_.expressions=std::move(selected);publish_locked(false,false);transaction.commit(false);return;
 }
 auto view=std::make_shared<VrmExpressionView>(old);view->selected=entry;view->entries[entry].weight=weight;const auto &selected=view->entries[entry];(selected.custom?view->weights_.custom:view->weights_.preset)[selected.name]=weight;view->request=next_epoch(state_.view_epoch);view->ready=false;view->error.clear();
 Transaction transaction(*this);state_.expressions=std::move(view);state_.view_epoch=state_.expressions->request;state_.error.clear();state_.message="Expression preview only; Apply static pose or Cancel";publish_locked(false,false);transaction.commit(false);
}
void Document::end_expression_preview(bool apply,uint64_t session,AssetSource expected){std::lock_guard lock(model_mutex_);end_expression_preview_locked(apply,session,expected);}
void Document::end_expression_preview_locked(bool apply,uint64_t session,AssetSource expected){
 const auto view=state_.expressions;if(!view)return;if(session&&session!=view->session)throw std::logic_error("This expression editor belongs to a retired preview session");expected_source(expected,*view);
 if(apply){
  if(!expression_source_matches_locked()||!view->ready||!view->error.empty())throw std::logic_error("Wait for a valid current expression preview before applying");
  // Queueing owns its own transaction. Clear only ephemeral state before that
  // admission; restore it exactly if the queue rejects before publication.
  const auto next_view=next_epoch(state_.view_epoch),next_input=next_epoch(state_.input_epoch);auto previous=state_;auto old_input=input_;
  state_.expressions.reset();state_.view_epoch=next_view;state_.input_epoch=next_input;input_.cancel();
  try{queue_scene_operation_locked("Apply static VRM1 expressions",[view](Scene &scene,const auto &cancelled){scene=bake_vrm1_expression_pose(*view->baseline_,view->object,view->weights_,cancelled);},AssetSource{view->generation,view->revision,view->object,view->edit_authority},{});}
  catch(...){state_=std::move(previous);input_=std::move(old_input);throw;}
  contact_rearmed_=shape_rearmed_={};return;
 }
 Transaction transaction(*this);state_.expressions.reset();state_.view_epoch=next_epoch(state_.view_epoch);state_.input_epoch=next_epoch(state_.input_epoch);input_.cancel();transaction.rebind();contact_rearmed_=shape_rearmed_={};state_.selection_pressed={true,true};state_.selection_release={true,true};state_.error.clear();state_.message="Expression preview cancelled; original scene unchanged";publish_locked(false,false);transaction.commit(false);
}
bool Document::expression_command_enabled_locked(SpatialCommand command) const {
 if(command==SpatialCommand::ExpressionBegin){if(expression_source_matches_locked())return true;const auto &o=scene_.active();return !state_.asset_busy&&!state_.recovery_busy&&!action_active_locked()&&!state_.object_grabbing&&!control_before_&&!state_.polygon&&!state_.cut&&!state_.retopology&&!uv_&&!state_.rig_fit&&(!state_.projection||!state_.projection->placing)&&!state_.selection_focus&&!(state_.isolate_held&&state_.held_object)&&object_editable_locked(o.id)&&o.mesh_active&&o.painted&&o.appearance&&o.appearance->asset&&o.appearance->asset->avatar_version==ImportedAvatarVersion::Vrm1;}
 if(command==SpatialCommand::ExpressionCancel)return bool(state_.expressions);
 if(!expression_source_matches_locked()||state_.asset_busy||state_.expressions->entries.empty())return false;
 return command!=SpatialCommand::ExpressionApply||(state_.expressions->ready&&state_.expressions->error.empty());
}
void Document::expression_action(SpatialCommand command){if(!expression_command(command))throw std::invalid_argument("Expected an expression command");std::lock_guard lock(model_mutex_);expression_command_locked(command);}
void Document::expression_command_locked(SpatialCommand command){
 if(!expression_command_enabled_locked(command))return;
 if(command==SpatialCommand::ExpressionBegin){const auto owner=input_.command_device()?input_.command_device():1;begin_expression_preview_locked();input_.retain_page(vrm_expression_page,owner,0);publish_locked(false,false);return;}
 if(command==SpatialCommand::ExpressionCancel||command==SpatialCommand::ExpressionApply){end_expression_preview_locked(command==SpatialCommand::ExpressionApply);return;}
 auto view=std::make_shared<VrmExpressionView>(*state_.expressions);
 if(command==SpatialCommand::ExpressionPrevious||command==SpatialCommand::ExpressionNext){view->selected=(view->selected+view->entries.size()+(command==SpatialCommand::ExpressionNext?1:-1))%view->entries.size();Transaction transaction(*this);state_.expressions=std::move(view);publish_locked(false,false);transaction.commit(false);return;}
 if(command==SpatialCommand::ExpressionReset){view->weights_={};for(auto &entry:view->entries)entry.weight=0;view->request=next_epoch(state_.view_epoch);view->ready=false;view->error.clear();Transaction transaction(*this);state_.expressions=std::move(view);state_.view_epoch=state_.expressions->request;state_.error.clear();publish_locked(false,false);transaction.commit(false);return;}
 const auto selected=view->selected;const auto amount=view->entries.at(selected).weight;configure_expression_preview_locked(selected,std::clamp(amount+(command==SpatialCommand::ExpressionIncrease?.05f:-.05f),0.f,1.f),view->session,{});
}
}
