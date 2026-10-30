// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake.hpp"
namespace dome {
struct ObjectBakeSettings {
  float front_mm = 50, back_mm = 50, ao_distance_mm = 250;
  uint32_t ao_samples = 16, padding = 8;
  bool allow_partial = false, allow_backfaces = false;
  bool prepare_colors = false;
  float color_error = .04f;
  bool prepare_normals=false;
  float normal_error=.02f;
};
struct ObjectBakeReport {
  uint64_t projected = 0, padded = 0, rays = 0, missed = 0, preserved_misses = 0;
  bool color_prepared = false;
  bool canonical_captured = false;
  std::array<float,4> color_error_bound{};
  bool normal_prepared=false;
  double normal_initial_error=0;
};
// Bake the displayed source into the active target's rest UVs beneath its
// existing editable paint layers. Hidden sources are valid; all source data
// and target geometry/deformation/layers survive. World-millimetre bounds are
// converted to target-local units. Failure leaves the detached scene unchanged.
ObjectBakeReport bake_object(Scene &, uint64_t source, ObjectBakeSettings = {},
                              const std::function<bool()> &cancelled = {},
                              const std::function<void(float)> &progress = {});
} // namespace dome
