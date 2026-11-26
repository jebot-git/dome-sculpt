// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace dome::detail {
struct SourceTextureTransform {
  std::array<double, 2> scale, offset;
  double rotation;
};

// Expression targets use absolute signed scales relative to the authored
// rotation. Two decompositions of the same rest matrix can animate differently.
inline SourceTextureTransform decompose_source_texture(const std::array<float,6> &m,
                                                        double rotation) {
  const double cosine=std::cos(rotation),sine=std::sin(rotation);
  const double sx=cosine*m[0]+sine*m[2],sy=-sine*m[1]+cosine*m[3];
  const double scale=std::max({1.,std::abs(sx),std::abs(sy)});
  if(std::abs(cosine*sx-m[0])>1e-6*scale||std::abs(sine*sx-m[2])>1e-6*scale||
     std::abs(-sine*sy-m[1])>1e-6*scale||std::abs(cosine*sy-m[3])>1e-6*scale)
    throw std::invalid_argument("Source texture shear cannot be represented by KHR_texture_transform");
  return {{sx,sy},{m[4],m[5]},rotation};
}

inline SourceTextureTransform decompose_source_texture(const std::array<float,6> &m) {
  const double sx=std::hypot(double(m[0]),double(m[2]));
  const double rotation=sx>1e-15?std::atan2(double(m[2]),double(m[0])):
                                   std::atan2(-double(m[1]),double(m[3]));
  auto transform=decompose_source_texture(m,rotation);
  transform.scale[0]=sx;
  return transform;
}
}
