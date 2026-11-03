// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>
namespace dome::detail {
// Fixed sparse incomplete LDL^T of the constrained Laplacian. Absolute/floored
// pivots make the PRECONDITIONER SPD; Q and its actual KKT residual are
// unchanged. P=C^-1 M C^-1 / mean(M), projected onto the current free
// variables, is SPD. No failed-solve fallback, adaptive output correction or
// radius/quality tuning.
class SquaredLaplacianPreconditioner {
  std::vector<size_t> starts_;
  std::vector<uint32_t> columns_;
  std::vector<double> lower_, d_, mass_, u_, v_;
  double scale_ = 0;
  template <class Budget>
  void inverse(std::span<double> x, Budget &budget) const {
    budget.step(x.size() * 2 + lower_.size() * 2);
    for (size_t i = 0; i < x.size(); ++i)
      for (size_t at = starts_[i]; at < starts_[i + 1]; ++at)
        x[i] -= lower_[at] * x[columns_[at]];
    for (size_t i = 0; i < x.size(); ++i)
      x[i] /= d_[i];
    for (size_t i = x.size(); i-- > 0;)
      for (size_t at = starts_[i]; at < starts_[i + 1]; ++at)
        x[columns_[at]] -= lower_[at] * x[i];
  }

public:
  size_t modified_pivots = 0, planned_bytes = 0;
  template <class System, class Budget>
  SquaredLaplacianPreconditioner(const System &l,
                                 std::span<const uint8_t> fixed,
                                 size_t maximum_bytes, Budget &budget) {
    budget.stop();
    const size_t n = l.diagonal.size(), nz = l.columns.size();
    if (!n || n > 2'000'000 || nz > 12'000'000 || fixed.size() != n ||
        l.starts.size() != n + 1 || l.mass.size() != n ||
        l.values.size() != nz || l.starts.front() != 0 || l.starts.back() != nz)
      throw std::invalid_argument(
          "Invalid sparse Laplacian preconditioner domain");
    planned_bytes = n * 80 + nz * 16 + 4096;
    if (planned_bytes > maximum_bytes)
      throw std::length_error(
          "Incomplete Laplacian exceeds preconstruction byte cap");
    starts_.assign(n + 1, 0);
    d_.resize(n);
    mass_ = l.mass;
    u_.resize(n);
    v_.resize(n);
    budget.step(n + nz);
    for (size_t i = 0; i < n; ++i) {
      if (fixed[i] > 1 || l.starts[i] > l.starts[i + 1] ||
          !(l.diagonal[i] > 0) || !std::isfinite(l.diagonal[i]) ||
          !(l.mass[i] > 0) || !std::isfinite(l.mass[i]))
        throw std::invalid_argument("Invalid constrained Laplacian scalar/row");
      scale_ += l.mass[i];
      uint32_t previous = 0;
      bool first = true;
      for (size_t at = l.starts[i]; at < l.starts[i + 1]; ++at) {
        auto col = l.columns[at];
        if (col >= n || col == i || (!first && col <= previous) ||
            !std::isfinite(l.values[at]))
          throw std::invalid_argument("Sparse Laplacian rows are not finite "
                                      "sorted unique off-diagonals");
        first = false;
        previous = col;
        if (col < i && !fixed[i] && !fixed[col])
          ++starts_[i + 1];
      }
      starts_[i + 1] += starts_[i];
    }
    scale_ /= n;
    if (!(scale_ > 0) || !std::isfinite(scale_))
      throw std::invalid_argument("Invalid Laplacian mass calibration");
    lower_.resize(starts_.back());
    columns_.resize(starts_.back());
    for (size_t i = 0; i < n; ++i) {
      size_t next = starts_[i];
      if (!fixed[i])
        for (size_t at = l.starts[i]; at < l.starts[i + 1]; ++at) {
          budget.step();
          auto col = l.columns[at];
          if (col < i && !fixed[col]) {
            columns_[next] = col;
            lower_[next++] = l.values[at];
          }
        }
      for (size_t at = starts_[i]; at < starts_[i + 1]; ++at) {
        auto j = columns_[at];
        double value = lower_[at];
        size_t a = starts_[i], b = starts_[j];
        while (a < at && b < starts_[j + 1]) {
          budget.step(10);
          auto x = columns_[a], y = columns_[b];
          if (x == y) {
            value -= lower_[a] * d_[x] * lower_[b];
            ++a;
            ++b;
          } else if (x < y)
            ++a;
          else
            ++b;
        }
        lower_[at] = value / d_[j];
        if (!std::isfinite(lower_[at]))
          throw std::runtime_error("Incomplete Laplacian factor overflow");
      }
      double pivot = fixed[i] ? 1 : l.diagonal[i];
      for (size_t at = starts_[i]; at < starts_[i + 1]; ++at) {
        budget.step(4);
        pivot -= lower_[at] * lower_[at] * d_[columns_[at]];
      }
      const double floor = 1e-8 * (fixed[i] ? 1 : l.diagonal[i]);
      d_[i] = std::max(std::abs(pivot), floor);
      modified_pivots += pivot <= floor;
      if (!(d_[i] > 0) || !std::isfinite(d_[i]))
        throw std::runtime_error(
            "Incomplete Laplacian positive pivot is invalid");
    }
  }
  template <class Budget>
  void apply(std::span<const double> r, std::span<double> z,
             std::span<const uint8_t> status, Budget &budget) {
    if (r.size() != d_.size() || z.size() != r.size() ||
        status.size() != r.size())
      throw std::invalid_argument("Preconditioner free-domain mismatch");
    budget.step(r.size() * 3);
    for (size_t i = 0; i < r.size(); ++i)
      u_[i] = status[i] ? 0 : r[i];
    inverse(u_, budget);
    for (size_t i = 0; i < r.size(); ++i)
      v_[i] = u_[i] * mass_[i] / scale_;
    inverse(v_, budget);
    for (size_t i = 0; i < r.size(); ++i) {
      z[i] = status[i] ? 0 : v_[i];
      if (!std::isfinite(z[i]))
        throw std::runtime_error(
            "Laplacian preconditioner produced nonfinite field");
    }
  }
};
} // namespace dome::detail
