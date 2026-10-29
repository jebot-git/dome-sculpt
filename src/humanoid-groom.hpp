// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/math.hpp"
#include <numbers>
namespace dome::creator_detail {
struct GroomStyle {
  float length = 0, volume = 0, fringe = 0;
};
inline float groom_phi(float angle) {
  float z = std::cos(angle);
  return 1.7f - .45f * std::max(0.f, z) + .65f * std::max(0.f, -z);
}
inline Vec3 groom_scalp(float theta, float phi, float volume) {
  return {std::sin(theta) * std::sin(phi) * (.95f + volume * .12f),
          7.55f + std::cos(phi) * 1.08f,
          .35f + std::cos(theta) * std::sin(phi) * (1.20f + volume * .10f)};
}
// One original geometry kernel shared by the MH fallback and fitted VRoid
// extensions. Callbacks supply metric fitting, cap clearance and guide fitting;
// generated UVs/indices are independent of skeleton/source mesh topology.
template <class Vertex, class Triangle, class Frame, class CapFit,
          class CenterFit>
void original_groom(GroomStyle c, Vertex vertex, Triangle triangle, Frame frame,
                    CapFit cap_fit, CenterFit center_fit) {
  constexpr float pi = std::numbers::pi_v<float>;
  constexpr int segments = 32, rings = 10;
  uint32_t next = 0;
  auto add = [&](Vec3 p, std::array<float, 2> uv, bool cap = false) {
    auto i = vertex(cap ? cap_fit(frame(p)) : frame(p), uv);
    next = i + 1;
    return i;
  };
  auto top = add(groom_scalp(0, 0, c.volume), {.5f, 0}, true);
  for (int r = 1; r <= rings; ++r)
    for (int s = 0; s <= segments; ++s) {
      float angle = 2 * pi * float(s) / segments;
      add(groom_scalp(angle, groom_phi(angle) * float(r) / rings, c.volume),
          {float(s) / segments, float(r) / rings * .38f}, true);
    }
  for (int s = 0; s < segments; ++s)
    triangle(top, top + 1 + s, top + 2 + s);
  for (int r = 0; r < rings - 1; ++r)
    for (int s = 0; s < segments; ++s) {
      uint32_t a = top + 1 + r * (segments + 1) + s, d = a + segments + 1;
      triangle(a, d, a + 1);
      triangle(a + 1, d, d + 1);
    }
  constexpr int locks = 24, steps = 10, sides = 6;
  for (int lock = 0; lock < locks; ++lock) {
    float theta = 2 * pi * (float(lock) + .5f) / locks,
          front = std::max(0.f, std::cos(theta));
    uint32_t start = next;
    Vec3 previous, first;
    for (int row = 0; row < steps; ++row) {
      float t = float(row) / (steps - 1),
            phi = .7f + (groom_phi(theta) - .7f) * t;
      Vec3 center = groom_scalp(theta, phi, c.volume);
      float drop = std::max(0.f, (t - .55f) / .45f) *
                   (front > .7f ? c.fringe * .5f : c.length * 4.5f);
      center.y -= drop;
      float flare = drop * .08f;
      center.x += std::sin(theta) * flare;
      center.z += std::cos(theta) * flare;
      float width = (.14f + c.volume * .05f) * (1 - .75f * t),
            depth = .07f * (1 - .6f * t);
      center = center_fit(center, width, depth, theta, t);
      for (int side = 0; side <= sides; ++side) {
        float a = 2 * pi * float(side) / sides;
        Vec3 p = center + Vec3{std::cos(theta) * std::cos(a) * width +
                                   std::sin(theta) * std::sin(a) * depth,
                               0,
                               -std::sin(theta) * std::cos(a) * width +
                                   std::cos(theta) * std::sin(a) * depth};
        add(p, {(float(lock % 6) + .06f + float(side) / sides * .88f) / 6,
                .42f + (float(lock / 6) + .12f + t * .76f) / 4 * .58f});
      }
      if (row == 0)
        first = center;
      previous = center;
    }
    for (int row = 0; row < steps - 1; ++row)
      for (int side = 0; side < sides; ++side) {
        uint32_t a = start + row * (sides + 1) + side, d = a + sides + 1;
        triangle(a, d, a + 1);
        triangle(a + 1, d, d + 1);
      }
    auto tip = add(
        previous + Vec3{0, -.07f, 0},
        {float(lock % 6) / 6 + .5f / 6, .42f + float(lock / 6 + 1) / 4 * .58f});
    auto cap = add(first, {float(lock % 6) / 6 + .5f / 6,
                           .42f + float(lock / 6) / 4 * .58f});
    for (int side = 0; side < sides; ++side) {
      uint32_t last = start + (steps - 1) * (sides + 1);
      triangle(tip, last + side + 1, last + side);
      triangle(cap, start + side, start + side + 1);
    }
  }
}
} // namespace dome::creator_detail
