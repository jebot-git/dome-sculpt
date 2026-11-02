// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/source-placement.hpp"
#include "gltf-document.hpp"
namespace dome::detail {
struct VrmSourceGraph {
  std::shared_ptr<const CreatorSourceModel> model;
  uint64_t instance = 0;
  std::vector<SceneGroup> prefix;
  std::vector<ImportedMatrix> current_local;
  std::vector<nlohmann::json> original_nodes;
  std::vector<std::optional<std::string>> names;
  nlohmann::json humanoid;
};
// Explicitly authenticated original-space route; conventional creator exports
// keep their existing transport. No source graph is inferred from names.
VrmSourceGraph vrm_source_graph(const std::vector<ObjectSurface> &,
                                const nlohmann::json &,
                                const std::function<bool()> &);
// Install one actual original-node/current-pose graph and keep each emitted
// skin's distinct IBM accessor. Returns exact original-node→emitted-node IDs.
std::vector<int> emit_vrm_source_graph(GltfDocument &,
                                       const std::vector<ObjectSurface> &,
                                       const VrmSourceGraph &,
                                       const std::function<bool()> &);
} // namespace dome::detail
