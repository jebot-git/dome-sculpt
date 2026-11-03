// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bounded-quadratic.hpp"
namespace dome::detail {
// Unconstrained Dirichlet quadratic minimizer, solely an initial field for a
// later bounded solve. Never an accepted clamped result. Actual free residual
// is independently recomputed before return; work/cancel/bytes stay bounded.
template <class Operator, class Budget>
BoundedQuadraticReport
solve_fixed_quadratic(Operator &op, std::span<const uint8_t> fixed,
                      std::span<const double> values, std::span<double> x,
                      const BoundedQuadraticOptions &options, Budget &budget,
                      double absolute_target = 0) {
  budget.stop();
  const size_t n = x.size();
  if (!options.maximum_bytes || options.maximum_bytes > 512u * 1024u * 1024u ||
      !n || n > 2'000'000 || fixed.size() != n || values.size() != n ||
      op.diagonal.size() != n || !options.maximum_cg_iterations ||
      options.maximum_cg_iterations > 16384 ||
      !(options.relative_tolerance > 0) || options.relative_tolerance > 1e-3 ||
      !std::isfinite(options.relative_tolerance) || absolute_target < 0 ||
      !std::isfinite(absolute_target))
    throw std::invalid_argument("Invalid fixed quadratic dimensions/options");
  if (options.maximum_bytes < 4096 ||
      n > (options.maximum_bytes - 4096) /
              (sizeof(double) * 6 + sizeof(uint8_t)))
    throw std::length_error("Fixed quadratic scratch exceeds byte cap");
  budget.step(n * 3);
  bool any = false;
  for (size_t i = 0; i < n; ++i) {
    if (fixed[i] > 1 || !std::isfinite(x[i]) || !(op.diagonal[i] > 0) ||
        !std::isfinite(op.diagonal[i]) ||
        (fixed[i] &&
         (!std::isfinite(values[i]) || values[i] < 0 || values[i] > 1)))
      throw std::invalid_argument("Invalid fixed quadratic scalar");
    any |= fixed[i] != 0;
  }
  if (!any)
    throw std::invalid_argument("Fixed quadratic lacks constraints");
  std::vector<double> product(n), r(n), z(n), p(n), ap(n), pins(n);
  std::vector<uint8_t> status(n);
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
    throw std::runtime_error("Invalid fixed quadratic scale");
  BoundedQuadraticReport report;
  auto residual = [&] {
    op.apply(x, product, budget);
    budget.step(n * 2);
    double norm = 0, energy = 0;
    for (size_t i = 0; i < n; ++i) {
      r[i] = fixed[i] ? 0 : -product[i];
      norm += r[i] * r[i];
      energy += .5 * x[i] * product[i];
    }
    report.kkt_residual = std::sqrt(norm) / scale;
    report.energy = energy;
    if (!std::isfinite(report.kkt_residual) || !std::isfinite(energy))
      throw std::runtime_error("Nonfinite unconstrained quadratic residual");
    return std::sqrt(norm);
  };
  const double target = absolute_target > 0
                            ? absolute_target
                            : options.relative_tolerance * .1 * scale;
  if (residual() <= target)
    return report;
  auto precondition = [&] {
    if constexpr (requires { op.precondition(r, z, status, budget); })
      op.precondition(r, z, status, budget);
    else {
      budget.step(n);
      for (size_t i = 0; i < n; ++i)
        z[i] = fixed[i] ? 0 : r[i] / op.diagonal[i];
    }
  };
  if constexpr (requires { op.begin_free_solve(status, budget); })
    op.begin_free_solve(status, budget);
  precondition();
  double rho = 0;
  budget.step(n);
  for (size_t i = 0; i < n; ++i) {
    p[i] = z[i];
    rho += r[i] * z[i];
  }
  for (uint32_t iteration = 0; iteration < options.maximum_cg_iterations;
       ++iteration) {
    budget.stop();
    op.apply(p, ap, budget);
    budget.step(n * 2);
    double curvature = 0;
    for (size_t i = 0; i < n; ++i) {
      if (fixed[i])
        ap[i] = 0;
      curvature += p[i] * ap[i];
    }
    if (!(curvature > 0) || !(rho > 0) || !std::isfinite(curvature) ||
        !std::isfinite(rho))
      throw std::runtime_error("Fixed quadratic PCG lost positive curvature");
    double alpha = rho / curvature, norm = 0;
    budget.step(n * 3);
    for (size_t i = 0; i < n; ++i) {
      if (!fixed[i])
        x[i] += alpha * p[i];
      r[i] -= alpha * ap[i];
      norm += r[i] * r[i];
    }
    ++report.cg_iterations;
    if (std::sqrt(norm) <= target ||
        iteration + 1 == options.maximum_cg_iterations) {
      if (residual() <= target)
        return report;
      if (iteration + 1 == options.maximum_cg_iterations)
        break;
      precondition();
      rho = 0;
      budget.step(n);
      for (size_t i = 0; i < n; ++i) {
        p[i] = z[i];
        rho += r[i] * z[i];
      }
      continue;
    }
    precondition();
    double next = 0;
    budget.step(n * 2);
    for (size_t i = 0; i < n; ++i)
      next += r[i] * z[i];
    double beta = next / rho;
    for (size_t i = 0; i < n; ++i)
      p[i] = z[i] + beta * p[i];
    rho = next;
  }
  throw std::runtime_error("Fixed quadratic missed true residual bound");
}
} // namespace dome::detail
