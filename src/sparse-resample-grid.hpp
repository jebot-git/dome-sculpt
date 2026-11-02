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
struct Grid {
  std::vector<Cell> cells;
  uint32_t mask = 0;
  explicit Grid(std::span<const Entry> entries, const std::function<void()> &checkpoint = {}) {
    if (entries.size() > maximum_entries) throw std::length_error("Sparse resampling source budget exceeded");
    size_t capacity = 2;
    while (capacity < entries.size() * 2) capacity *= 2;
    cells.resize(capacity);
    mask = uint32_t(capacity - 1);
    size_t work = 0;
    for (const auto &entry : entries) {
      if (!std::isfinite(entry.value) || std::abs(entry.value) > 1e6)
        throw std::invalid_argument("Sparse resampling source value is invalid");
      uint32_t slot = hash(entry.point) & mask, probe = 0;
      for (; probe < maximum_probe; ++probe) {
        if ((++work & 255) == 0 && checkpoint) checkpoint();
        auto &cell = cells[slot];
        if (!cell.occupied) {
          cell = {entry.point, 1, std::bit_cast<uint64_t>(entry.value)};
          break;
        }
        if (same(cell.point, entry.point)) throw std::invalid_argument("Sparse resampling source key is duplicated");
        slot = (slot + 1) & mask;
      }
      if (probe == maximum_probe) throw std::length_error("Sparse resampling source probe budget exceeded");
    }
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
inline void validate_queries(std::span<const Point> points, Parameters p,
                             const std::function<void()> &checkpoint = {}) {
  if (points.size() > maximum_queries) throw std::length_error("Sparse resampling query budget exceeded");
  const double source = std::bit_cast<double>(p.source), target = std::bit_cast<double>(p.target);
  // Revalidate opaque words before using them in host admission arithmetic.
  const auto checked = parameters(float(source), float(target), std::bit_cast<double>(p.background));
  if (checked.source != p.source || checked.target != p.target)
    throw std::invalid_argument("Sparse resampling spacing must be exact native voxel spacing");
  const auto limit = int64_t(std::min(double(INT32_MAX), std::floor(double(1U << 30) * source / target)));
  for (size_t i = 0; i < points.size(); ++i) {
    const auto &q = points[i];
    if (std::max({std::abs(int64_t(q.x)), std::abs(int64_t(q.y)), std::abs(int64_t(q.z))}) > limit)
      throw std::out_of_range("Sparse resampling scaled coordinate budget exceeded");
    if ((i & 255) == 0 && checkpoint) checkpoint();
  }
  if (checkpoint) checkpoint();
}
} // namespace dome::sparse_resample
