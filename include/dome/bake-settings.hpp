// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
namespace dome {
struct ObjectBakeSettings {
  float front_mm = 50, back_mm = 50, ao_distance_mm = 250;
  uint32_t ao_samples = 16, padding = 8;
  bool allow_partial = false, allow_backfaces = false;
  bool prepare_colors = false;
  float color_error = .04f;
  bool prepare_normals=false;
  float normal_error=.02f;
  // Explicit existing mesh with the target's ordered vertex/index identities.
  // Zero uses the ordinary normal-offset projection. Session configuration;
  // cage geometry is an ordinary editable/persistent scene mesh.
  uint64_t cage_object=0;
};
} // namespace dome
