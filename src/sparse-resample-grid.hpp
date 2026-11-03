// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "sparse-resample-kernel.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <functional>
#include <span>
#include <stdexcept>
#include <vector>
namespace dome::sparse_resample {
inline constexpr size_t maximum_entries = 4'000'000, maximum_queries = 16'000'000;
struct Entry { Point point; double value; };
class Grid {
  std::vector<Cell> cells_;
  uint32_t mask_ = 0;
public:
  std::span<const Cell> cells() const { return cells_; }
  uint32_t mask() const { return mask_; }
  explicit Grid(std::span<const Entry> entries, const std::function<void()> &checkpoint = {},
                const std::function<void(size_t)> &charge = {}) {
    if (entries.size() > maximum_entries) throw std::length_error("Sparse resampling source budget exceeded");
    size_t capacity = 2;
    while (capacity < entries.size() * 2) capacity *= 2;
    cells_.resize(capacity);
    mask_ = uint32_t(capacity - 1);
    size_t pending = 0;
    for (const auto &entry : entries) {
      if (!std::isfinite(entry.value) || std::abs(entry.value) > 1e6)
        throw std::invalid_argument("Sparse resampling source value is invalid");
      uint32_t slot = hash(entry.point) & mask_, probe = 0;
      for (; probe < maximum_probe; ++probe) {
        if (++pending == 256) {
          if (charge) charge(pending);
          pending = 0;
          if (checkpoint) checkpoint();
        }
        auto &cell = cells_[slot];
        if (!cell.occupied) {
          cell = {entry.point, 1, std::bit_cast<uint64_t>(entry.value)};
          break;
        }
        if (same(cell.point, entry.point)) throw std::invalid_argument("Sparse resampling source key is duplicated");
        slot = (slot + 1) & mask_;
      }
      if (probe == maximum_probe) throw std::length_error("Sparse resampling source probe budget exceeded");
    }
    if (pending && charge) charge(pending);
    if (checkpoint) checkpoint();
  }
};
inline Parameters parameters(float source, float target, double background) {
  if (!std::isfinite(source) || !std::isfinite(target) || source < .00001f || source > 1 ||
      target < .00001f || target > 1 || !std::isfinite(background) || std::abs(background) > 1e6)
    throw std::invalid_argument("Sparse resampling spacing/background is invalid");
  return {std::bit_cast<uint64_t>(double(source)), std::bit_cast<uint64_t>(double(target)),
          std::bit_cast<uint64_t>(background)};
}
inline int64_t coordinate_limit(Parameters p) {
  const double source = std::bit_cast<double>(p.source), target = std::bit_cast<double>(p.target);
  // Revalidate opaque words before using them in host admission arithmetic.
  const auto checked = parameters(float(source), float(target), std::bit_cast<double>(p.background));
  if (checked.source != p.source || checked.target != p.target)
    throw std::invalid_argument("Sparse resampling spacing must be exact native voxel spacing");
  return int64_t(std::min(double(INT32_MAX), std::floor(double(1U << 30) * source / target)));
}
inline void validate_queries(std::span<const Point> points, Parameters p,
                             const std::function<void()> &checkpoint = {}) {
  if (points.size() > maximum_queries) throw std::length_error("Sparse resampling query budget exceeded");
  const auto limit=coordinate_limit(p);
  for (size_t i = 0; i < points.size(); ++i) {
    const auto &q = points[i];
    if (std::max({std::abs(int64_t(q.x)), std::abs(int64_t(q.y)), std::abs(int64_t(q.z))}) > limit)
      throw std::out_of_range("Sparse resampling scaled coordinate budget exceeded");
    if ((i & 255) == 0 && checkpoint) checkpoint();
  }
  if (checkpoint) checkpoint();
}
inline void validate_error_queries(std::span<const ErrorQuery> points,Parameters p,
                                  const std::function<void()> &checkpoint={}) {
  if(points.size()>maximum_queries)throw std::length_error("Sparse quality query budget exceeded");
  const auto limit=coordinate_limit(p);
  for(size_t i=0;i<points.size();++i){
    const auto &q=points[i];const auto expected=std::bit_cast<double>(q.expected);
    if(q.reserved||!std::isfinite(expected)||std::abs(expected)>1e6)
      throw std::invalid_argument("Sparse quality expected value is invalid");
    if(std::max({std::abs(int64_t(q.point.x)),std::abs(int64_t(q.point.y)),std::abs(int64_t(q.point.z))})>limit)
      throw std::out_of_range("Sparse quality scaled coordinate budget exceeded");
    if((i&255)==0&&checkpoint)checkpoint();
  }
  if(checkpoint)checkpoint();
}
inline uint64_t checked_error(uint64_t bits) {
  if((bits>>63)||bits>=UINT64_C(0x7FF0000000000000))
    throw std::runtime_error("Sparse quality backend returned an invalid absolute error");
  return bits;
}
} // namespace dome::sparse_resample
