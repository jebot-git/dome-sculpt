// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace dome::detail {
// Integral in normalized physical segment parameter, NOT a visibility proof:
//   integral_lo^hi mass/(|head+t*(tail-head)-row|^2+mass) dt.
// A future caller must own the true visible intervals and source
// correspondence. This kernel grants no source/binding authority and changes no
// weight limits.
template <class Budget>
double bone_line_integral(std::array<double, 3> row, std::array<double, 3> head,
                          std::array<double, 3> tail, double mass, double lo,
                          double hi, Budget &work) {
  work.stop();
  work.step(128);
  if (!(mass > 0) || !std::isfinite(mass) || !std::isfinite(lo) ||
      !std::isfinite(hi) || lo < 0 || hi > 1 || lo > hi)
    throw std::invalid_argument("Invalid finite-bone integral domain");
  std::array<double, 3> r, d;
  double a = 0, b = 0, distance2 = 0;
  for (size_t k = 0; k < 3; ++k) {
    if (!std::isfinite(row[k]) || !std::isfinite(head[k]) ||
        !std::isfinite(tail[k]))
      throw std::invalid_argument("Nonfinite bone integral geometry");
    r[k] = head[k] - row[k];
    d[k] = tail[k] - head[k];
    a += d[k] * d[k];
    b += r[k] * d[k];
    distance2 += r[k] * r[k];
  }
  const double width = hi - lo;
  if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(distance2))
    throw std::invalid_argument("Bone integral metric overflow");
  if (!width)
    return 0;
  if (!a) {
    const double denominator = distance2 + mass;
    if (!std::isfinite(denominator))
      throw std::invalid_argument("Point integral metric overflow");
    work.stop();
    return width * (mass / denominator);
  }
  // a*c-b*b is evaluated by the nonnegative cross-product identity to avoid
  // catastrophic cancellation when a row lies near the actual bone line.
  const std::array<double, 3> cross{r[1] * d[2] - r[2] * d[1],
                                    r[2] * d[0] - r[0] * d[2],
                                    r[0] * d[1] - r[1] * d[0]};
  double determinant = a * mass;
  for (double v : cross)
    determinant += v * v;
  if (!(determinant > 0) || !std::isfinite(determinant))
    throw std::invalid_argument("Bone integral precision/range unsupported");
  const double root = std::sqrt(determinant);
  // Difference of arctangents evaluated directly: no subtraction of two
  // almost equal pi/2 values for a distant or very short physical source.
  const double numerator = root * a * width,
               denominator = determinant + (a * lo + b) * (a * hi + b);
  if (!(numerator > 0) || !std::isfinite(numerator) ||
      !std::isfinite(denominator))
    throw std::invalid_argument("Bone integral angle range unsupported");
  const double angle = std::atan2(numerator, denominator);
  const double value = (mass / root) * angle;
  if (!std::isfinite(value) || value < 0)
    throw std::runtime_error("Invalid bounded bone integral result");
  work.stop();
  return value;
}
} // namespace dome::detail
