// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "../include/dome/emission.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <stdexcept>

namespace dome::detail {
// Physical native emission uses linear cd/m^2. This is the existing preserving
// import's strength bound, not an encoding into the ordinary sRGB paint tile.
using dome::maximum_native_emission;
using EmissionRgb = std::array<float, 3>;

inline void validate_emission_rgb(const EmissionRgb &value, float maximum) {
  for (auto v : value)
    if (!std::isfinite(v) || v < 0 || v > maximum)
      throw std::invalid_argument("Invalid linear emission radiance");
}

// Legacy paint is relative to the original typed material's emission strength.
// New physical paint is absolute, including when that strength is zero. They
// cannot share one RGB coefficient without changing old projects or dividing
// by zero. The two coefficients below are unpremultiplied paint values.
using EmissionPaint = PaintEmissionValue;

// Worker-composited, premultiplied replacement terms. Coverage stays float:
// rounding it to UNORM8 can erase a visible HDR source residual near full cover.
// This does not contain/bake any original source image or animated role.
struct EmissionOverlay {
  EmissionRgb legacy{}, physical{};
  float coverage = 0;
  void validate() const {
    if (!std::isfinite(coverage) || coverage < 0 || coverage > 1)
      throw std::invalid_argument("Invalid linear emission coverage");
    validate_emission_rgb(legacy, coverage);
    validate_emission_rgb(physical, maximum_native_emission * coverage);
  }
};

inline EmissionOverlay emission_over(EmissionOverlay below,
                                     const EmissionPaint &above, float opacity) {
  below.validate();
  above.validate();
  if (!std::isfinite(opacity) || opacity < 0 || opacity > 1)
    throw std::invalid_argument("Invalid linear emission opacity");
  const float residual = 1 - opacity;
  for (size_t k = 0; k < 3; ++k) {
    below.legacy[k] = below.legacy[k] * residual + above.legacy[k] * opacity;
    below.physical[k] = below.physical[k] * residual + above.physical[k] * opacity;
  }
  below.coverage = below.coverage * residual + opacity;
  return below;
}

inline EmissionRgb emission_radiance(const EmissionOverlay &paint,
                                    const EmissionRgb &original,
                                    float legacy_strength) {
  paint.validate();
  for (float v : original)
    if (!std::isfinite(v) || v < 0)
      throw std::invalid_argument("Invalid original emission radiance");
  if (!std::isfinite(legacy_strength) || legacy_strength < 0)
    throw std::invalid_argument("Invalid legacy emission strength");
  EmissionRgb result;
  for (size_t k = 0; k < 3; ++k) {
    const double value = double(original[k]) * (1 - paint.coverage) +
                         double(paint.legacy[k]) * legacy_strength +
                         paint.physical[k];
    result[k] = float(value);
    if (!std::isfinite(result[k]))
      throw std::overflow_error("Emission radiance exceeds native float range");
  }
  return result;
}

struct EmissionFactor {
  EmissionRgb factor{};
  float strength = 1;
};

// Standard glTF factor components stay in [0,1]. This helper is only for a
// current constant physical factor; retained authored factor/strength pairs
// must be replayed verbatim instead of being unnecessarily refactored.
inline EmissionFactor emission_factor(const EmissionRgb &radiance) {
  validate_emission_rgb(radiance, maximum_native_emission);
  EmissionFactor out;
  out.strength = std::max({1.f, radiance[0], radiance[1], radiance[2]});
  for (size_t k = 0; k < 3; ++k)
    out.factor[k] = radiance[k] / out.strength;
  return out;
}
} // namespace dome::detail
