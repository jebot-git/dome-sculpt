// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
namespace dome {
bool Document::sculpt_target_ready_locked() const {
  const auto &o=scene_.active();
  return !state_.retopology && !uv_ && !state_.asset_busy && !state_.recovery_busy &&
      !control_before_ && !action_active_locked() && !o.mesh_active && o.visible && !o.locked;
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
