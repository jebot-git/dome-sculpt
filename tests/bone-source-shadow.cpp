// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/bone-source-shadow.hpp"
#include <iostream>
#include <random>
using namespace dome::detail;
struct Work {
  size_t used = 0, maximum = 500'000'000;
  bool cancelled = false;
  void stop() const {
    if (cancelled)
      throw std::runtime_error("cancelled");
  }
  void step(size_t n = 1) {
    if (n > maximum - used)
      throw std::length_error("work");
    used += n;
    stop();
  }
};
using P = bone_shadow::Point;
using T = bone_shadow::Triangle;
bool contains(const bone_shadow::Result &r, size_t numerator,
              size_t denominator, Work &w) {
  if (r.kind != bone_shadow::Kind::Interval)
    return false;
  volume_exact::Arithmetic exact(w);
  auto value =
      exact.fraction(exact.integer(numerator), exact.integer(denominator));
  return exact.compare(value, r.interval.enter) >= 0 &&
         exact.compare(value, r.interval.exit) <= 0;
}
// Independent long-double Moller-Trumbore point-ray oracle, with no divisions.
// Fixture inputs and query parameters are small dyadics: the products fit the
// actual long-double mantissa, including exact edge/endpoint equality.
bool entry_hit(P row, P head, P tail, const T &f, size_t at, size_t count) {
  using V = std::array<long double, 3>;
  auto sub = [](V a, V b) {
    for (size_t k = 0; k < 3; ++k)
      a[k] -= b[k];
    return a;
  };
  auto cv = [](P p) { return V{p[0], p[1], p[2]}; };
  auto cross = [](V a, V b) {
    return V{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
             a[0] * b[1] - a[1] * b[0]};
  };
  auto dot = [](V a, V b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
  V target{};
  for (size_t k = 0; k < 3; ++k)
    target[k] =
        (long double)head[k] + ((long double)tail[k] - head[k]) * at / count;
  auto d = sub(target, cv(row)), e1 = sub(cv(f[1]), cv(f[0])),
       e2 = sub(cv(f[2]), cv(f[0]));
  auto p = cross(d, e2);
  long double determinant = dot(e1, p);
  if (determinant <= 0)
    return false;
  auto q0 = sub(cv(row), cv(f[0]));
  long double u = dot(q0, p);
  if (u < 0 || u > determinant)
    return false;
  auto q = cross(q0, e1);
  long double v = dot(d, q), distance = dot(e2, q);
  return v >= 0 && u + v <= determinant && distance >= 0 &&
         distance <= determinant;
}
int main() try {
  Work work;
  const T wall{P{-2, -2, 0}, P{2, -2, 0}, P{0, 2, 0}};
  P row{0, 0, 1}, head{-3, 0, -1}, tail{3, 0, -1};
  auto interval = bone_shadow::triangle_shadow(row, head, tail, wall, work);
  volume_exact::Arithmetic exact(work);
  auto one_sixth = exact.fraction(exact.integer(1), exact.integer(6)),
       five_sixths = exact.fraction(exact.integer(5), exact.integer(6));
  if (interval.kind != bone_shadow::Kind::Interval ||
      exact.compare(interval.interval.enter, one_sixth) ||
      exact.compare(interval.interval.exit, five_sixths))
    throw std::runtime_error("analytic wall interval");
  T narrow{P{.375f, -1, 0}, P{.625f, -1, 0}, P{.5f, 1, 0}};
  auto hidden = bone_shadow::triangle_shadow(row, head, tail, narrow, work);
  if (contains(hidden, 0, 1, work) || contains(hidden, 1, 2, work) ||
      contains(hidden, 1, 1, work) || !contains(hidden, 2, 3, work))
    throw std::runtime_error("endpoint/midpoint shortcut negative");
  auto coplanar = bone_shadow::triangle_shadow(P{-3, 0, 0}, P{3, 0, 0},
                                               P{4, 0, 0}, wall, work);
  if (coplanar.kind != bone_shadow::Kind::CoplanarContact)
    throw std::runtime_error("coplanar contact");
  auto grazing = bone_shadow::triangle_shadow(P{-3, 0, 0}, P{0, 0, -1},
                                              P{0, 0, 1}, wall, work);
  if (grazing.kind != bone_shadow::Kind::Grazing)
    throw std::runtime_error("grazing contact");
  auto origin =
      bone_shadow::triangle_shadow(P{0, 0, 0}, head, tail, wall, work);
  if (origin.kind != bone_shadow::Kind::OriginContact)
    throw std::runtime_error("unowned origin contact");
  std::mt19937 random(0xa5143);
  size_t rays = 0, intervals = 0, ambiguous = 0;
  auto scalar = [&] { return float(int(random() % 65) - 32) / 16; };
  auto point = [&] { return P{scalar(), scalar(), scalar()}; };
  for (size_t sample = 0; sample < 512; ++sample) {
    P r = point(), a = point(), b = point();
    T f{point(), point(), point()};
    auto hit = bone_shadow::triangle_shadow(r, a, b, f, work);
    if (hit.kind != bone_shadow::Kind::None &&
        hit.kind != bone_shadow::Kind::Interval) {
      ++ambiguous;
      continue;
    }
    intervals += hit.kind == bone_shadow::Kind::Interval;
    auto reversed = bone_shadow::triangle_shadow(r, b, a, f, work);
    for (float scale : {.125f, 8.f}) {
      auto transform = [&](P p) {
        return P{scale * (-p[2] + 16), scale * (p[0] - 8), scale * (-p[1] + 4)};
      };
      T moved{transform(f[0]), transform(f[1]), transform(f[2])};
      auto current = bone_shadow::triangle_shadow(transform(r), transform(a),
                                                  transform(b), moved, work);
      if (current.kind != hit.kind ||
          (hit.kind == bone_shadow::Kind::Interval &&
           (exact.compare(hit.interval.enter, current.interval.enter) ||
            exact.compare(hit.interval.exit, current.interval.exit))))
        throw std::runtime_error("exact metric covariance");
    }
    for (size_t k = 0; k <= 256; ++k) {
      bool expected = entry_hit(r, a, b, f, k, 256),
           actual = contains(hit, k, 256, work);
      if (actual != expected)
        throw std::runtime_error("independent ray mismatch");
      if (actual != contains(reversed, 256 - k, 256, work))
        throw std::runtime_error("bone reversal");
      ++rays;
    }
  }
  Work small;
  small.maximum = 1;
  bool refused = false;
  try {
    bone_shadow::triangle_shadow(row, head, tail, wall, small);
  } catch (const std::length_error &) {
    refused = true;
  }
  if (!refused)
    throw std::runtime_error("missing work guard");
  Work cancel;
  cancel.cancelled = true;
  refused = false;
  try {
    bone_shadow::triangle_shadow(row, head, tail, wall, cancel);
  } catch (const std::runtime_error &) {
    refused = true;
  }
  if (!refused)
    throw std::runtime_error("missing cancel guard");
  for (float scale : {std::ldexp(1.f, -100), 1024.f}) {
    auto multiply = [&](P p) {
      for (auto &v : p)
        v *= scale;
      return p;
    };
    auto current = bone_shadow::triangle_shadow(
        multiply(row), multiply(head), multiply(tail),
        T{multiply(wall[0]), multiply(wall[1]), multiply(wall[2])}, work);
    if (current.kind != bone_shadow::Kind::Interval ||
        exact.compare(current.interval.enter, one_sixth) ||
        exact.compare(current.interval.exit, five_sixths))
      throw std::runtime_error("small/large exact dyadic covariance");
  }
  std::cout << "PASS random_cases=512 point_ray_oracles=" << rays
            << " intervals=" << intervals << " ambiguous=" << ambiguous
            << " work=" << work.used
            << " exact_wall_1/6_5/6=1 endpoint_midpoint_negative=1 "
               "coplanar_grazing_origin_guard=1 field_claim=0\n";
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
