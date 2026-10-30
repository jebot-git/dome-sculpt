// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "document-held-affine.hpp"
#include "dome/material-visibility.hpp"
#include "document-transaction.hpp"
#include "document-authority-transition.hpp"
namespace dome {
namespace {
Vec3 vec(ob::Vec3 p){return {p.x,p.y,p.z};}
bool equal_transform(CanvasTransform a,CanvasTransform b){return a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z&&a.orientation.w==b.orientation.w&&a.orientation.x==b.orientation.x&&a.orientation.y==b.orientation.y&&a.orientation.z==b.orientation.z&&a.scale==b.scale;}
}
CanvasTransform Document::object_world_transform_locked(uint64_t id) const {
  auto world=scene_.world_transform(id);
  if(state_.object_grabbing && state_.held_object==id)
    return ob::compose(ob::compose(world,ob::inverse(scene_.object(id).transform)),held_preview_transform_);
  return world;
}
AffineTransform Document::object_world_matrix_locked(uint64_t id) const {
  if(state_.object_grabbing&&state_.held_object==id&&held_affine_)return held_affine_->world(held_preview_transform_);
  return scene_.world_matrix(id);
}
bool Document::object_editable_locked(uint64_t id) const {
  auto found=std::find_if(scene_.objects().begin(),scene_.objects().end(),[&](const auto &o){return o.id==id;});
  return found!=scene_.objects().end() && found->visible && !found->locked &&
    (!state_.isolate_held || !state_.held_object || state_.held_object==id);
}
bool Document::object_editable(uint64_t id) const {std::lock_guard lock(model_mutex_);return object_editable_locked(id);}
void Document::require_object_editable_locked(uint64_t id) const {
  if(!object_editable_locked(id))throw std::logic_error("This object is read-only; release held-object isolation or select the editable held object");
}
void Document::require_authority_locked(AssetSource source) const {
  if(source.edit_authority && source.edit_authority!=state_.edit_authority)throw std::logic_error("Object editing authority changed; reopen the editor");
}
bool Document::invalidate_object_authority_locked() {
  if(state_.edit_authority==UINT64_MAX)throw std::overflow_error("Object editing authority exhausted");
  if(native_stroke_)cancel_native_locked();
  if(action_active_locked())cancel_action_locked();
  desktop_=false;input_.cancel();finish_control_locked(false);
  if(state_.retopology)cancel_retopology_locked();
  if(state_.cut)cancel_cut_locked();
  invalidate_uv_locked();
  // Accepted polygon cancellation follows successful permission publication.
  if(!polygon_accepting_)cancel_asset_work_locked();
  state_.object_grabbing=false;held_affine_.reset();object_navigation_.reset();navigation_.reset();
  ++state_.edit_authority;return false;
}
bool Document::held_weight_pending_locked() const {auto work=asset_work_.load();return state_.asset_busy&&work&&work->held_weight&&!work->cancelled.load();}
void Document::configure_object_grab(bool mode,bool isolate){std::lock_guard lock(model_mutex_);configure_object_grab_locked(mode,isolate);}
void Document::configure_object_grab_locked(bool mode,bool isolate){
  if(state_.object_grab_mode==mode && state_.isolate_held==isolate)return;
  if(held_weight_pending_locked())throw std::logic_error("Wait for the accepted held weight edit before changing grab controls");
  const bool picking_change=state_.object_grab_mode!=mode;
  const bool keep_motion=state_.object_grabbing&&!picking_change;const auto pose=held_preview_transform_,start=held_start_transform_;const auto nav=object_navigation_;const bool desktop=desktop_;const auto affine=held_affine_;
  AuthorityTransition transition(*this);const bool changed=invalidate_object_authority_locked();
  state_.object_grab_mode=mode;state_.isolate_held=isolate;
  if(keep_motion){state_.object_grabbing=true;held_preview_transform_=pose;held_start_transform_=start;object_navigation_=nav;desktop_=desktop;held_affine_=affine;}
  if(!mode)state_.held_object=0;
  publish_locked(changed||picking_change,false);transition.commit();
}
void Document::hold_object(uint64_t id){
  std::lock_guard lock(model_mutex_);
  if(state_.held_object==id)return;
  const auto &object=scene_.object(id);if(!object.visible||object.locked)throw std::logic_error("Hold a visible unlocked object");
  AuthorityTransition transition(*this);const bool changed=invalidate_object_authority_locked();state_.held_object=id;scene_.select(id);input_.bind(volume());bound_volume_=scene_.active().volume;last_volume_=volume().revision();last_paint_=scene_.active().mesh_active?scene_.active().painted->revision():0;publish_locked(changed,false);transition.commit();
}
void Document::release_held_object(bool accept){
  std::lock_guard lock(model_mutex_);if(state_.object_grabbing){finish_object_grab_locked(held_polygon_pending_locked()?false:accept,true);return;}
  AuthorityTransition transition(*this);const bool changed=invalidate_object_authority_locked();state_.held_object=0;publish_locked(changed,false);transition.commit();
}
bool Document::begin_object_grab(uint64_t id){std::lock_guard lock(model_mutex_);const bool began=begin_object_grab_locked(id);if(began)desktop_=true;return began;}
bool Document::begin_object_grab_locked(uint64_t id){
  if(!state_.object_grab_mode||state_.asset_busy||state_.recovery_busy)return false;
  const auto &object=scene_.object(id);if(!object.visible||object.locked)return false;
  const bool polygon_motion=state_.polygon && !polygon_accepting_ && id==polygon_source_.object && polygon_source_matches_locked();
  if(state_.polygon&&!polygon_motion)return false;
  AuthorityTransition transition(*this);const bool changed=polygon_motion?false:invalidate_object_authority_locked();scene_.select(id);input_.bind(volume());bound_volume_=scene_.active().volume;last_volume_=volume().revision();last_paint_=scene_.active().mesh_active?scene_.active().painted->revision():0;
  state_.held_object=id;held_preview_transform_=held_start_transform_=scene_.object(id).transform;state_.object_grabbing=true;
  held_affine_=scene_.has_affine_placement(id)?std::make_shared<const HeldAffine>(scene_,id):nullptr;
  publish_locked(changed,false);transition.commit();return true;
}
void Document::move_held_object(CanvasTransform local){std::lock_guard lock(model_mutex_);move_held_object_locked(local);}
void Document::move_held_object_locked(CanvasTransform local){
  validate_transform(local);
  if(held_polygon_pending_locked())throw std::logic_error("Wait for the accepted polygon edit before moving its captured pose");
  if(!state_.object_grabbing || !state_.held_object)throw std::logic_error("Begin an object grab first");
  require_object_editable_locked(state_.held_object);
  if(equal_transform(held_preview_transform_,local))return;
  const auto previous=held_preview_transform_;const auto affine=held_affine_;const auto navigation=object_navigation_;
  if(affine){auto candidate=std::make_shared<HeldAffine>(*affine);candidate->rebase(local);held_affine_=std::move(candidate);object_navigation_.reset();}
  held_preview_transform_=local;
  try{publish_locked(false,false);}catch(...){held_preview_transform_=previous;held_affine_=affine;object_navigation_=navigation;throw;}
}
void Document::move_held_object_world(CanvasTransform delta){std::lock_guard lock(model_mutex_);move_held_object_world_locked(delta);}
void Document::move_held_object_world_locked(CanvasTransform delta) {
  (void)affine_similarity(delta);
  if(!state_.object_grabbing||!state_.held_object)throw std::logic_error("Begin an object grab first");
  if(held_polygon_pending_locked())throw std::logic_error("Wait for the accepted polygon edit before moving its captured pose");
  require_object_editable_locked(state_.held_object);
  if(!held_affine_){
    auto parent=ob::compose(scene_.world_transform(state_.held_object),ob::inverse(scene_.object(state_.held_object).transform));
    move_held_object_locked(ob::compose(ob::inverse(parent),ob::compose(delta,scene_.world_transform(state_.held_object))));return;
  }
  const auto previous=held_affine_;
  if(equal_transform(ob::compose(delta,previous->base_pivot),previous->preview_pivot))return;
  auto candidate=std::make_shared<HeldAffine>(*previous);candidate->move(delta);held_affine_=std::move(candidate);
  try{publish_locked(false,false);}catch(...){held_affine_=previous;throw;}
}

void Document::update_object_grab_locked(const SpatialFrame &frame){
  std::array<TrackedHand,2> hands{frame.hands[0].tracked,frame.hands[1].tracked};
  for(size_t i=0;i<2;++i)if(!hands[i].valid||hands[i].squeeze<=.55f)polygon_grip_latch_[i]=false;
  if(held_polygon_pending_locked())return;
  if(state_.object_grabbing&&!object_navigation_.grabbing()&&held_weight_pending_locked())return;
  if(!state_.object_grabbing){
    int owner=-1;for(int i=0;i<2;++i)if(!polygon_grip_latch_[i]&&hands[i].valid&&hands[i].squeeze>.55f&&(!state_.hands[i].tracked.valid||state_.hands[i].tracked.squeeze<=.55f)){owner=i;break;}
    if(owner<0)return;
    uint64_t target=scene_.active().id;
    if(hands[owner].aimValid){
      auto snapshot=mesh_.load();if(!snapshot->picking||snapshot->generation!=state_.generation||snapshot->revision!=state_.revision)return;
      const auto &aim=hands[owner].aim;
      auto origin=world_to_model(state_.canvas,vec(aim.position));
      auto direction=vec(ob::rotate(ob::conjugate(state_.canvas.orientation),ob::rotate(aim.orientation,{0,0,-1})));
      auto hit=snapshot->picking->raycast(origin,direction,100,frame.time);if(!hit)return;
      const size_t corner=size_t(hit->triangle)*3;target=0;
      for(const auto&r:snapshot->ranges)if(corner>=r.first&&corner-r.first<r.count){target=r.object;break;}
      if(!target)return;
    }
    if(!begin_object_grab_locked(target))return;
  }
  const auto navigation=object_navigation_;
  try {
    if(held_affine_) {
      const auto world=ob::compose(state_.canvas,held_affine_->preview_pivot);
      if(auto moved=object_navigation_.update(hands,world)) {
        const auto pivot=ob::compose(ob::inverse(state_.canvas),*moved);
        move_held_object_world_locked(ob::compose(pivot,ob::inverse(held_affine_->base_pivot)));
      }
    } else {
      auto world=ob::compose(state_.canvas,object_world_transform_locked(state_.held_object));
      if(auto moved=object_navigation_.update(hands,world)){
        auto parent=ob::compose(scene_.world_transform(state_.held_object),ob::inverse(scene_.object(state_.held_object).transform));
        auto local=ob::compose(ob::inverse(parent),ob::compose(ob::inverse(state_.canvas),*moved));
        move_held_object_locked(local);
      }
    }
  }catch(...){object_navigation_=navigation;throw;}
  if(!object_navigation_.grabbing()){
    finish_object_grab_locked(true,!state_.polygon);
  }
}
void Document::finish_object_grab_locked(bool accept,bool release){
  if(!state_.object_grabbing)return;
  if(release&&held_polygon_pending_locked())accept=false;
  if(action_active_locked()&&!polygon_accepting_){if(accept){if(end_action_locked())record_stroke_locked();}else cancel_action_locked();}
  if(accept&&held_weight_pending_locked()){object_navigation_.reset();desktop_=false;state_.message="Finishing held weight edit...";publish_locked(false,false);return;}
  if(!accept&&held_weight_pending_locked())cancel_asset_work_locked();
  const auto id=state_.held_object;auto &object=scene_.object(id);const bool changed=!equal_transform(held_preview_transform_,held_start_transform_)||(held_affine_&&held_affine_->preview_edit!=object.affine_edit);
  AuthorityTransition authority(*this);Transaction transaction(*this);
  if(release&&state_.edit_authority==UINT64_MAX)throw std::overflow_error("Object editing authority exhausted");
  if(changed&&accept){auto before=scene_.snapshot();object.transform=held_preview_transform_;if(held_affine_)object.affine_edit=held_affine_->preview_edit;record_locked({{},std::move(before),scene_});}
  state_.object_grabbing=false;held_affine_.reset();desktop_=false;object_navigation_.reset();if(release){state_.held_object=0;++state_.edit_authority;input_.cancel();}
  const bool need_picking=release&&state_.object_grab_mode&&!mesh_.load()->picking;
  publish_locked((changed&&accept)||need_picking,false);
  if(state_.polygon&&!release)rebase_polygon_transform_locked();
  transaction.commit();authority.commit();
}
void Document::end_object_grab(bool accept){release_held_object(accept);}
void Document::object_visibility(uint64_t id,bool visible,AssetSource expected){
  std::lock_guard lock(model_mutex_);
  if(expected.generation && expected.generation!=state_.generation)throw std::logic_error("The document changed; reopen visibility controls");
  auto &o=scene_.object(id);if(o.visible==visible)return;
  AuthorityTransition transition(*this);invalidate_object_authority_locked();
  if(id==state_.held_object && !visible)state_.held_object=0;
  finish_locked();auto before=scene_.snapshot();scene_.object(id).visible=visible;scene_changed_locked(std::move(before));transition.commit();
}
std::optional<uint64_t> Document::pick_object(Vec3 origin,Vec3 direction,double seconds) const {
  if(!std::isfinite(seconds))throw std::invalid_argument("Invalid material pick time");
  std::lock_guard lock(model_mutex_);auto snapshot=mesh_.load();
  if(snapshot->generation!=state_.generation || snapshot->revision!=state_.revision || !snapshot->picking)return {};
  auto hit=snapshot->picking->raycast(origin,direction,100,seconds);if(!hit)return {};
  const size_t corner=size_t(hit->triangle)*3;
  for(const auto &r:snapshot->ranges)if(corner>=r.first&&corner-r.first<r.count)return r.object;
  return {};
}
} // namespace dome
