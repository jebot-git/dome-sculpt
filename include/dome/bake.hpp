// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scene.hpp"
namespace dome {
struct BakeMaterial {
  Material material;
  std::shared_ptr<const TextureMaps> maps;
  std::shared_ptr<const Image> normal_map;
  // PaintedMesh::composite already contains material factors. Imported glTF
  // textures use false, so linear material factors are applied exactly once.
  bool maps_are_composited = true, repeat_uv = false;
  float normal_strength = 1;
};
struct BakeSource {
  Mesh mesh;
  std::vector<std::array<float,4>> colors; // Optional linear glTF vertex RGBA.
  std::vector<std::array<float,4>> tangents; // Optional imported/MikkTSpace XYZ + handedness W.
  std::vector<BakeMaterial> materials{BakeMaterial{}};
  std::vector<uint32_t> triangle_materials; // Empty means material zero.
};
struct BakeOptions {
  uint32_t resolution = 512, padding = 8, ao_samples = 16;
  float front_distance = .05f, back_distance = .05f;
  float ao_distance = .25f, ray_bias = .0001f;
  // Optional cage shares low-mesh vertex identities. Each displacement must
  // face the low normal and fit front_distance. Rays extend back_distance
  // beyond the low surface; no unbounded nearest-surface fallback is used.
  std::vector<Vec3> cage_positions;
  std::vector<std::array<float,4>> low_tangents; // Preserve consumer tangent convention.
  std::vector<uint8_t> writable; // Optional resolution² mask: 0 protected, 1 writable.
  bool allow_backfaces = false, fail_on_miss = true, fail_on_overlap = true;
  bool normal_y_down = false; // Default is glTF/OpenGL +Y tangent convention.
  uint64_t max_texels = 4'194'304, max_rays = 64'000'000, max_raster_samples = 128'000'000;
  std::function<bool()> cancelled;
  std::function<void(float)> progress; // Monotonic detached worker fraction [0,1].
  // Exact topology provenance bypasses projection rays, with a checked source
  // triangle and source barycentrics per low corner. Empty retains ray baking.
  std::vector<uint32_t> source_triangles;
  std::vector<std::array<float,3>> source_corner_barycentrics;
};
struct BakeResult {
  TextureMaps maps; // Composited sRGB base/emission; linear ORM R=AO,G=rough,B=metal.
  Image normal, ambient_occlusion;
  // 0=outside UV, 1=projected hit, 2=padding, 3=missed projection, 4=protected. Missed/protected texels
  // are never filled by padding, so projection errors cannot be concealed.
  std::vector<uint8_t> coverage;
  uint64_t projected = 0, missed = 0, overlapping = 0, padded = 0, rays = 0, raster_samples = 0;
};
// Pure native worker operation. Low, high and cage must already share one
// coordinate system. Inputs are immutable; cancellation/failure returns no
// partial result. Uses the shared SurfaceQuery BVH, never a software viewport
// readback. Low UVs must be finite and inside [0,1]; overlap/miss policy explicit.
BakeResult bake_mesh(const Mesh &low, const BakeSource &, const BakeOptions & = {});
} // namespace dome
