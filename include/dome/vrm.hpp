// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scene.hpp"
#include <map>
#include <nlohmann/json.hpp>
namespace dome {
// VRM0 indices never migrate by names or by assuming output array order.
struct Vrm0IdentityMap {
  std::vector<int> nodes, textures;
  std::vector<std::vector<int>> meshes;
  // Emitted mesh -> original source morph slot -> native/output morph slot.
  std::map<int, std::vector<int>> morph_targets;
};
class Vrm0Metadata {
  nlohmann::json source_;

public:
  static Vrm0Metadata parse(const nlohmann::json &, size_t nodes, size_t meshes,
                            size_t textures, size_t materials);
  const nlohmann::json &source() const noexcept { return source_; }
  nlohmann::json remap(const Vrm0IdentityMap &) const;
};
struct VrmExportOptions {
  std::function<bool()> cancelled;
  size_t max_output_bytes = 256 * 1024 * 1024;
  // Zero preserves original main/shade dimensions. A nonzero value is an
  // explicit resampling choice for compiled paint maps, never a hidden fallback.
  uint32_t paint_resolution = 0;
  std::function<void()> before_commit;
};
struct VrmExportReport {
  size_t meshes = 0, joints = 0, normal_targets_recomputed = 0;
  std::vector<std::string> diagnostics;
  size_t materials = 0, painted_materials = 0;
};
// Source-version-preserving VRM0 only. Requires retained source identities;
// unsupported versions/features reject rather than being silently converted.
VrmExportReport export_vrm(const std::vector<ObjectSurface> &,
                           const std::filesystem::path &,
                           const VrmExportOptions & = {});
} // namespace dome
