// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "coupled-biharmonic-dual.hpp"
#include <array>
namespace dome::detail {
// Same convex dual, native projected limited-memory quasi-Newton.
// This is not a claim to reproduce L-BFGS-B. True NNLS projected
// KKT, complementarity gap and primal simplex feasibility are independently
// required. Original native Q stationarity is additionally checked by caller.
template <class Inverse, class Budget>
CoupledDualReport solve_projected_coupled_dual(
    Inverse &inverse, size_t rows, size_t labels, std::span<const double> r,
    std::span<double> primal, std::span<double> multipliers,
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
  constexpr size_t history = 8;
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
  std::vector<double> z(n, 0), next(n), gradient(n), next_gradient(n),
      direction(n), lambda(n), weights(n), next_weights(n), centered(n),
      column(rows), solved(rows), s(history * n), y(history * n),
      delta_point(n), delta_gradient(n);
  std::array<double, history> rho{}, alpha{};
  size_t used = 0, head = 0;
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
    long double residual = 0, gap = 0;
    for (size_t i = 0; i < n; ++i) {
      // Exactly the unit-step projected NNLS residual, expressed without
      // subtracting nearly equal large multipliers.
      long double v = weights[i] < 0 ? weights[i] : std::min(weights[i], z[i]);
      residual += v * v;
      gap += std::abs(static_cast<long double>(z[i]) * weights[i]);
    }
    report.kkt = double(std::sqrt(residual)) / scale;
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
    budget.step(n);
    for (size_t i = 0; i < n; ++i)
      direction[i] = (z[i] == 0 && gradient[i] >= 0) ? 0 : gradient[i];
    for (size_t age = 0; age < used; ++age) {
      size_t slot = (head + history - 1 - age) % history;
      auto si = std::span<const double>(s.data() + slot * n, n),
           yi = std::span<const double>(y.data() + slot * n, n);
      alpha[slot] = rho[slot] * double(dot(si, direction));
      budget.step(n);
      for (size_t i = 0; i < n; ++i)
        direction[i] -= alpha[slot] * yi[i];
    }
    double gamma = 1;
    if (used) {
      size_t slot = (head + history - 1) % history;
      auto si = std::span<const double>(s.data() + slot * n, n),
           yi = std::span<const double>(y.data() + slot * n, n);
      gamma = double(dot(si, yi) / dot(yi, yi));
      if (!(gamma > 0) || !std::isfinite(gamma))
        throw std::runtime_error("Invalid L-BFGS positive scale");
    }
    budget.step(n);
    for (double &value : direction)
      value *= gamma;
    for (size_t age = used; age-- > 0;) {
      size_t slot = (head + history - 1 - age) % history;
      auto si = std::span<const double>(s.data() + slot * n, n),
           yi = std::span<const double>(y.data() + slot * n, n);
      double beta = rho[slot] * double(dot(yi, direction));
      budget.step(n);
      for (size_t i = 0; i < n; ++i)
        direction[i] += si[i] * (alpha[slot] - beta);
    }
    budget.step(n);
    for (size_t i = 0; i < n; ++i) {
      direction[i] = -direction[i];
      if (z[i] == 0 && (gradient[i] >= 0 || direction[i] < 0))
        direction[i] = 0;
    }
    auto slope = dot(gradient, direction);
    if (!(slope < 0)) {
      // Standard deterministic curvature/history restart, before any trial.
      used = 0;
      head = 0;
      budget.step(n);
      for (size_t i = 0; i < n; ++i)
        direction[i] = (z[i] == 0 && gradient[i] >= 0) ? 0 : -gradient[i];
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
    auto si = std::span<double>(delta_point),
         yi = std::span<double>(delta_gradient);
    budget.step(n * 2);
    for (size_t i = 0; i < n; ++i) {
      si[i] = next[i] - z[i];
      yi[i] = next_gradient[i] - gradient[i];
    }
    long double sy = dot(si, yi), ss = dot(si, si), yy = dot(yi, yi);
    if (sy > 1e-12L * std::sqrt(ss * yy)) {
      rho[head] = double(1 / sy);
      if (!(rho[head] > 0) || !std::isfinite(rho[head]))
        throw std::runtime_error("L-BFGS reciprocal curvature overflow");
      budget.step(n * 2);
      std::copy(si.begin(), si.end(), s.begin() + head * n);
      std::copy(yi.begin(), yi.end(), y.begin() + head * n);
      head = (head + 1) % history;
      used = std::min(history, used + 1);
    }
    z.swap(next);
    gradient.swap(next_gradient);
    weights.swap(next_weights);
    objective = next_objective;
  }
  throw std::runtime_error(
      "Projected dual missed true primal/dual certificate");
}
} // namespace dome::detail
