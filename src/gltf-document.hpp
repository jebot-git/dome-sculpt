// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include <nlohmann/json.hpp>
namespace dome::detail {
struct GltfObjectIdentity {
  int node=-1, mesh=-1, material=-1;
  std::vector<uint32_t> joints;
};
struct GltfDocument {
  nlohmann::json json;
  std::vector<uint8_t> binary;
  std::vector<GltfObjectIdentity> objects;
};
GltfDocument make_gltf(const std::vector<ObjectSurface>&);
void write_glb(GltfDocument, const std::filesystem::path&);
}
