// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
namespace dome {
// Signed zero-centred logarithmic control in object-local metres. Uniform
// slider space retains useful fine control close to zero and spans ±100 m.
inline float sculpt_plane_metres(float normalized) {
  double signed_value=2*std::clamp(double(normalized),0.,1.)-1;
  return float(std::copysign(.0001*std::expm1(std::abs(signed_value)*std::log1p(100/.0001)),signed_value));
}
inline float sculpt_plane_slider(float metres) {
  double value=std::clamp(double(metres),-100.,100.);
  return float(.5+.5*std::copysign(std::log1p(std::abs(value)/.0001)/std::log1p(100/.0001),value));
}
}
