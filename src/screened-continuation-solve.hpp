// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <span>
#include <stdexcept>
#include <vector>
namespace dome::detail {
// RESEARCH ONLY. Positive screened SURFACE/CONTACT graph, not a volumetric
// discretization or a BBW energy. Every bone sample is a fixed auxiliary node
// at its actual source position; surface rows have only finite conductances.
struct ContinuationReport {
  size_t rows = 0, samples = 0, contacts = 0, iterations = 0, work = 0,
         bytes = 0, supported_controls = 0;
  double residual = 0, minimum = 0, partition = 0;
};
// IC(0) for the actual screened M-matrix, not its square. No diagonal shifts,
// dropped negative pivots or acceptance by preconditioned residual.
template <class System, class Budget> class ContinuationIC {
  std::vector<size_t> starts_;
  std::vector<uint32_t> columns_;
  std::vector<double> values_, diagonal_;

public:
  ContinuationIC(const System &s, Budget &budget) {
    const size_t n = s.diagonal.size();
    starts_.resize(n + 1);
    diagonal_.resize(n);
    columns_.reserve(s.columns.size() / 2);
    values_.reserve(s.columns.size() / 2);
    for (uint32_t i = 0; i < n; ++i) {
      budget.stop();
      starts_[i] = columns_.size();
      for (size_t at = s.starts[i]; at < s.starts[i + 1]; ++at) {
        budget.step();
        auto j = s.columns[at];
        if (j >= i)
          break;
        double value = s.values[at];
        size_t a = starts_[i], b = starts_[j];
        while (a < columns_.size() && b < starts_[j + 1]) {
          budget.step(4);
          if (columns_[a] < columns_[b])
            ++a;
          else if (columns_[b] < columns_[a])
            ++b;
          else {
            value -= values_[a++] * values_[b++];
          }
        }
        value /= diagonal_[j];
        if (!std::isfinite(value))
          throw std::runtime_error("Continuation IC nonfinite entry");
        columns_.push_back(j);
        values_.push_back(value);
      }
      starts_[i + 1] = columns_.size();
      double pivot = s.diagonal[i];
      for (size_t at = starts_[i]; at < starts_[i + 1]; ++at) {
        budget.step(2);
        pivot -= values_[at] * values_[at];
      }
      if (!(pivot > 0) || !std::isfinite(pivot))
        throw std::runtime_error("Continuation IC lost positive pivot");
      diagonal_[i] = std::sqrt(pivot);
    }
  }
  void apply(std::span<const double> rhs, std::span<double> out,
             Budget &budget) const {
    const size_t n = diagonal_.size();
    budget.step(4 * n + 4 * columns_.size());
    for (size_t i = 0; i < n; ++i) {
      double value = rhs[i];
      for (size_t at = starts_[i]; at < starts_[i + 1]; ++at)
        value -= values_[at] * out[columns_[at]];
      out[i] = value / diagonal_[i];
    }
    for (size_t i = n; i-- > 0;) {
      out[i] /= diagonal_[i];
      for (size_t at = starts_[i]; at < starts_[i + 1]; ++at)
        out[columns_[at]] -= values_[at] * out[i];
    }
  }
};
template <class System, class Budget, class Preconditioner>
void continuation_solve(const System &s, std::span<const double> rhs,
                        std::span<double> x, Preconditioner &ic, Budget &budget,
                        ContinuationReport &report) {
  const size_t n = rhs.size();
  std::vector<double> r(rhs.begin(), rhs.end()), z(n), p(n), ap(n);
  std::fill(x.begin(), x.end(), 0);
  double squared = 0;
  for (double v : rhs) {
    budget.step();
    squared += v * v;
  }
  if (!squared)
    throw std::runtime_error("Continuation omitted an eligible control");
  double scale = std::sqrt(squared);
  ic.apply(r, z, budget);
  p = z;
  double rho = std::inner_product(r.begin(), r.end(), z.begin(), 0.);
  budget.step(n * 2);
  for (size_t iteration = 0; iteration < 4096; ++iteration) {
    budget.stop();
    s.multiply(p, ap, budget);
    double curvature = std::inner_product(p.begin(), p.end(), ap.begin(), 0.);
    budget.step(n);
    if (!(curvature > 0) || !(rho > 0) || !std::isfinite(curvature))
      throw std::runtime_error("Continuation PCG lost positive curvature");
    double alpha = rho / curvature, error = 0;
    budget.step(n * 2);
    for (size_t i = 0; i < n; ++i) {
      x[i] += alpha * p[i];
      r[i] -= alpha * ap[i];
      error += r[i] * r[i];
    }
    ++report.iterations;
    if (std::sqrt(error) <= 1e-9 * scale || iteration == 4095) {
      s.multiply(x, ap, budget);
      error = 0;
      budget.step(n);
      for (size_t i = 0; i < n; ++i) {
        r[i] = rhs[i] - ap[i];
        error += r[i] * r[i];
      }
      double residual = std::sqrt(error) / scale;
      if (residual <= 1e-9) {
        report.residual = std::max(report.residual, residual);
        for (double v : x) {
          budget.step();
          if (!std::isfinite(v) || v < -1e-9 || v > 1 + 1e-9)
            throw std::runtime_error("Continuation violates maximum principle");
          report.minimum = std::min(report.minimum, v);
        }
        return;
      }
      if (iteration == 4095)
        break;
      ic.apply(r, z, budget);
      p = z;
      rho = std::inner_product(r.begin(), r.end(), z.begin(), 0.);
      budget.step(n * 2);
      continue;
    }
    ic.apply(r, z, budget);
    double next = std::inner_product(r.begin(), r.end(), z.begin(), 0.);
    double beta = next / rho;
    budget.step(2 * n);
    for (size_t i = 0; i < n; ++i)
      p[i] = z[i] + beta * p[i];
    rho = next;
  }
  throw std::runtime_error("Continuation failed original residual bound");
}
} // namespace dome::detail
