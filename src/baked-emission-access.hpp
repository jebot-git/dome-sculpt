// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/paint.hpp"
#include <cmath>
#include <stdexcept>

namespace dome::detail {
// This is the sole old-base promotion rule. It retains the old legacy-relative
// coefficient; it does not resolve it with any newly supplied material factor.
inline BakedEmissionSample baked_emission_texel(const BakedTextures &baked, size_t at) {
  if (at >= baked.coverage.size()) throw std::out_of_range("Baked emission texel");
  if (baked.emission_field) return baked.emission_field->values()[at];
  BakedEmissionSample result;
  result.coverage = baked.coverage[at]/255.f;
  for (size_t k = 0; k < 3; ++k) {
    const float x = baked.maps.emission.rgba.at(at*4+k)/255.f;
    result.legacy[k] = (x<=.04045f ? x/12.92f : std::pow((x+.055f)/1.055f,2.4f))*result.coverage;
  }
  return result;
}
} // namespace dome::detail
