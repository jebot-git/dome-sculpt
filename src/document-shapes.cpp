// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
namespace dome {
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
} // namespace dome
