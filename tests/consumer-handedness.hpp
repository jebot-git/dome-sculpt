// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cmath>
#include <stdexcept>
namespace consumer_frame {
// Independent parsed-file consumer math, not a production placement helper.
inline float handedness(const std::array<float,16> &m){
 double a[3][3]{};
 for(size_t c=0;c<3;++c){const double scale=std::hypot(double(m[c*4]),double(m[c*4+1]),double(m[c*4+2]));if(!std::isfinite(scale)||scale==0)throw std::invalid_argument("Consumer frame is singular");for(size_t r=0;r<3;++r)a[c][r]=m[c*4+r]/scale;}
 const double determinant=a[0][0]*(a[1][1]*a[2][2]-a[2][1]*a[1][2])-a[1][0]*(a[0][1]*a[2][2]-a[2][1]*a[0][2])+a[2][0]*(a[0][1]*a[1][2]-a[1][1]*a[0][2]);
 if(!std::isfinite(determinant)||std::abs(determinant)<=1e-12)throw std::invalid_argument("Consumer frame is singular");
 return determinant<0?-1.f:1.f;
}
}
