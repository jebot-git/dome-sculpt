// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "volume.hpp"
#include <array>
namespace dome {
struct Mesh {
  std::vector<Vec3> positions, normals;
  std::vector<std::array<float, 2>> uv;
  std::vector<uint32_t> indices;
  void validate() const;
};
Mesh surface_mesh(const Volume &);
struct AtlasInfo {
  uint32_t width = 0, height = 0, charts = 0;
  float utilization = 0;
};
AtlasInfo unwrap(Mesh &, uint32_t resolution = 1024, uint32_t padding = 4);
void export_glb(const Mesh &, const std::filesystem::path &);
} // namespace dome
