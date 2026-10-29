// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mesh-processing.hpp"
#include <span>
namespace dome {
struct RemeshOptions {
  float target_length = .05f;
  unsigned iterations = 5;
  // Curvature-derived target multiplier 1/(1+adaptivity*angle/pi), min .25.
  float adaptivity = 0, feature_angle_degrees = 45, relax_strength = .3f;
  float maximum_projection_distance = .03f;
  size_t max_vertices = 200'000, max_triangles = 400'000;
  size_t max_operations = 100'000, max_edge_visits = 20'000'000;
  size_t max_projection_triangle_tests = 20'000'000;
  bool split = true, collapse = true, flip = true, relax = true;
  std::function<bool()> cancelled;
};
struct RemeshCornerSource {
  uint32_t triangle = 0;
  std::array<float, 3> barycentric{};
};
struct RemeshResult {
  Mesh mesh;
  std::vector<uint32_t> materials;                // Per triangle.
  std::vector<RemeshCornerSource> source_corners; // Per mesh index.
  size_t splits = 0, collapses = 0, flips = 0, relaxed_vertices = 0;
};
// Native CPU split/collapse/valence-flip/projected-relax reference kernel.
// Immutable source charts prevent crossing disconnected surfaces, UV seams,
// sharp creases or material borders. Pins and protected vertices stay fixed;
// protected edges may be split canonically. Source provenance supports explicit
// attribute/paint transfer; this function never transfers existing paint
// pixels. Input targets (optional) are absolute lengths per input vertex,
// overriding automatic curvature targets. Surface self-intersections are not
// repaired.
RemeshResult remesh_triangles(const Mesh &, const RemeshOptions & = {},
                              std::span<const uint32_t> materials = {},
                              std::span<const float> target_lengths = {});
} // namespace dome
