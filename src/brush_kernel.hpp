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
DS_DEVICE inline float brush_one(Sample s, Brush b) {
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
  if (b.operation != BrushOperation::Sculpt) {
    float falloff = fmaxf(0.f, 1.f - radial / radius);
    float target = b.operation == BrushOperation::MaskAdd ? 1.f
                   : b.operation == BrushOperation::MaskSubtract ? 0.f : s.average;
    return fmaxf(0.f, fminf(1.f, old + (target - old) *
        (falloff * falloff * b.strength * b.pressure)));
  }
  if (s.influence <= 0) return old;
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
  float amount = b.strength * b.pressure;
  if (s.influence != 1) amount *= s.influence;
  result = old + (result - old) * amount;
  return fmaxf(-b.band, fminf(b.band, result));
}
// Evaluate all mirrors against the same input sample. Taking the strongest
// delta (stable inward tie) avoids additive pressure at overlapping seams and
// keeps symmetry in the selected compute backend, with one dispatch/readback.
DS_DEVICE inline float brush_sample(Sample s, Brush b) {
  unsigned axes = b.symmetry_axes & 7;
  if (!axes) return brush_one(s, b);
  if (b.from.x == b.symmetry_origin.x && b.to.x == b.symmetry_origin.x &&
      (b.operation != BrushOperation::Sculpt || b.tool != Tool::Flatten || b.normal.x == 0)) axes &= ~1u;
  if (b.from.y == b.symmetry_origin.y && b.to.y == b.symmetry_origin.y &&
      (b.operation != BrushOperation::Sculpt || b.tool != Tool::Flatten || b.normal.y == 0)) axes &= ~2u;
  if (b.from.z == b.symmetry_origin.z && b.to.z == b.symmetry_origin.z &&
      (b.operation != BrushOperation::Sculpt || b.tool != Tool::Flatten || b.normal.z == 0)) axes &= ~4u;
  float best = s.distance;
  for (unsigned bits = 0; bits < 8; ++bits) {
    if (bits & ~axes) continue;
    Brush reflected = b;
    if (bits & 1) { reflected.from.x = 2 * b.symmetry_origin.x - b.from.x; reflected.to.x = 2 * b.symmetry_origin.x - b.to.x; reflected.normal.x = -b.normal.x; }
    if (bits & 2) { reflected.from.y = 2 * b.symmetry_origin.y - b.from.y; reflected.to.y = 2 * b.symmetry_origin.y - b.to.y; reflected.normal.y = -b.normal.y; }
    if (bits & 4) { reflected.from.z = 2 * b.symmetry_origin.z - b.from.z; reflected.to.z = 2 * b.symmetry_origin.z - b.to.z; reflected.normal.z = -b.normal.z; }
    float value = brush_one(s, reflected);
    if (fabsf(value - s.distance) > fabsf(best - s.distance) ||
        (fabsf(value - s.distance) == fabsf(best - s.distance) && value < best)) best = value;
  }
  return best;
}
} // namespace dome
