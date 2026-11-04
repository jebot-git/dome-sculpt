// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bone-source-shadow.hpp"
#include <cfenv>
#include <cfloat>
namespace dome::detail::bone_shadow {
// First-stage orient3d error bound from Shewchuk's public-domain predicates.c
// (May18,1996), official source SHA
// f8662c3f407d1c1c5dcd4dd49ea8b8ddd801a71827d65734206ddc3741792029. Same
// bounded binary32 geometry as the exact control. Volatile intermediates
// enforce the binary64 operations underlying that bound (no FMA contraction).
// Unknown arithmetic/sign always goes to the original exact predicate.
template <class Budget>
int filtered_orientation(Point a, Point b, Point c, Point d, Budget &w) {
#if defined(__FAST_MATH__) || FLT_EVAL_METHOD != 0
  (void)a;
  (void)b;
  (void)c;
  (void)d;
  (void)w;
  return 0;
#else
  static_assert(std::numeric_limits<double>::is_iec559 &&
                std::numeric_limits<double>::digits == 53);
  w.step(64);
  if (std::fegetround() != FE_TONEAREST)
    return 0;
  double x[3], y[3], z[3];
  for (size_t k = 0; k < 3; ++k) {
    x[k] = double(b[k]) - a[k];
    y[k] = double(c[k]) - a[k];
    z[k] = double(d[k]) - a[k];
  }
  volatile double p0 = x[0] * y[1], p1 = y[0] * x[1], p2 = y[0] * x[2],
                  p3 = x[0] * y[2], p4 = x[1] * y[2], p5 = y[1] * x[2];
  volatile double t0 = z[2] * (p0 - p1), t1 = z[1] * (p2 - p3),
                  t2 = z[0] * (p4 - p5), partial = t0 + t1;
  const double value = partial + t2;
  volatile double q0 = (std::abs(p0) + std::abs(p1)) * std::abs(z[2]),
                  q1 = (std::abs(p2) + std::abs(p3)) * std::abs(z[1]),
                  q2 = (std::abs(p4) + std::abs(p5)) * std::abs(z[0]),
                  sum = q0 + q1;
  const double permanent = sum + q2;
  constexpr double unit = 0x1p-53, bound = (7. + 56. * unit) * unit;
  if (!std::isfinite(value) || !std::isfinite(permanent))
    return 0;
  const double error = bound * permanent;
  if (value > error)
    return 1;
  if (-value > error)
    return -1;
  return 0;
#endif
}
struct FilterReport {
  size_t exact = 0, rejected = 0, full = 0;
};
template <class Budget>
Result filtered_triangle_shadow(Point row, Point head, Point tail,
                                const Triangle &face, Budget &w,
                                FilterReport &report) {
  w.stop();
  // This filter never bypasses ambiguous row-on-plane/origin ownership.
  const int side = filtered_orientation(face[0], face[1], face[2], row, w);
  if (side < 0) {
    ++report.rejected;
    return {};
  }
  if (side > 0) {
    int a = filtered_orientation(face[0], face[1], face[2], head, w),
        b = head == tail
                ? a
                : filtered_orientation(face[0], face[1], face[2], tail, w);
    if (a > 0 && b > 0) {
      ++report.rejected;
      return {};
    }
    bool full = a < 0 && b < 0;
    for (size_t c = 0; c < 3; ++c) {
      a = filtered_orientation(row, head, face[c], face[(c + 1) % 3], w);
      b = head == tail
              ? a
              : filtered_orientation(row, tail, face[c], face[(c + 1) % 3], w);
      if (a > 0 && b > 0) {
        ++report.rejected;
        return {};
      }
      full &= a < 0 && b < 0;
    }
    if (full) {
      volume_exact::Arithmetic exact(w);
      ++report.full;
      return {Kind::Interval, {exact.zero(), exact.one()}};
    }
  }
  ++report.exact;
  return triangle_shadow(row, head, tail, face, w);
}
} // namespace dome::detail::bone_shadow
