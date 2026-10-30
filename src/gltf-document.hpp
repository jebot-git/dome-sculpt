// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "source-morph.hpp"
#include <nlohmann/json.hpp>
namespace dome::detail {
struct GltfObjectIdentity {
  int node = -1, mesh = -1, material = -1;
  std::vector<uint32_t> joints;
  PaintSourceTexture canonical_normal,prepared_normal;
};
struct GltfDocument {
  nlohmann::json json;
  std::vector<uint8_t> binary;
  std::vector<GltfObjectIdentity> objects;
  size_t normal_targets_recomputed = 0;
};
GltfDocument make_gltf(const std::vector<ObjectSurface> &,
                       SourceMorphSpace = SourceMorphSpace::Native,
                       bool recompute_changed_normals = false,
                       std::function<bool()> cancelled = {});
void write_glb(GltfDocument, const std::filesystem::path &);
void write_glb(GltfDocument, const std::filesystem::path &,
               const std::function<bool()> &cancelled,
               const std::function<void()> &before_commit);
} // namespace dome::detail
