// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
namespace dome {
bool Document::sculpt_target_ready_locked() const {
  const auto &o=scene_.active();
  return !state_.cut && !state_.retopology && !uv_ && !state_.asset_busy && !state_.recovery_busy &&
      !control_before_ && !action_active_locked() && !o.mesh_active && o.visible && !o.locked;
}
bool Document::resolution_target_ready_locked() const {
  const auto scale=scene_.world_transform(scene_.active().id).scale;
  return sculpt_target_ready_locked() && std::isfinite(scale) && scale>0 &&
      std::isfinite(scale*1000) && scale*.01f>0;
}
void Document::apply_procedural_sculpt(ProceduralSculptOptions options,AssetSource expected) {
  std::lock_guard lock(model_mutex_);queue_procedural_locked(std::move(options),expected);
}
void Document::queue_procedural_locked(ProceduralSculptOptions options,AssetSource expected) {
  options.validate();
  if(!resolution_target_ready_locked())throw std::logic_error("Select an unlocked visible sculpt source for procedural detail");
  options.cancelled={};options.progress={};
  auto work=std::make_shared<AssetWork>();auto changed=std::make_shared<bool>(false);
  auto report=std::make_shared<std::string>();const auto object=scene_.active().id;
  queue_scene_operation_locked("Procedural sculpt detail",
    [options=std::move(options),object,work,changed,report](Scene &scene,const auto &cancelled) mutable {
      options.cancelled=cancelled;options.progress=[work](float value){work->progress.store(value);};
      ProceduralSculptReport result;*changed=procedural_sculpt_object(scene,object,options,&result);
      *report=std::to_string(result.primitives)+" details, "+std::to_string(result.changed_samples)+" changed samples";
    },expected,report,work,changed);
}
void Document::queue_resolution_locked(SculptResolutionOptions options,AssetSource expected) {
  options.validate();
  if(!resolution_target_ready_locked())throw std::logic_error("Select an unlocked visible sculpt source to change resolution");
  if((expected.generation && expected.generation!=state_.generation) ||
     (expected.revision && expected.revision!=state_.revision) ||
     (expected.object && expected.object!=scene_.active().id))
    throw std::logic_error("Sculpt target changed; reopen resolution controls");
  if(options.voxel_size==volume().voxel_size())return;
  options.cancelled={};options.progress={};
  auto work=std::make_shared<AssetWork>();auto changed=std::make_shared<bool>(false);
  auto report=std::make_shared<std::string>();const auto object=scene_.active().id;
  queue_scene_operation_locked("Change sculpt resolution",
      [options=std::move(options),work,changed,report,object](Scene &scene,const auto &cancelled) mutable {
        options.cancelled=cancelled;options.progress=[work](float value){work->progress.store(value);};
        SculptResolutionReport result;*changed=resample_sculpt_object(scene,object,options,&result);
        *report=std::to_string(result.layer_samples)+" layer samples, "+std::to_string(result.mask_samples)+" mask samples";
      },expected,report,work,changed);
}
void Document::change_sculpt_resolution(SculptResolutionOptions options,AssetSource expected) {
  std::lock_guard lock(model_mutex_);queue_resolution_locked(std::move(options),expected);
}
void Document::sculpt_settings_locked(Volume::SculptSettings settings, AssetSource expected) {
  if (!sculpt_target_ready_locked()) throw std::logic_error("Sculpt mask and symmetry controls are unavailable");
  if ((expected.generation && expected.generation!=state_.generation) ||
      (expected.revision && expected.revision!=state_.revision) ||
      (expected.object && expected.object!=scene_.active().id))
    throw std::logic_error("Sculpt target changed; reopen the controls");
  if (volume().sculpt_settings(settings)) { record_stroke_locked(); publish_locked(); }
}
void Document::sculpt_settings(Volume::SculptSettings settings, AssetSource expected) {
  std::lock_guard lock(model_mutex_); sculpt_settings_locked(settings,expected);
}
void Document::queue_sculpt_mask_locked(std::optional<float> fill, AssetSource expected) {
  if (!sculpt_target_ready_locked()) throw std::logic_error("Sculpt mask operations are unavailable");
  if(fill && (!std::isfinite(*fill) || *fill<0 || *fill>1)) throw std::invalid_argument("Invalid mask fill");
  const auto object=scene_.active().id;
  auto changed=std::make_shared<bool>(false);
  auto work=std::make_shared<AssetWork>();
  queue_scene_operation_locked(fill ? "Fill sculpt mask" : "Invert sculpt mask",
      [object,fill,changed](Scene &scene,const auto &cancelled) {
        auto &volume=*scene.object(object).volume;
        *changed=fill?volume.fill_mask(*fill,cancelled):volume.invert_mask(cancelled);
      },expected,{},work,changed);
}
void Document::sculpt_mask_fill(float value, AssetSource expected) {
  std::lock_guard lock(model_mutex_); queue_sculpt_mask_locked(value,expected);
}
void Document::sculpt_mask_invert(AssetSource expected) {
  std::lock_guard lock(model_mutex_); queue_sculpt_mask_locked({},expected);
}
} // namespace dome
