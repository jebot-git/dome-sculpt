// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake.hpp"
namespace dome {
struct ObjectBakeSettings {
  float front_mm = 50, back_mm = 50, ao_distance_mm = 250;
  uint32_t ao_samples = 16, padding = 8;
};
struct ObjectBakeReport {
  uint64_t projected = 0, padded = 0, rays = 0;
};
// Bake the displayed source into the active target's rest UVs beneath its
// existing editable paint layers. Hidden sources are valid; all source data
// and target geometry/deformation/layers survive. World-millimetre bounds are
// converted to target-local units. Failure leaves the detached scene unchanged.
ObjectBakeReport bake_object(Scene &, uint64_t source, ObjectBakeSettings = {},
                              const std::function<bool()> &cancelled = {});
} // namespace dome
