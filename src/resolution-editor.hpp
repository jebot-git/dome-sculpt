// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/sculpt-resolution.hpp"
namespace dome {
std::optional<SculptResolutionOptions> sculpt_resolution_dialog(float current_voxel,float world_scale,SculptResolutionOptions);
}
