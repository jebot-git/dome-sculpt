// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/import.hpp"
#include <cmath>
namespace dome::detail {
inline int imported_texture_coordinate(int base,
                                       const ImportedTextureTransform &t) {
  const auto result = t.authored && t.texcoord >= 0 ? t.texcoord : base;
  if (result < 0 || result > 7)
    throw std::invalid_argument(
        "Imported texture coordinate is outside TEXCOORD_0..7");
  return result;
}
inline std::array<float, 6>
imported_texture_matrix(const ImportedTextureTransform &t) {
  for (float value :
       {t.offset[0], t.offset[1], t.scale[0], t.scale[1], t.rotation})
    if (!std::isfinite(value) || std::abs(value) > 10000)
      throw std::invalid_argument(
          "Imported texture transform exceeds finite native bounds");
  if (t.texcoord < -1 || t.texcoord > 7)
    throw std::invalid_argument(
        "Imported texture coordinate override is invalid");
  if (!t.authored)
    return {1, 0, 0, 1, 0, 0};
  const auto c = std::cos(double(t.rotation)), s = std::sin(double(t.rotation));
  return {float(c * t.scale[0]), float(-s * t.scale[1]),
          float(s * t.scale[0]), float(c * t.scale[1]),
          t.offset[0],           t.offset[1]};
}
} // namespace dome::detail
