// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake.hpp"
#include "bake-settings.hpp"
namespace dome {
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
