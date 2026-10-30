// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scene.hpp"
#include "mesh-processing.hpp"
namespace dome {
struct SculptResolutionOptions {
  float voxel_size = .015f; // Object-local metres, independent of object/group scale.
  // Zero selects half the larger input/output voxel; positive explicit metres.
  float maximum_surface_error = 0;
  float minimum_normal_dot = .5f;
  // Independent raw layer reconstruction in metres and protection in [0,1].
  float maximum_layer_error = 0, maximum_mask_error = .1f;
  size_t maximum_samples = 4'000'000, maximum_work = 128'000'000;
  size_t maximum_surface_cells = 50'000, maximum_vertices = 400'000,
         maximum_triangles = 600'000;
  size_t maximum_quality_samples = 1'000'000, maximum_projection_tests = 20'000'000;
  std::function<bool()> cancelled;
};
struct SculptResolutionReport {
  bool changed = false;
  size_t layer_samples = 0, mask_samples = 0, work = 0,
         quality_samples = 0, projection_tests = 0;
  float maximum_surface_bound = 0, minimum_normal_dot = 1,
        maximum_layer_error = 0, maximum_mask_error = 0;
  TopologyReport source, target;
  int64_t source_euler = 0, target_euler = 0;
};
struct SculptResolutionResult {
  // Same resolution is an exact no-op: no replacement, allocations or history.
  std::optional<Volume> replacement;
  SculptResolutionReport report;
};
// Detached, bounded CPU foundation. Each layer's independent displacement and
// protection field is resampled in physical units; layer IDs/properties and
// settings are retained. A persistent physical band avoids changing the layer
// neutral field when voxel spacing changes. Rejects uncertifiable sampled
// geometry/normal/topology quality; it never silently flattens the layer stack.
SculptResolutionResult resample_sculpt(const Volume &, const SculptResolutionOptions & = {});
// Publishes only a fully validated detached candidate, retaining all original
// object/paint/source archives and all other objects. Intended for asset workers.
bool resample_sculpt_object(Scene &, uint64_t object, const SculptResolutionOptions &,
                           SculptResolutionReport * = nullptr);
} // namespace dome
