// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/volume.hpp"
namespace dome::sculpt_detail {
std::optional<Vec3> raycast_field(const Volume &,Vec3,Vec3,float);
std::optional<Vec3> raycast_field(const Volume &,Vec3,Vec3,float,const std::function<bool()> &);
}
