// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "fixed-quadratic.hpp"
namespace dome::detail {
// Explicit block active-set construction informed by pinned libigl active_set.
// Intermediate free solutions may violate bounds. Every breached constraint is
// added; every wrong-sign active multiplier is released together. Unlike the
// reference's difference/iteration stop, ONLY actual primal feasibility and
// fresh original-Q box KKT authorize return. Cycles/caps refuse, never
// fallback.
template <class Operator, class Budget>
BoundedQuadraticReport solve_block_bounded_quadratic(
    Operator &op, std::span<const uint8_t> fixed,
    std::span<const double> values, std::span<double> x,
    const BoundedQuadraticOptions &options, Budget &budget) {
  budget.stop();
  const size_t n = x.size();
  if (!n || n > 2'000'000 || fixed.size() != n || values.size() != n ||
      op.diagonal.size() != n || !options.maximum_active_iterations ||
      options.maximum_active_iterations > 16384 ||
      !(options.relative_tolerance > 0) || options.relative_tolerance > 1e-3 ||
      !std::isfinite(options.relative_tolerance))
    throw std::invalid_argument("Invalid block quadratic domain/options");
  if (options.maximum_bytes < 8192 ||
      options.maximum_bytes > 512u * 1024u * 1024u ||
      n > (options.maximum_bytes - 8192) /
              (sizeof(double) * 10 + sizeof(uint8_t) * 4))
    throw std::length_error("Block quadratic scratch exceeds byte cap");
  budget.step(n * 3);
  bool any = false;
  for (size_t i = 0; i < n; ++i) {
    if (fixed[i] > 1 || !std::isfinite(x[i]) || !(op.diagonal[i] > 0) ||
        !std::isfinite(op.diagonal[i]) ||
        (fixed[i] &&
         (!std::isfinite(values[i]) || values[i] < 0 || values[i] > 1)))
      throw std::invalid_argument("Invalid block quadratic scalar");
    any |= fixed[i] != 0;
  }
  if (!any)
    throw std::invalid_argument("Block quadratic lacks fixed constraints");
  std::vector<uint8_t> status(n), active(n);
  std::vector<double> pinvalues(n), product(n), pins(n);
  for (size_t i = 0; i < n; ++i)
    if (fixed[i]) {
      status[i] = 3;
      x[i] = values[i];
      pins[i] = values[i];
    }
  op.apply(pins, product, budget);
  double scale = 0;
  for (double value : product)
    scale += value * value;
  scale = std::max(1., std::sqrt(scale));
  if (!std::isfinite(scale))
    throw std::runtime_error("Invalid original block quadratic scale");
  const double inactive =
      options.relative_tolerance * scale / (4 * std::sqrt(double(n)));
  BoundedQuadraticReport report;
  for (uint32_t iteration = 0; iteration < options.maximum_active_iterations;
       ++iteration) {
    budget.stop();
    budget.step(n * 2);
    size_t changes = 0;
    for (size_t i = 0; i < n; ++i) {
      if (!status[i] && x[i] < 0) {
        status[i] = 1;
        ++report.activated;
        ++changes;
      } else if (!status[i] && x[i] > 1) {
        status[i] = 2;
        ++report.activated;
        ++changes;
      }
      active[i] = status[i] != 0;
      pinvalues[i] = status[i] == 3 ? values[i] : status[i] == 2 ? 1 : 0;
    }
    auto solved =
        solve_fixed_quadratic(op, active, pinvalues, x, options, budget,
                              options.relative_tolerance * .05 * scale);
    report.cg_iterations += solved.cg_iterations;
    ++report.active_iterations;
    op.apply(x, product, budget);
    budget.step(n * 4);
    double norm = 0, energy = 0;
    bool feasible = true;
    for (size_t i = 0; i < n; ++i) {
      if (!std::isfinite(x[i]))
        throw std::runtime_error("Nonfinite block quadratic field");
      feasible &= x[i] >= 0 && x[i] <= 1;
      double g = product[i], v = fixed[i]    ? 0
                                 : x[i] == 0 ? std::min(g, 0.)
                                 : x[i] == 1 ? std::max(g, 0.)
                                             : g;
      norm += v * v;
      energy += .5 * x[i] * g;
    }
    report.energy = energy;
    report.kkt_residual = std::sqrt(norm) / scale;
    if (!std::isfinite(energy) || !std::isfinite(report.kkt_residual))
      throw std::runtime_error("Nonfinite actual block KKT/energy");
    if (feasible && report.kkt_residual <= options.relative_tolerance)
      return report;
    for (size_t i = 0; i < n; ++i) {
      if ((status[i] == 1 && product[i] < -inactive) ||
          (status[i] == 2 && product[i] > inactive)) {
        status[i] = 0;
        ++report.released;
        ++changes;
      }
    }
    if (feasible && !changes)
      throw std::runtime_error("Block active set stalled above true KKT bound");
  }
  throw std::runtime_error("Block active set missed true KKT bound");
}
} // namespace dome::detail
