// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scene.hpp"
namespace dome {
// Only for freshly extracted sculpt geometry, BEFORE UV/paint/deformation
// authoring. Split numerically inconsistent gradient-normal faces into flat
// corners without moving/dropping any triangle. Authored/imported mesh normals
// never pass through this explicit sculpt preparation step.
size_t condition_sculpt_normals(Mesh &, const std::function<bool()> &cancelled = {});
// Worker operation on a detached scene. Samples the displayed/rest-or-posed
// source in object coordinates and creates a separate editable scalar field.
// The original object, paint, layers and deformation remain retained, hidden.
uint64_t create_voxel_sculpt(Scene &, float world_voxel_mm,
                             const std::function<bool()> &cancelled = {});
} // namespace dome
