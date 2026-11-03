// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "coupled-biharmonic-dual.hpp"
#include <array>
#include <cstdint>
namespace dome::detail {
// Same convex dual, matrix-free projected Newton/PCG. One exact zero
// multiplier per row removes the PoU nullspace. True NNLS projected
// KKT, complementarity gap and primal simplex feasibility are independently
// required. Original native Q stationarity is additionally checked by caller.
template <class Inverse, class Budget>
CoupledDualReport
solve_newton_coupled_dual(Inverse &inverse, size_t rows, size_t labels,
                          std::span<const double> r, std::span<double> primal,
                          std::span<double> multipliers,
                          const CoupledDualOptions &o, Budget &budget) {
  budget.stop();
  if (!rows || rows > 2'000'000 || labels < 2 || labels > 256 ||
      rows > 8'000'000 / labels || r.size() != rows * labels ||
      primal.size() != r.size() || multipliers.size() != r.size() ||
      !o.maximum_iterations || o.maximum_iterations > 1'000'000 ||
      !(o.relative_tolerance > 0) || o.relative_tolerance > 1e-3 ||
      !std::isfinite(o.relative_tolerance))
    throw std::invalid_argument("Invalid squared coupled dual domain");
  const size_t n = r.size();
  if (!o.maximum_cg_iterations || o.maximum_cg_iterations > 16384)
    throw std::invalid_argument("Invalid Newton CG bound");
  if (o.maximum_bytes < 8192 || o.maximum_bytes > 512u * 1024u * 1024u ||
      n > (o.maximum_bytes - 8192) / 256)
    throw std::length_error("Projected dual history exceeds byte admission");
  budget.step(n * 2);
  double scale = 0;
  for (size_t i = 0; i < rows; ++i) {
    long double sum = 0;
    for (size_t k = 0; k < labels; ++k) {
      double value = r[i * labels + k];
      if (!std::isfinite(value))
        throw std::invalid_argument("Nonfinite projected-dual R");
      sum += value;
      scale += value * value;
    }
    if (std::abs(sum - 1) > 1e-12)
      throw std::invalid_argument("Projected-dual R lacks partition unity");
  }
  scale = std::max(1., std::sqrt(scale));
  if (!std::isfinite(scale))
    throw std::invalid_argument("Projected dual scale overflow");
  std::vector<double> z(n), next(n), gradient(n), next_gradient(n),
      direction(n), lambda(n), weights(n), next_weights(n), centered(n),
      column(rows), solved(rows), residual(n), preconditioned(n), search(n),
      product(n), temporary(n), energy(n);
  std::vector<uint8_t> free(n);
  std::vector<size_t> free_count(rows), pivot(rows);
  auto dot = [&](std::span<const double> a, std::span<const double> b) {
    budget.step(n);
    long double value = 0;
    for (size_t i = 0; i < n; ++i)
      value += static_cast<long double>(a[i]) * b[i];
    return value;
  };
  auto evaluate = [&](std::span<const double> point, std::span<double> g,
                      std::span<double> w) {
    budget.step(n * 3);
    for (size_t i = 0; i < n; ++i) {
      lambda[i] = point[i];
      if (!std::isfinite(lambda[i]) || lambda[i] < 0)
        throw std::runtime_error("Projected dual multiplier overflow");
    }
    for (size_t i = 0; i < rows; ++i) {
      long double mean = 0;
      for (size_t k = 0; k < labels; ++k)
        mean += lambda[i * labels + k] / labels;
      for (size_t k = 0; k < labels; ++k)
        centered[i * labels + k] = lambda[i * labels + k] - double(mean);
    }
    for (size_t k = 0; k < labels; ++k) {
      budget.step(rows * 2);
      for (size_t i = 0; i < rows; ++i)
        column[i] = centered[i * labels + k];
      inverse.apply_inverse(column, solved, budget);
      for (size_t i = 0; i < rows; ++i) {
        w[i * labels + k] = r[i * labels + k] + solved[i];
        g[i * labels + k] = w[i * labels + k];
        if (!std::isfinite(w[i * labels + k]) ||
            !std::isfinite(g[i * labels + k]))
          throw std::runtime_error("Projected dual gradient overflow");
      }
    }
    budget.step(n);
    long double objective = 0;
    for (size_t i = 0; i < n; ++i)
      objective += .5L * lambda[i] * (static_cast<long double>(w[i]) + r[i]);
    if (!std::isfinite(objective))
      throw std::runtime_error("Projected dual objective overflow");
    return objective;
  };
  auto objective = evaluate(z, gradient, weights);
  CoupledDualReport report;
  for (unsigned iteration = 0; iteration < o.maximum_iterations; ++iteration) {
    budget.stop();
    budget.step(n * 4);
    long double certificate_squared = 0, gap = 0;
    for (size_t i = 0; i < n; ++i) {
      // Exactly the unit-step projected NNLS residual, expressed without
      // subtracting nearly equal large multipliers.
      long double v = weights[i] < 0 ? weights[i] : std::min(weights[i], z[i]);
      certificate_squared += v * v;
      gap += std::abs(static_cast<long double>(z[i]) * weights[i]);
    }
    report.kkt = double(std::sqrt(certificate_squared)) / scale;
    report.duality_gap = double(gap / std::max(1.L, std::abs(objective)));
    report.iterations = iteration;
    if (!std::isfinite(report.kkt) || !std::isfinite(report.duality_gap))
      throw std::runtime_error("Projected dual certificate overflow");
    if (report.kkt <= o.relative_tolerance &&
        report.duality_gap <= o.relative_tolerance) {
      report.minimum = INFINITY;
      budget.step(n);
      for (size_t i = 0; i < rows; ++i) {
        long double sum = 0;
        for (size_t k = 0; k < labels; ++k) {
          sum += weights[i * labels + k];
          report.minimum = std::min(report.minimum, weights[i * labels + k]);
        }
        report.partition_error =
            std::max(report.partition_error, double(std::abs(sum - 1)));
      }
      if (report.minimum < -o.relative_tolerance * scale ||
          report.partition_error > 1e-10)
        throw std::runtime_error(
            "Projected dual primal feasibility missed bound");
      budget.stop();
      for (size_t i = 0; i < n; ++i) {
        primal[i] = weights[i];
        multipliers[i] = z[i];
      }
      return report;
    }
    // Canonical chart: at least one multiplier is exactly zero per row.
    // Fix the zero with largest gradient; any optimum of this chart that
    // violates global NNLS KKT admits a pivot change, never false success.
    budget.step(n * 3);
    for (size_t row = 0; row < rows; ++row) {
      size_t chosen = labels;
      for (size_t k = 0; k < labels; ++k)
        if (z[row * labels + k] == 0 &&
            (chosen == labels ||
             weights[row * labels + k] > weights[row * labels + chosen]))
          chosen = k;
      if (chosen == labels)
        throw std::logic_error("Newton dual lost its zero-row chart");
      pivot[row] = chosen;
      free_count[row] = 0;
      for (size_t k = 0; k < labels; ++k) {
        size_t i = row * labels + k;
        free[i] = k != chosen && (z[i] > 0 || weights[i] < 0);
        free_count[row] += free[i];
        residual[i] = free[i] ? -gradient[i] : 0;
        direction[i] = 0;
      }
    }
    auto hessian = [&](std::span<const double> input,
                       std::span<double> output) {
      budget.step(n * 2);
      for (size_t row = 0; row < rows; ++row) {
        long double mean = 0;
        for (size_t k = 0; k < labels; ++k)
          mean += input[row * labels + k] / labels;
        for (size_t k = 0; k < labels; ++k)
          centered[row * labels + k] = input[row * labels + k] - double(mean);
      }
      for (size_t k = 0; k < labels; ++k) {
        budget.step(rows * 2);
        for (size_t row = 0; row < rows; ++row)
          column[row] = centered[row * labels + k];
        inverse.apply_inverse(column, solved, budget);
        for (size_t row = 0; row < rows; ++row) {
          size_t i = row * labels + k;
          output[i] = free[i] ? solved[row] : 0;
          if (!std::isfinite(output[i]))
            throw std::runtime_error("Newton Hessian overflow");
        }
      }
    };
    auto label_block = [&](std::span<const double> input,
                           std::span<double> output) {
      budget.step(n * 2 + rows);
      for (size_t row = 0; row < rows; ++row) {
        size_t u = free_count[row];
        if (u >= labels)
          throw std::logic_error("Newton PoU principal block is singular");
        long double sum = 0;
        for (size_t k = 0; k < labels; ++k)
          if (free[row * labels + k])
            sum += input[row * labels + k];
        double beta =
            u ? (std::sqrt(double(labels) / (labels - u)) - 1) / u : 0;
        for (size_t k = 0; k < labels; ++k) {
          size_t i = row * labels + k;
          output[i] = free[i] ? input[i] + beta * double(sum) : 0;
        }
      }
    };
    auto precondition = [&] {
      // T * P_free * A * P_free * T is SPD. T is the positive
      // inverse square-root of each exact principal PoU label block.
      label_block(residual, temporary);
      for (size_t k = 0; k < labels; ++k) {
        budget.step(rows * 2);
        for (size_t row = 0; row < rows; ++row)
          column[row] = temporary[row * labels + k];
        inverse.apply_energy(column, solved, budget);
        for (size_t row = 0; row < rows; ++row)
          energy[row * labels + k] = solved[row];
      }
      label_block(energy, preconditioned);
    };
    long double rhs_norm = std::sqrt(dot(residual, residual));
    if (!(rhs_norm > 0))
      throw std::runtime_error("Newton chart stalled above global KKT");
    // Predeclared inexact-Newton forcing; independent final certificates
    // remain unchanged and never use this inner threshold as acceptance.
    double forcing =
        std::min(.5, std::sqrt(std::max(
                         report.kkt, std::numeric_limits<double>::epsilon())));
    long double target = forcing * rhs_norm;
    precondition();
    auto cg_rho = dot(residual, preconditioned);
    if (!(cg_rho > 0) || !std::isfinite(cg_rho))
      throw std::runtime_error("Newton preconditioner lost SPD");
    budget.step(n);
    std::copy(preconditioned.begin(), preconditioned.end(), search.begin());
    bool solved_cg = false;
    for (unsigned cg = 0; cg < o.maximum_cg_iterations; ++cg) {
      hessian(search, product);
      auto curvature = dot(search, product);
      if (!(curvature > 0) || !std::isfinite(curvature))
        throw std::runtime_error("Newton principal Hessian lost SPD");
      double alpha = double(cg_rho / curvature);
      budget.step(n * 3);
      for (size_t i = 0; i < n; ++i) {
        direction[i] += alpha * search[i];
        residual[i] -= alpha * product[i];
      }
      ++report.cg_iterations;
      if (std::sqrt(dot(residual, residual)) <= target) {
        solved_cg = true;
        break;
      }
      precondition();
      auto next_rho = dot(residual, preconditioned);
      if (!(next_rho > 0) || !std::isfinite(next_rho))
        throw std::runtime_error("Newton CG residual/preconditioner lost SPD");
      double beta = double(next_rho / cg_rho);
      budget.step(n);
      for (size_t i = 0; i < n; ++i)
        search[i] = preconditioned[i] + beta * search[i];
      cg_rho = next_rho;
    }
    if (!solved_cg)
      throw std::runtime_error("Newton CG missed prescribed residual");
    budget.step(n);
    for (size_t i = 0; i < n; ++i)
      if (z[i] == 0 && direction[i] < 0)
        direction[i] = 0;
    auto slope = dot(gradient, direction);
    if (!(slope < 0)) {
      // Standard deterministic curvature/history restart, before any trial.
      ++report.direction_restarts;
      budget.step(n);
      for (size_t i = 0; i < n; ++i)
        direction[i] = free[i] ? -gradient[i] : 0;
      slope = dot(gradient, direction);
    }
    if (!(slope < 0) || !std::isfinite(slope))
      throw std::runtime_error("Projected dual stalled above true NNLS KKT");
    double step = 1;
    bool accepted = false;
    long double next_objective = 0;
    for (unsigned trial = 0; trial < 64; ++trial) {
      budget.step(n);
      for (size_t i = 0; i < n; ++i)
        next[i] = std::max(0., z[i] + step * direction[i]);
      next_objective = evaluate(next, next_gradient, next_weights);
      // Exact quadratic difference in multiplier coordinates avoids
      // subtracting O(1) objectives when true progress is O(1e-18).
      budget.step(n * 3);
      long double objective_difference = 0, projected_slope = 0;
      for (size_t i = 0; i < n; ++i) {
        long double delta = static_cast<long double>(next[i]) - z[i];
        projected_slope += delta * gradient[i];
        objective_difference +=
            .5L * delta *
            (static_cast<long double>(weights[i]) + next_weights[i]);
      }
      if (projected_slope < 0 &&
          objective_difference <= 1e-4L * projected_slope) {
        accepted = true;
        break;
      }
      step *= .5;
      ++report.backtracks;
    }
    if (!accepted)
      throw std::runtime_error("Projected dual bounded line search failed");
    z.swap(next);
    gradient.swap(next_gradient);
    weights.swap(next_weights);
    objective = next_objective;
  }
  throw std::runtime_error(
      "Projected dual missed true primal/dual certificate");
}
} // namespace dome::detail
