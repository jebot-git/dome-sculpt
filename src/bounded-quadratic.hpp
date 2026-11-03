// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>
namespace dome::detail {
struct BoundedQuadraticOptions {
  size_t maximum_bytes = 512u * 1024u * 1024u;
  uint32_t maximum_active_iterations = 4096, maximum_cg_iterations = 4096;
  double relative_tolerance = 1e-9;
};
struct BoundedQuadraticReport {
  size_t active_iterations = 0, cg_iterations = 0, activated = 0, released = 0;
  double kkt_residual = 0, energy = 0;
};
// Native primal-feasible active-set QP: min .5*x'Q*x, 0<=x<=1,
// x[fixed]=values[fixed]. Q must be symmetric PSD, positive definite after
// constraining its null modes. Matrix-free PCG solves each free Newton system;
// acceptance uses a fresh full product and actual box KKT residual, never
// iteration count or clamping an unconstrained result. Caller stages x
// privately. Operator: diagonal span + apply(x,y,budget). Budget: step(count),
// stop().
template <class Operator, class Budget>
BoundedQuadraticReport
solve_bounded_quadratic(Operator &op, std::span<const uint8_t> fixed,
                        std::span<const double> values, std::span<double> x,
                        const BoundedQuadraticOptions &options,
                        Budget &budget) {
  budget.stop();
  const size_t n = x.size();
  if (!options.maximum_bytes || options.maximum_bytes > 512u * 1024u * 1024u ||
      !n || n > 2'000'000 || fixed.size() != n || values.size() != n ||
      op.diagonal.size() != n || !options.maximum_active_iterations ||
      options.maximum_active_iterations > 16384 ||
      !options.maximum_cg_iterations || options.maximum_cg_iterations > 16384 ||
      !(options.relative_tolerance > 0) || options.relative_tolerance > 1e-3 ||
      !std::isfinite(options.relative_tolerance))
    throw std::invalid_argument(
        "Invalid bounded quadratic dimensions/controls");
  if (n > options.maximum_bytes / (sizeof(double) * 8 + sizeof(uint8_t)))
    throw std::length_error("Bounded quadratic scratch exceeds byte cap");
  budget.step(n * 4);
  bool any_fixed = false;
  for (size_t i = 0; i < n; ++i) {
    if (fixed[i] > 1 || !std::isfinite(x[i]) || x[i] < 0 || x[i] > 1 ||
        !std::isfinite(op.diagonal[i]) || !(op.diagonal[i] > 0) ||
        (fixed[i] &&
         (!std::isfinite(values[i]) || values[i] < 0 || values[i] > 1)))
      throw std::invalid_argument("Invalid bounded quadratic input/diagonal");
    any_fixed |= fixed[i] != 0;
  }
  if (!any_fixed)
    throw std::invalid_argument(
        "Bounded quadratic lacks fixed handle authority");
  std::vector<double> product(n), gradient(n), r(n), z(n), p(n), ap(n),
      direction(n), pins(n);
  std::vector<uint8_t> status(n);
  for (size_t i = 0; i < n; ++i) {
    if (fixed[i]) {
      x[i] = values[i];
      pins[i] = values[i];
      status[i] = 3;
    } else
      status[i] = x[i] == 0 ? 1 : x[i] == 1 ? 2 : 0;
  }
  op.apply(pins, product, budget);
  double scale = 0;
  for (double v : product)
    scale += v * v;
  scale = std::max(1., std::sqrt(scale));
  if (!std::isfinite(scale))
    throw std::runtime_error("Nonfinite quadratic reference scale");
  BoundedQuadraticReport report;
  auto inspect = [&] {
    op.apply(x, product, budget);
    budget.step(n * 4);
    double residual = 0, free_residual = 0, energy = 0;
    for (size_t i = 0; i < n; ++i) {
      double g = gradient[i] = product[i];
      double v = status[i] == 3   ? 0
                 : status[i] == 1 ? std::min(g, 0.)
                 : status[i] == 2 ? std::max(g, 0.)
                                  : g;
      residual += v * v;
      if (status[i] == 0)
        free_residual += g * g;
      energy += .5 * x[i] * g;
    }
    report.energy = energy;
    report.kkt_residual = std::sqrt(residual) / scale;
    if (!std::isfinite(energy) || !std::isfinite(report.kkt_residual))
      throw std::runtime_error("Nonfinite bounded quadratic KKT/energy");
    return std::sqrt(free_residual);
  };
  auto free_product = [&](std::span<const double> a, std::span<double> out) {
    op.apply(a, out, budget);
    budget.step(n);
    for (size_t i = 0; i < n; ++i)
      if (status[i])
        out[i] = 0;
  };
  auto precondition = [&] {
    if constexpr (requires { op.precondition(r, z, status, budget); })
      op.precondition(r, z, status, budget);
    else {
      budget.step(n);
      for (size_t i = 0; i < n; ++i)
        z[i] = status[i] ? 0 : r[i] / op.diagonal[i];
    }
  };
  for (uint32_t outer = 0; outer < options.maximum_active_iterations; ++outer) {
    budget.stop();
    double free_norm = inspect();
    if (report.kkt_residual <= options.relative_tolerance)
      return report;
    if (free_norm <= options.relative_tolerance * .25 * scale) {
      size_t release = n;
      double worst = 0;
      budget.step(n);
      for (size_t i = 0; i < n; ++i) {
        double violation = status[i] == 1   ? -gradient[i]
                           : status[i] == 2 ? gradient[i]
                                            : 0;
        if (violation > worst) {
          worst = violation;
          release = i;
        }
      }
      if (release == n)
        throw std::runtime_error(
            "Quadratic active set has no improving constraint");
      status[release] = 0;
      ++report.released;
    }
    std::fill(direction.begin(), direction.end(), 0);
    double rho = 0;
    budget.step(n * 4);
    for (size_t i = 0; i < n; ++i) {
      r[i] = status[i] ? 0 : -gradient[i];
    }
    precondition();
    for (size_t i = 0; i < n; ++i) {
      p[i] = z[i];
      rho += r[i] * z[i];
    }
    const double target = options.relative_tolerance * .1 * scale;
    bool solved = false;
    for (uint32_t iteration = 0; iteration < options.maximum_cg_iterations;
         ++iteration) {
      budget.stop();
      free_product(p, ap);
      budget.step(n);
      double curvature = 0;
      for (size_t i = 0; i < n; ++i)
        curvature += p[i] * ap[i];
      if (!(curvature > 0) || !(rho > 0) || !std::isfinite(curvature) ||
          !std::isfinite(rho))
        throw std::runtime_error("Quadratic free PCG lost positive curvature");
      double alpha = rho / curvature, norm = 0;
      budget.step(n * 3);
      for (size_t i = 0; i < n; ++i) {
        direction[i] += alpha * p[i];
        r[i] -= alpha * ap[i];
        norm += r[i] * r[i];
      }
      ++report.cg_iterations;
      if (std::sqrt(norm) <= target ||
          iteration + 1 == options.maximum_cg_iterations) {
        free_product(direction, ap);
        norm = 0;
        budget.step(n);
        for (size_t i = 0; i < n; ++i) {
          r[i] = status[i] ? 0 : -gradient[i] - ap[i];
          norm += r[i] * r[i];
        }
        if (std::sqrt(norm) <= target) {
          solved = true;
          break;
        }
        if (iteration + 1 == options.maximum_cg_iterations)
          break;
        rho = 0;
        budget.step(n * 3);
        precondition();
        for (size_t i = 0; i < n; ++i) {
          p[i] = z[i];
          rho += r[i] * z[i];
        }
        continue;
      }
      double next = 0;
      budget.step(n * 3);
      precondition();
      for (size_t i = 0; i < n; ++i)
        next += r[i] * z[i];
      double beta = next / rho;
      for (size_t i = 0; i < n; ++i)
        p[i] = z[i] + beta * p[i];
      rho = next;
    }
    if (!solved)
      throw std::runtime_error("Quadratic free PCG missed true residual bound");
    double step = 1;
    size_t blocked = n;
    budget.step(n);
    for (size_t i = 0; i < n; ++i)
      if (status[i] == 0 && direction[i] != 0) {
        double feasible =
            direction[i] > 0 ? (1 - x[i]) / direction[i] : -x[i] / direction[i];
        if (feasible < step) {
          step = feasible;
          blocked = i;
        }
      }
    if (step < 0 || !std::isfinite(step))
      throw std::runtime_error("Invalid feasible quadratic step");
    const double before = report.energy;
    budget.step(n * 2);
    for (size_t i = 0; i < n; ++i)
      if (status[i] == 0) {
        double value = x[i] + step * direction[i];
        if (value < -1e-12 || value > 1 + 1e-12 || !std::isfinite(value))
          throw std::runtime_error("Quadratic line search lost feasibility");
        x[i] = std::clamp(value, 0.,
                          1.); // Only roundoff on a certified feasible step.
      }
    if (blocked != n) {
      x[blocked] = direction[blocked] > 0 ? 1 : 0;
      status[blocked] = x[blocked] == 0 ? 1 : 2;
      ++report.activated;
    }
    ++report.active_iterations;
    inspect();
    if (report.energy > before + 1e-11 * std::max(1., std::abs(before)))
      throw std::runtime_error("Bounded quadratic energy increased");
  }
  throw std::runtime_error("Bounded quadratic missed actual KKT bound");
}
} // namespace dome::detail
