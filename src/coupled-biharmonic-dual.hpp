// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <cmath>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>
namespace dome::detail {
// Native research control for Solomon/Stein2025 (doi10.1111/cgf.70159),
// eqs12-16. No copied implementation/runtime. For free-row SPD energy A,
// R is its unconstrained minimum corrected to sum1 per row. H(Y)=A^-1 Y P,
// P=I-11'/labels. min_{Y>=0} .5<Y,H(Y)>+<R,Y>; primal W=R+H(Y).
// Input inverse certification belongs to its caller. Each quadratic step
// majorization is independently checked and backtracked. This kernel
// enforces actual dual projected KKT, not iteration count or squared-variable
// stationarity; no postnormalization or accepted unconverged field.
struct CoupledDualOptions {
  size_t maximum_bytes = 128u * 1024u * 1024u;
  unsigned maximum_iterations = 16384, maximum_cg_iterations = 4096;
  double relative_tolerance = 1e-10;
};
struct CoupledDualReport {
  size_t iterations = 0, backtracks = 0, cg_iterations = 0,
         direction_restarts = 0;
  double final_lipschitz = 0, duality_gap = 0;
  double kkt = 0, minimum = 0, partition_error = 0;
};
template <class Inverse, class Budget>
CoupledDualReport solve_coupled_biharmonic_dual(
    Inverse &inverse, size_t rows, size_t labels, std::span<const double> r,
    std::span<double> primal, std::span<double> multipliers,
    const CoupledDualOptions &o, Budget &budget) {
  budget.stop();
  if (!rows || rows > 2'000'000 || labels < 2 || labels > 256 ||
      rows > 8'000'000 / labels || r.size() != rows * labels ||
      primal.size() != r.size() || multipliers.size() != r.size() ||
      !(inverse.lipschitz > 0) || !std::isfinite(inverse.lipschitz) ||
      !o.maximum_iterations || o.maximum_iterations > 1'000'000 ||
      !(o.relative_tolerance > 0) || o.relative_tolerance > 1e-3 ||
      !std::isfinite(o.relative_tolerance))
    throw std::invalid_argument("Invalid coupled dual domain/options");
  const size_t count = r.size();
  if (o.maximum_bytes < 8192 || o.maximum_bytes > 512u * 1024u * 1024u ||
      count > (o.maximum_bytes - 8192) / 96)
    throw std::length_error("Coupled dual scratch exceeds byte admission");
  budget.step(count * 2);
  double scale = 0;
  for (size_t i = 0; i < rows; ++i) {
    long double sum = 0;
    for (size_t k = 0; k < labels; ++k) {
      double value = r[i * labels + k];
      if (!std::isfinite(value))
        throw std::invalid_argument("Nonfinite coupled dual initial field");
      sum += value;
      scale += value * value;
    }
    if (std::abs(sum - 1) > 1e-12)
      throw std::invalid_argument("Coupled dual R does not sum to1");
  }
  scale = std::max(1., std::sqrt(scale));
  if (!std::isfinite(scale))
    throw std::invalid_argument("Coupled dual scale overflow");
  // Both externally visible outputs stay unchanged on any failure.
  std::vector<double> lambda(count), extrapolated(count), next(count),
      gradient(count), current_gradient(count), centered(count), column(rows),
      solved(rows);
  auto evaluate = [&](std::span<const double> y, std::span<double> g) {
    budget.step(count * 2);
    for (size_t i = 0; i < rows; ++i) {
      double mean = 0;
      for (size_t k = 0; k < labels; ++k)
        mean += y[i * labels + k] / labels;
      for (size_t k = 0; k < labels; ++k)
        centered[i * labels + k] = y[i * labels + k] - mean;
    }
    for (size_t k = 0; k < labels; ++k) {
      budget.step(rows * 2);
      for (size_t i = 0; i < rows; ++i)
        column[i] = centered[i * labels + k];
      inverse.apply_inverse(column, solved, budget);
      for (size_t i = 0; i < rows; ++i) {
        double value = r[i * labels + k] + solved[i];
        if (!std::isfinite(value))
          throw std::runtime_error("Coupled dual inverse/field overflow");
        g[i * labels + k] = value;
      }
    }
  };
  double acceleration = 1, lipschitz = inverse.lipschitz;
  CoupledDualReport report;
  for (unsigned iteration = 0; iteration < o.maximum_iterations; ++iteration) {
    budget.stop();
    evaluate(extrapolated, gradient);
    bool certified = false;
    for (unsigned backtrack = 0; backtrack < 64; ++backtrack) {
      budget.step(count * 2);
      for (size_t i = 0; i < count; ++i) {
        next[i] = std::max(0., extrapolated[i] - gradient[i] / lipschitz);
        if (!std::isfinite(next[i]))
          throw std::runtime_error("Coupled dual iterate overflow");
      }
      // For a quadratic, this is exactly2*(f(next)-f(y)-grad(y)'d).
      // Use independently evaluated inverse/gradient at next, not a
      // guessed spectral bound or an unchanged source's observed constant.
      evaluate(next, current_gradient);
      budget.step(count * 4);
      long double curvature = 0, squared_step = 0, absolute = 0;
      for (size_t i = 0; i < count; ++i) {
        long double d = static_cast<long double>(next[i]) - extrapolated[i];
        long double term =
            d * (static_cast<long double>(current_gradient[i]) - gradient[i]);
        curvature += term;
        absolute += std::abs(term);
        squared_step += d * d;
      }
      if (!std::isfinite(curvature) || !std::isfinite(squared_step) ||
          curvature < -64 * std::numeric_limits<double>::epsilon() * absolute)
        throw std::runtime_error(
            "Coupled dual inverse lost positive curvature");
      if (curvature <= lipschitz * squared_step) {
        certified = true;
        break;
      }
      if (!(squared_step > 0))
        throw std::runtime_error("Coupled dual backtrack has no finite step");
      lipschitz = 2 * std::max(lipschitz, double(curvature / squared_step));
      ++report.backtracks;
      if (!std::isfinite(lipschitz))
        throw std::runtime_error("Coupled dual backtrack scale overflow");
    }
    if (!certified)
      throw std::runtime_error("Coupled dual step was not certified");
    report.final_lipschitz = lipschitz;
    budget.step(count * 2);
    long double residual = 0;
    for (size_t i = 0; i < count; ++i) {
      // Actual NNLS first-order residual avoids subtracting nearly equal
      // large multipliers in a projected-gradient difference.
      long double v =
          next[i] > 0 ? current_gradient[i] : std::min(current_gradient[i], 0.);
      residual += v * v;
    }
    report.kkt = double(std::sqrt(residual)) / scale;
    report.iterations = iteration + 1;
    if (!std::isfinite(report.kkt))
      throw std::runtime_error("Coupled dual KKT overflow");
    if (report.kkt <= o.relative_tolerance) {
      report.minimum = INFINITY;
      budget.step(count);
      for (size_t i = 0; i < rows; ++i) {
        long double sum = 0;
        for (size_t k = 0; k < labels; ++k) {
          double w = current_gradient[i * labels + k];
          sum += w;
          report.minimum = std::min(report.minimum, w);
        }
        report.partition_error =
            std::max(report.partition_error, double(std::abs(sum - 1)));
      }
      if (report.minimum < -o.relative_tolerance * scale ||
          report.partition_error > 1e-10)
        throw std::runtime_error(
            "Coupled dual primal feasibility missed bound");
      budget.stop();
      std::copy(current_gradient.begin(), current_gradient.end(),
                primal.begin());
      std::copy(next.begin(), next.end(), multipliers.begin());
      return report;
    }
    const double t = .5 * (1 + std::sqrt(1 + 4 * acceleration * acceleration));
    budget.step(count);
    for (size_t i = 0; i < count; ++i)
      extrapolated[i] =
          next[i] + (acceleration - 1) / t * (next[i] - lambda[i]);
    lambda.swap(next);
    acceleration = t;
  }
  throw std::runtime_error("Coupled dual missed actual KKT bound");
}
} // namespace dome::detail
