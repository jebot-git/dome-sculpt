// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/volume.hpp"
namespace dome::test {
inline Volume dense_ray_corridor() {
  Volume::Voxels values;
  // Each traversed cell has a genuinely low corner at x=y=1. The axial ray
  // x=y=0 has exactly zero weight for that corner and stays positive. Thus a
  // >20000-cell candidate budget is exhausted by geometry, never empty space.
  for (int z = -12000; z <= 12000; ++z)
    values[{1, 1, z}] = -.00002f;
  return Volume::from_samples(.00001f, std::move(values));
}
} // namespace dome::test
