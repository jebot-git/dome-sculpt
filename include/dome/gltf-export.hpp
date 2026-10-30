// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scene.hpp"
namespace dome {
struct GltfExportOptions {
  bool recompute_changed_normals = false;
  std::function<bool()> cancelled;
  size_t max_output_bytes = 256 * 1024 * 1024;
  // Optional cancellation/commit arbitration, after durable temporary output.
  std::function<void()> before_commit;
};
struct GltfExportReport {
  size_t meshes = 0, normal_targets_recomputed = 0;
  std::vector<std::string> diagnostics;
};
// Additive opt-in/reporting API. Existing two-argument wrappers stay strict.
// Unsupported tangent recomputation always rejects rather than dropping data.
GltfExportReport export_glb(const std::vector<ObjectSurface> &,
                            const std::filesystem::path &,
                            const GltfExportOptions &);
} // namespace dome
