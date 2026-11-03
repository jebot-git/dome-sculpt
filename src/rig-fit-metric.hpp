// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/affine.hpp"
#include "dome/rig.hpp"
#include <cmath>
#include <stdexcept>
namespace dome::detail {
// Bone.length measures the REST frame's local Y, not a difference of rounded
// world endpoints. Translation must not affect this physical world distance.
inline float rig_fit_world_length(const AffineTransform &world,
                                  const RigMatrix &rest,float length){
 const auto axis=(world*AffineTransform(rest)).direction({0,length,0});
 const auto result=float(std::hypot(axis[0],axis[1],axis[2]));
 if(!std::isfinite(result)||result<=0)throw std::overflow_error("Bone world length exceeds native metric range");
 return result;
}
} // namespace dome::detail
