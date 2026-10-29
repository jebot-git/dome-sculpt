// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mesh.hpp"
#include <span>
namespace dome {
struct UvSolveOptions {
  // A seed selects its entire connected UV chart; empty selects every chart.
  std::vector<uint32_t> seed_faces;
  bool preserve_existing_cuts = true;
  uint32_t maximum_iterations = 2000;
  double relative_tolerance = 1e-9;
  size_t maximum_vertices = 200'000, maximum_triangles = 400'000;
  size_t maximum_work = 200'000'000, maximum_bytes = 256 * 1024 * 1024;
};
struct UvChartSolve {
  uint32_t first_face = 0, faces = 0, vertices = 0, hard_pins = 0;
  uint32_t automatic_pins = 0, iterations = 0;
  double relative_residual = 0;
};
struct UvSolveResult {
  Mesh mesh;
  uint64_t source_topology = 0;
  // Complete source identity: output vertex -> original input vertex. Geometry,
  // normals and pin bytes are copied exactly; no source vertices are merged.
  std::vector<uint32_t> source_vertices;
  // Original face-corner order -> output vertex; equals mesh.indices.
  std::vector<uint32_t> corner_vertices;
  std::vector<UvChartSolve> charts;
};
struct UvStitchEdge {
  // Explicit outgoing face-corner edges, not arbitrary vertex guesses.
  uint32_t a = 0, b = 0;
};
// Detached worker-only matrix-free least-squares conformal solve. Hard pins are
// eliminated exactly; independent charts get deterministic automatic anchors.
// Throws actionable seam/manifold/rank/convergence/resource diagnostics, never
// returns a partial solve. Layout packing is a separate operation.
UvSolveResult conformal_uv(const Mesh &, const UvSolveOptions & = {},
                           const std::function<bool()> &cancelled = {});
// Join only explicitly paired matching boundary/seam edges, clear those seam
// flags, preserve compatible hard pins, then conformally solve joined charts.
// Finite tolerance applies only to explicit pairs; there is no global weld.
UvSolveResult stitch_uv(const Mesh &, std::span<const UvStitchEdge>,
                        double geometric_tolerance = 1e-6,
                        const UvSolveOptions & = {},
                        const std::function<bool()> &cancelled = {});
} // namespace dome
