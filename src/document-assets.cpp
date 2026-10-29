// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/import.hpp"
#include "dome/mesh-authoring.hpp"
namespace dome {
SceneDraft Document::draft_scene() const {
  std::lock_guard lock(model_mutex_);
  if (state_.asset_busy || state_.recovery_busy || control_before_ || action_active_locked())
    throw std::logic_error("Finish the current action before opening the asset editor");
  return {scene_.snapshot(), {state_.generation, state_.revision, scene_.active().id}};
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
                                      AssetSource expected, std::shared_ptr<std::string> report, std::shared_ptr<bool> changed) {
  if (label.empty() || label.size() > 256 || label.find('\0') != std::string::npos || !operation)
    throw std::invalid_argument("Invalid asset operation");
  if ((expected.generation && expected.generation != state_.generation) ||
      (expected.revision && expected.revision != state_.revision) ||
      (expected.object && expected.object != scene_.active().id))
    throw std::logic_error("The asset editor source changed; reopen the editor");
  if (state_.asset_busy || state_.recovery_busy || control_before_ || action_active_locked())
    throw std::logic_error("Finish the current action before starting asset work");
  publish_locked();
  FileJob job{scene_.snapshot(), {}, state_.revision, state_.generation, content_, false};
  job.object = scene_.active().id;
  job.scene_operation = std::move(operation);
  job.operation_label = std::move(label);
  job.operation_report = std::move(report);
  job.operation_changed = std::move(changed);
  std::lock_guard file_lock(file_mutex_);
  if (files_.size() >= 8) throw std::runtime_error("File and asset work queue is full");
  files_.push_back(std::move(job));
  state_.asset_busy = true;
  state_.error.clear();
  state_.message = files_.back().operation_label + "...";
  publish_locked();
  file_changed_.notify_one();
}
void Document::remesh_to_sculpt(float world_voxel_mm, AssetSource expected) {
  std::lock_guard lock(model_mutex_);
  queue_voxel_sculpt_locked(world_voxel_mm, expected);
}
void Document::bake_from_object(uint64_t source, ObjectBakeSettings settings, AssetSource expected) {
  auto report = std::make_shared<std::string>();
  queue_scene_operation("Bake surface", [source, settings, report](Scene &scene, const auto &cancelled) {
    auto result = bake_object(scene, source, settings, cancelled);
    *report = std::to_string(result.projected) + " texels, " +
              std::to_string(result.padded) + " padded, " +
              std::to_string(result.rays) + " rays";
  }, expected, report);
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
    ImportLimits limits;
    size_t vertices = 0, triangles = 0;
    for (auto &o : scene.objects()) if (o.painted) {
      vertices += o.painted->mesh().positions.size();
      triangles += o.painted->mesh().indices.size() / 3;
    }
    if (vertices >= limits.max_vertices || triangles >= limits.max_triangles)
      throw std::length_error("Scene mesh budget exhausted");
    limits.max_vertices -= vertices;
    limits.max_triangles -= triangles;
    limits.cancelled = cancelled;
    auto asset = import_mesh(path, limits);
    for (size_t i = 0; i < std::min<size_t>(asset.warnings.size(), 8); ++i) {
      if (!report->empty()) *report += "; ";
      *report += asset.warnings[i].substr(0, 200);
    }
    if (asset.warnings.size() > 8) *report += "; additional import diagnostics omitted";
    if (asset.primitives.size() > 256 - scene.objects().size())
      throw std::length_error("Import exceeds the scene object limit");
    for (auto &primitive : asset.primitives) {
      if (cancelled()) throw std::runtime_error("Import cancelled");
      const auto &m = asset.materials.at(primitive.material);
      // Authoring currently stores UV0 PBR scalar materials. Refuse data that
      // cannot round-trip yet, rather than stripping source attributes/maps.
      if (!primitive.colors.empty() || !primitive.tangents.empty() || !primitive.uv1.empty())
        throw std::runtime_error("Authoring import of vertex colors, authored tangents or UV1 is not available yet; source data was not changed");
      if (m.color_texture >= 0 || m.metal_rough_texture >= 0 || m.normal_texture >= 0 ||
          m.occlusion_texture >= 0 || m.emissive_texture >= 0)
        throw std::runtime_error("Textured material authoring import is not available yet; source data was not changed");
      if (m.alpha == ImportedMaterial::AlphaMode::Mask)
        throw std::runtime_error("Alpha-mask material import is not available yet");
      Material material;
      material.name = m.name;
      material.color = m.color;
      if (m.alpha == ImportedMaterial::AlphaMode::Opaque) material.color[3] = 1;
      material.metallic = m.metallic;
      material.roughness = m.roughness;
      material.emissive = m.emissive;
      material.double_sided = m.double_sided;
      material.validate();
      if (primitive.mesh.uv.empty()) unwrap(primitive.mesh, resolution);
      auto painted = std::make_shared<PaintedMesh>(std::move(primitive.mesh), resolution);
      auto id = scene.add(primitive.name.empty() ? "Imported mesh" : primitive.name, Volume{});
      auto &object = scene.object(id);
      object.material = std::move(material);
      object.painted = std::move(painted);
      object.mesh_active = true;
    }
  }, {}, report);
}
} // namespace dome
