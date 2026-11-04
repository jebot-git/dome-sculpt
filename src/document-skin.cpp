// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/humanoid-creator.hpp"
namespace dome {
bool Document::skin_target_ready_locked() const {
  const auto &o = scene_.active();
  return !state_.polygon && !state_.cut && !state_.retopology && !uv_ &&
         !state_.asset_busy && !state_.recovery_busy &&
         !state_.object_grabbing && !action_active_locked() &&
         object_editable_locked(o.id) && o.mesh_active && o.painted;
}
std::vector<uint64_t>
Document::skin_sources_locked(SkinAuthoringMode mode) const {
  std::vector<uint64_t> ids;
  for (const auto &o : scene_.objects())
    if ((mode != SkinAuthoringMode::SourceVisible ||
         (o.group == scene_.active().group && o.appearance &&
          o.appearance->asset && scene_.active().appearance &&
          o.appearance->asset == scene_.active().appearance->asset &&
          !o.appearance->asset.owner_before(
              scene_.active().appearance->asset) &&
          !scene_.active().appearance->asset.owner_before(
              o.appearance->asset) &&
          o.deformation && o.deformation->skin && o.deformation->humanoid)) &&
        (mode != SkinAuthoringMode::Transfer || o.id != scene_.active().id) &&
        o.mesh_active && o.painted && !o.painted->mesh().indices.empty() &&
        o.deformation && o.deformation->skeleton &&
        (mode != SkinAuthoringMode::Transfer || o.deformation->skin))
      ids.push_back(o.id);
  return ids;
}
bool Document::selected_skin_ready_locked(SkinAuthoringMode mode) const {
  if (!skin_target_ready_locked() || mode != SkinAuthoringMode::SourceVisible ||
      state_.shape_sculpt || state_.selected_objects.empty() ||
      state_.selected_objects.size() > 64 ||
      skin_sources_locked(SkinAuthoringMode::SourceVisible).empty())
    return false;
  for (auto id : state_.selected_objects) {
    auto found = std::find_if(scene_.objects().begin(), scene_.objects().end(),
                              [&](const auto &o) { return o.id == id; });
    if (found == scene_.objects().end() ||
        found->group != scene_.active().group || !object_editable_locked(id) ||
        !found->mesh_active || !found->painted || !found->deformation ||
        !found->deformation->skin)
      return false;
  }
  return true;
}
void Document::queue_bind_skin_locked(
    SkinAuthoringOptions options, AssetSource expected, bool selected,
    std::vector<uint64_t> expected_selection) {
  options.validate();
  if (selected && !expected_selection.empty() &&
      expected_selection != state_.selected_objects)
    throw std::logic_error(
        "Selected bind targets changed while choosing options");
  if (options.mode == SkinAuthoringMode::SourceVisible && state_.shape_sculpt)
    throw std::logic_error(
        "Finish shape-key editing before binding REST weights");
  auto ids = skin_sources_locked(options.mode);
  if (!skin_target_ready_locked() ||
      std::find(ids.begin(), ids.end(), options.reference) == ids.end())
    throw std::logic_error(
        "Select an editable mesh and an eligible skeleton/skin reference");
  if (selected && (options.mode != SkinAuthoringMode::SourceVisible ||
                   !selected_skin_ready_locked(options.mode)))
    throw std::logic_error(
        "Select editable parts of the same imported source group");
  const std::vector<uint64_t> targets =
      selected ? state_.selected_objects
               : std::vector<uint64_t>{scene_.active().id};
  for (auto id : targets)
    require_object_editable_locked(id);
  SkinAdmission admission;
  admission.options = options;
  admission.options.locks.clear();
  admission.use_locks = !options.locks.empty();
  if (admission.use_locks)
    admission.locks = options.locks;
  auto changed = std::make_shared<bool>(false);
  auto report = std::make_shared<std::string>();
  const std::string label = options.mode == SkinAuthoringMode::SourceVisible
                                ? (selected ? "Source-assisted selected bind"
                                            : "Source-assisted bind")
                            : options.mode == SkinAuthoringMode::Transfer
                                ? "Transfer skin weights"
                                : "Automatic skin bind";
  queue_scene_operation_locked(
      label,
      [targets, options, changed, report](Scene &scene, const auto &cancelled) {
        if (options.mode == SkinAuthoringMode::SourceVisible) {
          *changed =
              author_source_visible_skin(scene, targets, options, cancelled);
          *report = *changed
                        ? "Source-assisted weights updated; originals retained"
                        : "No skin changes; original weights retained";
        } else {
          *changed = author_skin(scene, targets.front(), options, cancelled);
          *report =
              *changed
                  ? "Skin updated; rest geometry, paint and morphs retained"
                  : "No skin changes; original weights retained";
        }
      },
      expected, report, {}, changed, {}, 0, 0, {}, {}, std::move(admission));
}
void Document::bind_skin(SkinAuthoringOptions options, AssetSource expected) {
  std::lock_guard lock(model_mutex_);
  queue_bind_skin_locked(std::move(options), expected);
}
void Document::bind_skin_selected(SkinAuthoringOptions options,
                                  AssetSource expected,
                                  std::vector<uint64_t> expected_selection) {
  std::lock_guard lock(model_mutex_);
  queue_bind_skin_locked(std::move(options), expected, true,
                         std::move(expected_selection));
}
} // namespace dome
