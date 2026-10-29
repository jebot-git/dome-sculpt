// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace ob {
struct Vec3 {
  float x = 0, y = 0, z = 0;
};
inline Vec3 operator+(Vec3 a, Vec3 b) {
  return {a.x + b.x, a.y + b.y, a.z + b.z};
}
inline Vec3 operator-(Vec3 a, Vec3 b) {
  return {a.x - b.x, a.y - b.y, a.z - b.z};
}
inline Vec3 operator*(Vec3 a, float b) { return {a.x * b, a.y * b, a.z * b}; }
inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(Vec3 v) { return std::sqrt(dot(v, v)); }
inline Vec3 normalized(Vec3 v, Vec3 fallback = {1, 0, 0}) {
  float n = length(v);
  return n > 1e-7f ? v * (1 / n) : fallback;
}
inline bool finite(Vec3 v) {
  return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);
}
struct Quat {
  float x = 0, y = 0, z = 0, w = 1;
};
inline bool finite(Quat q) {
  return std::isfinite(q.x) && std::isfinite(q.y) && std::isfinite(q.z) &&
         std::isfinite(q.w);
}
inline Quat normalized(Quat q) {
  float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
  if (n < 1e-7f)
    return {};
  return {q.x / n, q.y / n, q.z / n, q.w / n};
}
inline Vec3 rotate(Quat q, Vec3 v) {
  q = normalized(q);
  Vec3 u{q.x, q.y, q.z};
  return v + cross(u, v) * (2 * q.w) + cross(u, cross(u, v)) * 2;
}
inline float segmentDistance(Vec3 point, Vec3 a, Vec3 b) {
  Vec3 d = b - a;
  float size = dot(d, d);
  return length(
      point -
      (a + d * (size > 1e-12f ? std::clamp(dot(point - a, d) / size, 0.f, 1.f)
                              : 0)));
}
} // namespace ob

namespace ob {
inline Quat conjugate(Quat q) {
  q = normalized(q);
  return {-q.x, -q.y, -q.z, q.w};
}
inline Quat multiply(Quat a, Quat b) {
  return normalized(Quat{a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
                         a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
                         a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
                         a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z});
}
struct Transform {
  Vec3 position;
  Quat orientation;
  float scale = 1;
};
inline bool finite(Transform t) {
  return finite(t.position) && finite(t.orientation) &&
         std::isfinite(t.scale) && t.scale > 0;
}
inline Vec3 transform(Transform t, Vec3 p) {
  return t.position + rotate(t.orientation, p * t.scale);
}
inline Transform compose(Transform a, Transform b) {
  return {transform(a, b.position), multiply(a.orientation, b.orientation),
          a.scale * b.scale};
}
inline Transform inverse(Transform t) {
  auto q = conjugate(t.orientation);
  return {rotate(q, t.position) * (-1 / t.scale), q, 1 / t.scale};
}
inline Quat between(Vec3 a, Vec3 b) {
  a = normalized(a);
  b = normalized(b);
  float d = std::clamp(dot(a, b), -1.f, 1.f);
  if (d < -.9999f) {
    auto axis = normalized(
        cross(a, std::abs(a.y) < .9f ? Vec3{0, 1, 0} : Vec3{1, 0, 0}));
    return {axis.x, axis.y, axis.z, 0};
  }
  auto axis = cross(a, b);
  return normalized(Quat{axis.x, axis.y, axis.z, 1 + d});
}
} // namespace ob
