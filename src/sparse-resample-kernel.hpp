// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "softfloat/dome_f64.hpp"
#include <cstdint>
#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_RESAMPLE_FN __host__ __device__ inline
#else
#define DOME_RESAMPLE_FN inline
#endif
namespace dome::sparse_resample {
struct Point { int32_t x, y, z; };
struct Cell { Point point; uint32_t occupied; uint64_t value; };
struct Parameters { uint64_t source, target, background; };
inline constexpr uint32_t maximum_probe = 128;
inline constexpr uint64_t one = UINT64_C(0x3FF0000000000000);
DOME_RESAMPLE_FN uint32_t hash(Point p) {
  uint32_t h = uint32_t(p.x) * 0x8DA6B343U ^ uint32_t(p.y) * 0xD8163841U ^ uint32_t(p.z) * 0xCB1AB31FU;
  h ^= h >> 16; h *= 0x7FEB352DU; h ^= h >> 15; h *= 0x846CA68BU; h ^= h >> 16;
  return h;
}
DOME_RESAMPLE_FN bool same(Point a, Point b) {
  return a.x == b.x && a.y == b.y && a.z == b.z;
}
DOME_RESAMPLE_FN const Cell *find(const Cell *cells, uint32_t mask, Point p) {
  uint32_t slot = hash(p) & mask;
  for (uint32_t n = 0; n < maximum_probe; ++n) {
    const auto &cell = cells[slot];
    if (!cell.occupied) return nullptr;
    if (same(cell.point, p)) return &cell;
    slot = (slot + 1) & mask;
  }
  // Construction proves every present key is within maximum_probe. An absent
  // query therefore cannot miss an existing key beyond this bound.
  return nullptr;
}
// Host admission bounds every scaled coordinate to [-2^30,2^30]. This routine
// needs no native floating conversion (including for negative sub-unit values).
DOME_RESAMPLE_FN int32_t floor_coordinate(uint64_t bits) {
  const auto magnitude = bits & UINT64_C(0x7FFFFFFFFFFFFFFF);
  const bool negative = bits >> 63;
  const int exponent = int(magnitude >> 52) - 1023;
  if (exponent < 0) return negative && magnitude ? -1 : 0;
  const uint64_t significand = (magnitude & UINT64_C(0x000FFFFFFFFFFFFF)) | UINT64_C(0x0010000000000000);
  const unsigned shift = unsigned(52 - exponent);
  const auto integer = uint32_t(significand >> shift);
  const bool fraction = (significand & ((UINT64_C(1) << shift) - 1)) != 0;
  return negative ? -int32_t(integer) - int32_t(fraction) : int32_t(integer);
}
DOME_RESAMPLE_FN uint64_t sample(const Cell *cells, uint32_t mask, Point q, Parameters parameters) {
  using namespace softfloat64;
  const float64_t source{parameters.source}, target{parameters.target}, background{parameters.background};
  const auto px = f64_div(f64_mul(i32_to_f64(q.x), target), source);
  const auto py = f64_div(f64_mul(i32_to_f64(q.y), target), source);
  const auto pz = f64_div(f64_mul(i32_to_f64(q.z), target), source);
  const Point lo{floor_coordinate(px.v), floor_coordinate(py.v), floor_coordinate(pz.v)};
  const auto fx = f64_sub(px, i32_to_f64(lo.x));
  const auto fy = f64_sub(py, i32_to_f64(lo.y));
  const auto fz = f64_sub(pz, i32_to_f64(lo.z));
  const auto nx = f64_sub({one}, fx), ny = f64_sub({one}, fy), nz = f64_sub({one}, fz);
  auto result = background;
  for (int z = 0; z < 2; ++z) for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x) {
    const auto *cell = find(cells, mask, {lo.x + x, lo.y + y, lo.z + z});
    if (!cell) continue;
    auto term = f64_sub({cell->value}, background);
    term = f64_mul(term, x ? fx : nx);
    term = f64_mul(term, y ? fy : ny);
    term = f64_mul(term, z ? fz : nz);
    result = f64_add(result, term);
  }
  return result.v;
}
static_assert(sizeof(Point) == 12 && sizeof(Cell) == 24 && sizeof(Parameters) == 24);
} // namespace dome::sparse_resample
#undef DOME_RESAMPLE_FN
