// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/rig.hpp"

namespace dome::detail {
struct VolumeBindingOptions {
  BindOptions binding;
  float cell_size = .01f;
  // Numerical tests exercise different grid phases; the editor uses cell centres.
  std::array<float,3> grid_phase{.5f,.5f,.5f};
  size_t maximum_grid_points = 8'000'000;
  size_t maximum_bytes = 256u * 1024u * 1024u;
};
struct VolumeBindingReport {
  size_t grid_points = 0, interior_points = 0, volume_components = 0;
  size_t work = 0, peak_planned_bytes = 0;
};
SkinWeights volume_bind_native(const Mesh &, const Skeleton &,
                              const VolumeBindingOptions &,
                              const std::function<bool()> &cancelled = {},
                              VolumeBindingReport * = nullptr);
} // namespace dome::detail
