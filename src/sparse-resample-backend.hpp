// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "sparse-resample-grid.hpp"
#include <memory>
namespace dome {
class SparseFieldSampler {
  struct Impl;
  std::unique_ptr<Impl> impl_;
public:
  SparseFieldSampler(const sparse_resample::Grid &, sparse_resample::Parameters,
                     size_t chunk_capacity = 65536);
  ~SparseFieldSampler();
  SparseFieldSampler(const SparseFieldSampler &) = delete;
  std::vector<uint64_t> sample(std::span<const sparse_resample::Point>,
                              const std::function<bool()> &cancelled = {});
  // Coordinates+expected words in; only one exact binary64 maximum out.
  uint64_t maximum_error(std::span<const sparse_resample::ErrorQuery>,
                         const std::function<bool()> &cancelled = {});
};
inline void sparse_resample_checkpoint(const std::function<bool()> &cancelled) {
  if (cancelled && cancelled()) throw std::runtime_error("Sculpt resolution change cancelled");
}
} // namespace dome
