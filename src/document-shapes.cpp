// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "document-transaction.hpp"
namespace dome {
void Document::configure_shape_sculpt(bool enabled){std::lock_guard lock(model_mutex_);configure_shape_sculpt_locked(enabled);}
void Document::configure_shape_sculpt_locked(bool enabled){
 if(state_.shape_sculpt==enabled)return;
 if(!shape_target_ready_locked()||(enabled&&(!scene_.active().deformation||!scene_.active().deformation->morphs||scene_.active().deformation->morphs->targets.empty())))
  throw std::logic_error("Finish current editing and select a mesh with a shapekey");
 Transaction transaction(*this);input_.cancel();state_.shape_sculpt=enabled;state_.shape_sculpt_object=enabled?scene_.active().id:0;
 if(enabled)state_.weights.enabled=false;
 state_.message=enabled?"Sculpt selected shapekey":"Shapekey sculpt finished";
 shape_rearmed_={};publish_locked(true,false);transaction.commit();
}
bool Document::shape_target_ready_locked() const {
 const auto &o=scene_.active();
 return !state_.polygon&&!state_.cut&&!state_.retopology&&!uv_&&!state_.asset_busy&&!state_.recovery_busy&&
   !state_.object_grabbing&&!control_before_&&!action_active_locked()&&object_editable_locked(o.id)&&o.mesh_active&&o.painted;
}
void Document::queue_shape_locked(ShapeKeyEdit edit,AssetSource expected){
 if(!shape_target_ready_locked())throw std::logic_error("Finish current editing or object motion, then choose an editable mesh");
 auto object=scene_.active().id;auto work=std::make_shared<AssetWork>();auto changed=std::make_shared<bool>(false);
 queue_scene_operation_locked("Author shapekey",[object,edit=std::move(edit),work,changed](Scene &scene,const auto &cancelled){
  auto result=author_shapekey(scene,object,edit,cancelled);*changed=result.changed;work->morph_target.store(uint32_t(result.target));
 },expected,{},work,changed);
}
void Document::author_shape(ShapeKeyEdit edit,AssetSource expected){
 std::lock_guard lock(model_mutex_);queue_shape_locked(std::move(edit),expected);
}
void Document::process_shape_spatial_locked(const SpatialFrame &f){
 const bool valid=f.focused&&f.head_valid&&ob::finite(f.head)&&std::isfinite(f.time)&&f.time>=0;
 state_.hands=valid?f.hands:std::array<SpatialHand,2>{};
 auto sample=[&](size_t i){
  const auto &h=f.hands[i];const auto &pose=h.tracked.aimValid?h.tracked.aim:h.tracked.grip;
  NativeStrokeSample s;auto p=ob::transform(ob::inverse(state_.canvas),pose.position);s.origin={p.x,p.y,p.z};
  auto d=ob::rotate(ob::conjugate(state_.canvas.orientation),ob::rotate(pose.orientation,{0,0,-1}));s.direction={d.x,d.y,d.z};
  s.material_time=f.time;s.brush.radius=state_.radius/state_.canvas.scale;s.brush.tool=input_.tool();s.brush.pressure=std::clamp(h.tracked.trigger,0.f,1.f);s.brush.strength=s.brush.tool==Tool::Smooth?.55f:1;
  sample_native_locked(std::move(s));
 };
 if(native_stroke_&&native_stroke_->spatial_owner){
  auto session=native_stroke_;auto i=session->spatial_owner-1;const auto &h=f.hands[i];
  if(!valid||!h.tracked.valid||!ob::finite(h.tracked.grip)||!std::isfinite(h.tracked.trigger)||(h.tracked.aimValid&&!ob::finite(h.tracked.aim))){
   session->menu_after=false;shape_rearmed_={};if(!session->accepted.load())cancel_native_locked();publish_locked(false,false);return;
  }
  if(session->accepted.load())return;
  if(h.joystick){
   session->menu_after=true;state_.panel={h.tracked.grip.position,f.head.orientation,1};state_.panel.position=state_.panel.position+ob::rotate(f.head.orientation,{0,.08f,-.18f});
   end_native_locked(true);publish_locked(false,false);return;
  }
  if(h.tracked.trigger<=.02f){shape_rearmed_[i]=true;end_native_locked(true);publish_locked(false,false);return;}
  try{sample(i);}catch(...){cancel_native_locked();throw;}
  return;
 }
 if(!valid||input_.menu_open()||state_.controls.parameter!=VrParameter::None||state_.asset_busy||state_.object_grabbing){shape_rearmed_={};return;}
 for(size_t i=0;i<2;++i){const auto &h=f.hands[i];
  if(!h.tracked.valid){shape_rearmed_[i]=false;continue;}
  if(!std::isfinite(h.tracked.trigger)||!ob::finite(h.tracked.grip)||(h.tracked.aimValid&&!ob::finite(h.tracked.aim))){shape_rearmed_[i]=false;continue;}
  if(h.tracked.trigger<=.02f&&!h.joystick){shape_rearmed_[i]=true;continue;}
  if(!shape_rearmed_[i]||h.joystick)continue;
  shape_rearmed_={};if(begin_native_locked(uint32_t(i+1))){try{sample(i);}catch(...){cancel_native_locked();throw;}}return;
 }
}
} // namespace dome
