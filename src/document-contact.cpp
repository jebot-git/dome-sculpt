// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
namespace dome {
bool Document::process_affine_paint_spatial_locked(const SpatialFrame &f){
 const bool active=native_stroke_&&native_stroke_->space==NativeStrokeSpace::EvaluatedWorldBrush;
 if(!active&&(!scene_.has_affine_placement(scene_.active().id)||!scene_.active().mesh_active||state_.shape_sculpt))return false;
 const bool valid=f.focused&&f.head_valid&&ob::finite(f.head)&&std::isfinite(f.time)&&f.time>=0;
 auto sample=[&](size_t i){
  const auto &h=f.hands[i].tracked;auto p=ob::transform(ob::inverse(state_.canvas),h.grip.position);
  auto n=ob::rotate(ob::conjugate(state_.canvas.orientation),ob::rotate(h.grip.orientation,{0,0,1}));
  auto &session=*native_stroke_;NativeStrokeSample s;s.space=NativeStrokeSpace::EvaluatedWorldBrush;s.material_time=f.time;
  s.brush.to={p.x,p.y,p.z};s.brush.from=session.contact_previous.value_or(s.brush.to);s.brush.normal={n.x,n.y,n.z};
  s.brush.radius=state_.radius/state_.canvas.scale;s.brush.pressure=std::clamp(h.trigger,0.f,1.f);s.brush.strength=input_.strength;s.brush.tool=input_.tool();
  if(sample_native_locked(s))session.contact_previous=s.brush.to;
 };
 if(active){
  auto session=native_stroke_;const size_t i=session->spatial_owner-1;const auto &h=f.hands[i];state_.hands=valid?f.hands:std::array<SpatialHand,2>{};
  if(!valid||!h.tracked.valid||!ob::finite(h.tracked.grip)||!std::isfinite(h.tracked.trigger)){
   session->menu_after=false;contact_rearmed_={};if(!session->accepted.load())cancel_native_locked();publish_locked(false,false);return true;
  }
  if(session->accepted.load())return true;
  if(h.joystick){session->menu_after=true;state_.panel={h.tracked.grip.position,f.head.orientation,1};state_.panel.position=state_.panel.position+ob::rotate(f.head.orientation,{0,.08f,-.18f});end_native_locked(true);publish_locked(false,false);return true;}
  if(h.tracked.trigger<=.02f){contact_rearmed_[i]=true;end_native_locked(true);publish_locked(false,false);return true;}
  try{sample(i);}catch(...){cancel_native_locked();throw;}return true;
 }
 if(!valid||input_.menu_open()||state_.controls.parameter!=VrParameter::None||state_.asset_busy||state_.object_grabbing||state_.weights.enabled||state_.polygon||state_.cut||state_.retopology){contact_rearmed_={};return false;}
 for(size_t i=0;i<2;++i){const auto &h=f.hands[i];
  if(!h.tracked.valid||!std::isfinite(h.tracked.trigger)||!ob::finite(h.tracked.grip)){contact_rearmed_[i]=false;continue;}
  if(h.tracked.trigger<=.02f&&!h.joystick){contact_rearmed_[i]=true;continue;}
  if(!contact_rearmed_[i]||h.joystick)continue;
  contact_rearmed_={};if(begin_native_locked(uint32_t(i+1),NativeStrokeSpace::EvaluatedWorldBrush)){try{sample(i);}catch(...){cancel_native_locked();throw;}}return true;
 }
 return false;
}
} // namespace dome
