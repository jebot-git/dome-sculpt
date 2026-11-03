// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cmath>
#include <stdexcept>
namespace dome {
inline constexpr float maximum_native_emission = 10000;
enum class PaintEmissionUnits { MaterialRelative, Physical };
// Unpremultiplied linear contributions. MaterialRelative is the historical
// painted value, multiplied by the authored MToon strength when applicable.
// Physical is absolute cd/m^2 and remains independent of that strength.
struct PaintEmissionValue {
  std::array<float,3> legacy{},physical{};
  // Physical strokes retain the actual linear coverage. The common UNORM8
  // coverage remains the unchanged legacy plane, not the HDR authority.
  float coverage=0;
  void validate() const {
    if(!std::isfinite(coverage)||coverage<0||coverage>1)
      throw std::invalid_argument("Invalid physical emission coverage");
    for(float v:legacy)if(!std::isfinite(v)||v<0||v>1)
      throw std::invalid_argument("Invalid material-relative emission value");
    for(float v:physical)if(!std::isfinite(v)||v<0||v>maximum_native_emission)
      throw std::invalid_argument("Invalid physical emission value");
  }
  bool operator==(const PaintEmissionValue &) const = default;
};
} // namespace dome
