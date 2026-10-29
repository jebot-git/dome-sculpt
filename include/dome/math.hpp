// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <compare>
#include <cstdint>
#include <stdexcept>
namespace dome {
struct Vec3 {
  float x = 0, y = 0, z = 0;
  Vec3 operator+(Vec3 b) const { return {x + b.x, y + b.y, z + b.z}; }
  Vec3 operator-(Vec3 b) const { return {x - b.x, y - b.y, z - b.z}; }
  Vec3 operator*(float b) const { return {x * b, y * b, z * b}; }
  Vec3 operator/(float b) const { return *this * (1 / b); }
  Vec3 &operator+=(Vec3 b) {
    *this = *this + b;
    return *this;
  }
};
inline float dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline float length(Vec3 a) { return std::sqrt(dot(a, a)); }
inline Vec3 normalized(Vec3 a) {
  float l = length(a);
  return l > 1e-12f ? a / l : Vec3{0, 1, 0};
}
inline bool finite(Vec3 a) {
  return std::isfinite(a.x) && std::isfinite(a.y) && std::isfinite(a.z);
}
struct Int3 {
  int x = 0, y = 0, z = 0;
  auto operator<=>(const Int3 &) const = default;
  Int3 operator+(Int3 b) const { return {x + b.x, y + b.y, z + b.z}; }
  Int3 operator-(Int3 b) const { return {x - b.x, y - b.y, z - b.z}; }
};
struct IntHash {
  size_t operator()(Int3 a) const {
    uint64_t h = 1469598103934665603ULL;
    for (int v : {a.x, a.y, a.z}) {
      h ^= uint32_t(v);
      h *= 1099511628211ULL;
    }
    return h;
  }
};
struct Quat {
  float w = 1, x = 0, y = 0, z = 0;
  Quat operator*(Quat b) const {
    return {w * b.w - x * b.x - y * b.y - z * b.z,
            w * b.x + x * b.w + y * b.z - z * b.y,
            w * b.y - x * b.z + y * b.w + z * b.x,
            w * b.z + x * b.y - y * b.x + z * b.w};
  }
  Quat unit() const {
    float n = std::sqrt(w * w + x * x + y * y + z * z);
    return n > 1e-12f ? Quat{w / n, x / n, y / n, z / n} : Quat{};
  }
  Quat inverse() const { return {w, -x, -y, -z}; }
  Vec3 rotate(Vec3 v) const {
    Quat p = *this * Quat{0, v.x, v.y, v.z} * inverse();
    return {p.x, p.y, p.z};
  }
  std::array<float, 16> matrix() const {
    return {1 - 2 * (y * y + z * z),
            2 * (x * y + w * z),
            2 * (x * z - w * y),
            0,
            2 * (x * y - w * z),
            1 - 2 * (x * x + z * z),
            2 * (y * z + w * x),
            0,
            2 * (x * z + w * y),
            2 * (y * z - w * x),
            1 - 2 * (x * x + y * y),
            0,
            0,
            0,
            0,
            1};
  }
};
// Blender-style sphere/hyperbola projection. See third_party/sources.json.
inline Vec3 trackball(float x, float y) {
  constexpr float r = 1.1f, t = r * 0.7071067811865475f;
  float d = std::sqrt(x * x + y * y);
  return normalized(
      {x, y, d < t ? std::sqrt(r * r - d * d) : t * t / std::max(d, 1e-8f)});
}
inline Quat arcball(Vec3 a, Vec3 b) {
  float d = std::clamp(dot(a, b), -1.f, 1.f);
  if (d < -.99999f) {
    Vec3 axis = normalized(
        cross(a, std::abs(a.x) < .8f ? Vec3{1, 0, 0} : Vec3{0, 1, 0}));
    return {0, axis.x, axis.y, axis.z};
  }
  Vec3 axis = cross(a, b);
  return Quat{1 + d, axis.x, axis.y, axis.z}.unit();
}
} // namespace dome
