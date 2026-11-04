// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "volume-exact.hpp"
namespace dome::detail::bone_shadow {
using Point = std::array<float, 3>;
using Triangle = std::array<Point, 3>;
enum class Kind { None, Interval, OriginContact, CoplanarContact, Grazing };
struct Result {
  Kind kind = Kind::None;
  volume_exact::Interval interval;
};
// Exact orientation magnitude, using the same bounded dyadic arithmetic as
// the audited volume predicates. This does not claim volume membership.
template <class Arithmetic>
volume_exact::Integer determinant(Point a, Point b, Point c, Point d,
                                  Arithmetic &exact) {
  std::array<volume_exact::Integer, 3> x, y, z;
  for (size_t k = 0; k < 3; ++k) {
    auto origin = exact.scalar(a[k]);
    x[k] = exact.subtract(exact.scalar(b[k]), origin);
    y[k] = exact.subtract(exact.scalar(c[k]), origin);
    z[k] = exact.subtract(exact.scalar(d[k]), origin);
  }
  auto term = [&](size_t i, size_t j, size_t k) {
    return exact.multiply(x[i], exact.subtract(exact.multiply(y[j], z[k]),
                                               exact.multiply(y[k], z[j])));
  };
  return exact.add(exact.add(term(0, 1, 2), term(1, 2, 0)), term(2, 0, 1));
}
template <class Arithmetic>
volume_exact::Fraction ratio(volume_exact::Integer n, volume_exact::Integer d,
                             Arithmetic &exact) {
  // A 3D determinant has a common dyadic scale. Cancel it in the ratio before
  // cross-products; retaining that irrelevant scale would overflow 1024 bits.
  const auto common = n.sign ? std::min(n.shift, d.shift) : d.shift;
  if (n.sign)
    n.shift -= common;
  d.shift -= common;
  return exact.fraction(n, d);
}
// Entry-wall shadows of a whole genuine finite bone, not endpoint/midpoint
// sampling. With row in front of the oriented triangle, source parameter t is
// occluded exactly when four affine determinants are <=0: source behind the
// plane and all three ray/edge barycentric determinants. Their intersection
// is a rational interval. Structural incident-face exclusion belongs to the
// owning caller, not position equality or this geometry-only kernel.
template <class Budget>
Result triangle_shadow(Point row, Point head, Point tail, const Triangle &face,
                       Budget &work) {
  work.stop();
  volume_exact::Arithmetic exact(work);
  for (size_t k = 0; k < 3; ++k) {
    work.step(8);
    const auto lo = std::min({row[k], head[k], tail[k]}),
               hi = std::max({row[k], head[k], tail[k]});
    if (hi < std::min({face[0][k], face[1][k], face[2][k]}) ||
        lo > std::max({face[0][k], face[1][k], face[2][k]}))
      return {};
  }
  auto side = determinant(face[0], face[1], face[2], row, exact);
  if (side.sign < 0)
    return {}; // Entire possible crossing is outgoing, not an entry wall.
  if (!side.sign) {
    int drop = -1;
    auto project = [](Point p, size_t k) {
      return std::array<float, 2>{p[(k + 1) % 3], p[(k + 2) % 3]};
    };
    std::array<std::array<float, 2>, 3> flat;
    for (size_t k = 0; k < 3 && drop < 0; ++k) {
      for (size_t c = 0; c < 3; ++c)
        flat[c] = project(face[c], k);
      if (exact.orient2(flat[0], flat[1], flat[2]).sign)
        drop = int(k);
    }
    if (drop < 0)
      throw std::invalid_argument("Degenerate source-shadow wall");
    const int orientation = exact.orient2(flat[0], flat[1], flat[2]).sign;
    bool inside = true;
    for (size_t c = 0; c < 3; ++c) {
      int s =
          exact.orient2(flat[c], flat[(c + 1) % 3], project(row, drop)).sign;
      inside &= !s || s == orientation;
    }
    if (inside)
      return {Kind::OriginContact, {}};
    auto a = determinant(face[0], face[1], face[2], head, exact),
         b = determinant(face[0], face[1], face[2], tail, exact);
    if (a.sign && a.sign == b.sign)
      return {};
    if (a.sign || b.sign)
      return {Kind::Grazing,
              {}}; // No false clear for an isolated coplanar ray.
    std::array<Point, 3> fan{row, head, tail};
    for (size_t c = 0; c < 3; ++c) {
      volume_exact::Point2 p, q;
      p.ordinary = q.ordinary = true;
      auto u = project(fan[c], drop), v = project(fan[(c + 1) % 3], drop);
      p.free = {u[0], u[1]};
      q.free = {v[0], v[1]};
      if (exact.clip(flat, p, q))
        return {Kind::CoplanarContact, {}};
    }
    auto a2 = project(row, drop), b2 = project(head, drop),
         c2 = project(tail, drop);
    int fan_sign = exact.orient2(a2, b2, c2).sign;
    if (fan_sign)
      for (auto p : flat) {
        int x = exact.orient2(a2, b2, p).sign,
            y = exact.orient2(b2, c2, p).sign,
            z = exact.orient2(c2, a2, p).sign;
        if ((!x || x == fan_sign) && (!y || y == fan_sign) &&
            (!z || z == fan_sign))
          return {Kind::CoplanarContact, {}};
      }
    return {};
  }
  volume_exact::Interval clipped{exact.zero(), exact.one()};
  auto constraint = [&](volume_exact::Integer a, volume_exact::Integer b) {
    if (a.sign <= 0 && b.sign <= 0)
      return true;
    if (a.sign > 0 && b.sign > 0)
      return false;
    auto at = ratio(a, exact.subtract(a, b), exact);
    if (a.sign > 0) {
      if (exact.compare(at, clipped.enter) > 0)
        clipped.enter = at;
    } else if (exact.compare(at, clipped.exit) < 0)
      clipped.exit = at;
    return exact.compare(clipped.enter, clipped.exit) <= 0;
  };
  if (!constraint(determinant(face[0], face[1], face[2], head, exact),
                  determinant(face[0], face[1], face[2], tail, exact)))
    return {};
  for (size_t c = 0; c < 3; ++c)
    if (!constraint(determinant(row, head, face[c], face[(c + 1) % 3], exact),
                    determinant(row, tail, face[c], face[(c + 1) % 3], exact)))
      return {};
  work.stop();
  return {Kind::Interval, clipped};
}
} // namespace dome::detail::bone_shadow
