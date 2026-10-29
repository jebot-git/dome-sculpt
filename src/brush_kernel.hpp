// SPDX-License-Identifier: GPL-3.0-or-later
// Shared arithmetic, compiled by the CPU, CUDA, HIP and SYCL implementations.
#pragma once
#include "dome/brush.hpp"
#if defined(__CUDACC__) || defined(__HIPCC__)
#define DS_DEVICE __host__ __device__
#else
#define DS_DEVICE
#endif
namespace dome {
DS_DEVICE inline float brush_sample(Sample s, Brush b) {
  float vx = b.to.x - b.from.x, vy = b.to.y - b.from.y, vz = b.to.z - b.from.z;
  float px = s.position.x - b.from.x, py = s.position.y - b.from.y,
        pz = s.position.z - b.from.z;
  float v2 = vx * vx + vy * vy + vz * vz;
  float t = v2 > 1e-14f
                ? fminf(1.f, fmaxf(0.f, (px * vx + py * vy + pz * vz) / v2))
                : 0;
  float dx = px - t * vx, dy = py - t * vy, dz = pz - t * vz;
  float radial = sqrtf(dx * dx + dy * dy + dz * dz);
  float radius = b.radius * (.25f + .75f * sqrtf(b.pressure));
  float sphere = radial - radius, old = s.distance, result = old;
  if (b.tool == Tool::Clay || b.tool == Tool::Carve) {
    float a = b.tool == Tool::Clay ? old : -old;
    float k = b.blend;
    float h = k > 0 ? fmaxf(k - fabsf(a - sphere), 0.f) / k : 0;
    result = fminf(a, sphere) - h * h * k * .25f;
    if (b.tool == Tool::Carve)
      result = -result;
  } else {
    float falloff = fmaxf(0.f, 1.f - radial / radius);
    float target = b.tool == Tool::Smooth
                       ? s.average
                       : (s.position.x - b.to.x) * b.normal.x +
                             (s.position.y - b.to.y) * b.normal.y +
                             (s.position.z - b.to.z) * b.normal.z;
    // Do not create a new surface in the empty truncated-distance background.
    if (fabsf(old) < b.band * .999f)
      result = old + (target - old) * falloff * falloff;
  }
  result = old + (result - old) * (b.strength * b.pressure);
  return fmaxf(-b.band, fminf(b.band, result));
}
} // namespace dome
