// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scene.hpp"
namespace dome {
// Worker operation on a detached scene. Samples the displayed/rest-or-posed
// source in object coordinates and creates a separate editable scalar field.
// The original object, paint, layers and deformation remain retained, hidden.
uint64_t create_voxel_sculpt(Scene &, float world_voxel_mm,
                             const std::function<bool()> &cancelled = {});
} // namespace dome
