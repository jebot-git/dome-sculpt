// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
namespace dome {
// Match the complete native baker domain. A small linear neighbourhood at zero
// keeps zero/tiny saved bounds editable while retaining logarithmic precision.
inline constexpr double bake_distance_max_mm=100000;
inline constexpr double bake_distance_pivot_mm=.01;
inline double bake_distance_from_unit(double t){
 if(t<=0)return 0;if(t>=1)return bake_distance_max_mm;
 return bake_distance_pivot_mm*std::expm1(t*std::log1p(bake_distance_max_mm/bake_distance_pivot_mm));
}
inline double bake_distance_to_unit(double mm){
 if(mm<=0)return 0;if(mm>=bake_distance_max_mm)return 1;
 return std::log1p(mm/bake_distance_pivot_mm)/std::log1p(bake_distance_max_mm/bake_distance_pivot_mm);
}
inline double bake_distance_step(double mm,double exponent){
 return std::clamp((mm+bake_distance_pivot_mm)*std::exp(exponent)-bake_distance_pivot_mm,0.,bake_distance_max_mm);
}
} // namespace dome
