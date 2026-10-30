// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/import.hpp"
#include "dome/import-authoring.hpp"
#include "dome/mesh-authoring.hpp"
#include "document-transaction.hpp"
namespace dome {
SceneDraft Document::draft_scene() const {
  std::lock_guard lock(model_mutex_);
  if (state_.cut || state_.retopology || state_.asset_busy || state_.recovery_busy || control_before_ || action_active_locked())
    throw std::logic_error("Finish the current action before opening the asset editor");
  return {scene_.snapshot(), {state_.generation, state_.revision, scene_.active().id,state_.edit_authority}};
}
void Document::edit_scene(std::string label, SceneOperation operation, AssetSource expected) {
  queue_scene_operation(std::move(label), std::move(operation), expected, {});
}
void Document::queue_scene_operation(std::string label, SceneOperation operation,
                                    AssetSource expected, std::shared_ptr<std::string> report) {
  std::lock_guard lock(model_mutex_);
  queue_scene_operation_locked(std::move(label), std::move(operation), expected, std::move(report));
}
void Document::queue_scene_operation_locked(std::string label, SceneOperation operation,
                                      AssetSource expected, std::shared_ptr<std::string> report, std::shared_ptr<AssetWork> work, std::shared_ptr<bool> changed,
                                      std::shared_ptr<const MeshSnapshot> rollback_preview, uint64_t retopo_session, uint64_t cut_session) {
  require_authority_locked(expected);
  if(state_.object_grabbing)throw std::logic_error("Release object motion before starting asset work");
  if(state_.isolate_held && state_.held_object)require_object_editable_locked(scene_.active().id);
  if (label.empty() || label.size() > 256 || label.find('\0') != std::string::npos || !operation)
    throw std::invalid_argument("Invalid asset operation");
  if ((expected.generation && expected.generation != state_.generation) ||
      (expected.revision && expected.revision != state_.revision) ||
      (expected.object && expected.object != scene_.active().id))
    throw std::logic_error("The asset editor source changed; reopen the editor");
  if (state_.asset_busy || state_.recovery_busy || control_before_ || action_active_locked())
    throw std::logic_error("Finish the current action before starting asset work");
  if (state_.retopology && (!retopo_session || retopo_session != retopo_session_.load()))
    throw std::logic_error("Apply or cancel retopology before starting asset work");
  if (state_.cut && (!cut_session || cut_session != cut_session_.load()))
    throw std::logic_error("Apply or cancel the plane cut before starting asset work");
  FileJob job{scene_.snapshot(), {}, state_.revision, state_.generation, content_, false};
  job.object = scene_.active().id;job.edit_authority=state_.edit_authority;job.isolation_target=state_.isolate_held?state_.held_object:0;
  job.scene_operation = std::move(operation);
  job.operation_label = std::move(label);
  job.operation_report = std::move(report);
  job.work = work ? std::move(work) : std::make_shared<AssetWork>();
  job.operation_changed = std::move(changed);
  job.rollback_preview = std::move(rollback_preview);
  job.retopo_session = retopo_session;
  job.cut_session = cut_session;
  std::lock_guard file_lock(file_mutex_);
  if (files_.size() >= 8) throw std::runtime_error("File and asset work queue is full");
  Transaction transaction(*this);
  files_.push_back(std::move(job));
  try {
  asset_work_.store(files_.back().work);
  state_.asset_busy = true;
  state_.error.clear();
  state_.message = files_.back().operation_label + "...";
  publish_locked(false,false);transaction.commit();
  }catch(...){files_.pop_back();throw;}
  file_changed_.notify_one();
}
void Document::remesh_to_sculpt(float world_voxel_mm, AssetSource expected) {
  std::lock_guard lock(model_mutex_);
  queue_voxel_sculpt_locked(world_voxel_mm, expected);
}
void Document::queue_voxel_sculpt_locked(float world_voxel_mm, AssetSource expected) {
  if (!std::isfinite(world_voxel_mm) || world_voxel_mm < .01f || world_voxel_mm > 1000)
    throw std::invalid_argument("Voxel size must be between 0.01 and 1000 mm");
  queue_scene_operation_locked("Voxel remesh", [world_voxel_mm](Scene &scene, const auto &cancelled) {
    create_voxel_sculpt(scene, world_voxel_mm, cancelled);
  }, expected, {});
}
void Document::import_asset(const std::filesystem::path &path, uint32_t resolution) {
  if (resolution < 64 || resolution > 2048 || (resolution & (resolution - 1)))
    throw std::invalid_argument("Invalid paint resolution");
  auto report = std::make_shared<std::string>();
  queue_scene_operation("Import mesh", [path, resolution, report](Scene &scene, const auto &cancelled) {
    ImportAuthoringOptions options;options.paint_resolution=resolution;options.limits.cancelled=cancelled;
    auto prepared=prepare_authoring_import(scene,path,std::move(options));
    for(size_t i=0;i<std::min<size_t>(prepared.diagnostics.size(),8);++i){if(!report->empty())*report+="; ";*report+=prepared.diagnostics[i].substr(0,200);}
    if(prepared.diagnostics.size()>8)*report+="; additional import diagnostics omitted";
    scene=std::move(prepared.scene);
  }, {}, report);
}
} // namespace dome
