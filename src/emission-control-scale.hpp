// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/vr-controls.hpp"
#include <algorithm>
#include <cmath>
namespace dome::detail {
// Actual values remain linear radiance. This presentation curve provides an
// exact zero endpoint and useful travel at low radiance as well as at 10,000.
inline bool emission_control_scale(const VrControlState &c) {
  return c.maximum>1 && (c.parameter==VrParameter::InkEmission || c.parameter==VrParameter::MaterialEmission);
}
inline float emission_control_fraction(float value,float maximum) {
  constexpr double knee=.001;
  return float(std::log1p(std::clamp(double(value),0.0,double(maximum))/knee)/std::log1p(double(maximum)/knee));
}
inline float emission_control_value(float fraction,float maximum) {
  constexpr double knee=.001;
  if(fraction<=0)return 0;
  if(fraction>=1)return maximum;
  return float(knee*std::expm1(double(fraction)*std::log1p(double(maximum)/knee)));
}
}
