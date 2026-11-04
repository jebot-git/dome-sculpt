// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <span>
#include <stdexcept>
#include <vector>
namespace dome::detail {
// A distinct variational construction. Contacts penalize only labels genuinely
// permitted by BOTH endpoints; each row is an exact permitted-label simplex.
// No normalized scalar-field postprocessing, absent-label sink or deleted
// common-role spring stands in for this constrained energy.
struct SharedSimplexReport {
  size_t sweeps = 0, bytes = 0;
  double kkt = 0, partition = 0, energy = 0;
};
template <class System, class Budget>
SharedSimplexReport solve_shared_simplex(
    const System &s, std::span<const double> screen,
    std::span<const double> rhs,
    std::span<const std::array<uint64_t, 4>> masks,
    std::span<const uint32_t> joints, std::vector<double> &out,
    size_t maximum_bytes, Budget &work) {
  const size_t n = screen.size(), labels = joints.size();
  if (!n || n > 2'000'000 || !labels || labels > 256 ||
      masks.size() != n || rhs.size() != n * labels ||
      s.starts.size() != n + 1 || s.starts.back() != s.columns.size() ||
      s.values.size() != s.columns.size())
    throw std::invalid_argument("Shared-simplex dimensions");
  const size_t bytes = n * labels * 2 * sizeof(double) +
                       3 * 256 * sizeof(double) + 256 * sizeof(uint32_t);
  if (bytes > maximum_bytes)
    throw std::length_error("Shared-simplex storage before allocation");
  work.step_items(n, labels * 4);
  SharedSimplexReport report;
  report.bytes = bytes;
  std::vector<double> diagonal(n * labels);
  out.assign(n * labels, 0.);
  auto allowed = [&](size_t i, size_t l) {
    const auto j = joints[l];
    if (j >= 256)
      throw std::invalid_argument("Shared-simplex joint outside mask");
    return (masks[i][j / 64] & (uint64_t(1) << (j % 64))) != 0;
  };
  for (size_t i = 0; i < n; ++i) {
    work.stop();
    if (!std::isfinite(screen[i]) || screen[i] < 0)
      throw std::invalid_argument("Shared-simplex negative screen");
    size_t count = 0;
    double total = 0;
    for (size_t l = 0; l < labels; ++l) {
      work.step(4);
      auto b = rhs[i * labels + l];
      if (!std::isfinite(b) || b < 0 || (!allowed(i, l) && b != 0))
        throw std::invalid_argument("Shared-simplex RHS lost declared intent");
      if (allowed(i, l)) {
        ++count;
        total += b;
        double d = screen[i];
        for (size_t e = s.starts[i]; e < s.starts[i + 1]; ++e) {
          work.step(4);
          if (s.columns[e] >= n || !std::isfinite(s.values[e]) || s.values[e] >= 0)
            throw std::invalid_argument("Shared-simplex graph edge invalid");
          if (allowed(s.columns[e], l))
            d -= s.values[e];
        }
        if (!(d > 0) || !std::isfinite(d))
          throw std::runtime_error("Unanchored shared-simplex variable");
        diagonal[i * labels + l] = d;
      }
    }
    if (!count)
      throw std::invalid_argument("Empty shared-simplex row intent");
    for (size_t l = 0; l < labels; ++l) {
      work.step();
      if (allowed(i, l))
        out[i * labels + l] = total ? rhs[i * labels + l] / total : 1. / count;
    }
  }
  std::array<double, 256> q{}, d{};
  std::array<uint32_t, 256> active{};
  // Exact cyclic block minimization. A whole-row weighted simplex projection
  // is the minimizer of the current conditional quadratic, not final pruning.
  // The finite cap is a refusal; independent KKT is the only acceptance gate.
  for (size_t sweep = 0; sweep < 4096; ++sweep) {
    work.stop();
    for (size_t i = 0; i < n; ++i) {
      work.stop();
      size_t count = 0;
      for (size_t l = 0; l < labels; ++l) {
        work.step();
        if (!allowed(i, l)) continue;
        active[count++] = uint32_t(l);
        q[l] = rhs[i * labels + l];
        d[l] = diagonal[i * labels + l];
        for (size_t e = s.starts[i]; e < s.starts[i + 1]; ++e) {
          work.step(4);
          if (allowed(s.columns[e], l))
            q[l] -= s.values[e] * out[size_t(s.columns[e]) * labels + l];
        }
      }
      std::sort(active.begin(), active.begin() + count, [&](auto a, auto b) {
        work.step();
        return q[a] > q[b];
      });
      double numer = -1, denom = 0, lambda = 0;
      size_t chosen = 0;
      for (size_t a = 0; a < count; ++a) {
        work.step(8);
        auto l = active[a];
        numer += q[l] / d[l];
        denom += 1. / d[l];
        lambda = numer / denom;
        chosen = a + 1;
        if (chosen == count || lambda >= q[active[chosen]]) break;
      }
      for (size_t a = 0; a < count; ++a) {
        work.step(3);
        auto l = active[a];
        out[i * labels + l] = a < chosen ? (q[l] - lambda) / d[l] : 0.;
      }
    }
    report.sweeps = sweep + 1;
    report.kkt = report.partition = report.energy = 0;
    // Independent replay of the ORIGINAL constrained energy gradient, rather
    // than the latest update delta or the weighted projection's own residual.
    for (size_t i = 0; i < n; ++i) {
      double sum = 0, dual = 0, positive = 0, scale = 0;
      for (size_t l = 0; l < labels; ++l) {
        work.step(4);
        auto x = out[i * labels + l];
        if (!allowed(i, l)) {
          if (x != 0) throw std::logic_error("Forbidden shared-simplex label nonzero");
          continue;
        }
        if (!std::isfinite(x) || x < 0)
          throw std::runtime_error("Shared-simplex primal positivity lost");
        sum += x;
        double g = screen[i] * x - rhs[i * labels + l];
        scale = std::max(scale, diagonal[i * labels + l]);
        report.energy += .5 * screen[i] * x * x - rhs[i * labels + l] * x;
        for (size_t e = s.starts[i]; e < s.starts[i + 1]; ++e) {
          work.step(8);
          const auto j = s.columns[e];
          if (allowed(j, l)) {
            double delta = x - out[size_t(j) * labels + l];
            g -= s.values[e] * delta;
            report.energy -= .25 * s.values[e] * delta * delta;
          }
        }
        q[l] = g;
        if (x > 0) { dual += g; ++positive; }
      }
      if (!positive || !(scale > 0))
        throw std::runtime_error("Shared-simplex row has no free label");
      dual /= positive;
      report.partition = std::max(report.partition, std::abs(sum - 1.));
      for (size_t l = 0; l < labels; ++l) {
        work.step(3);
        if (!allowed(i, l)) continue;
        const double violation = out[i * labels + l] > 0
                                     ? std::abs(q[l] - dual)
                                     : std::max(0., dual - q[l]);
        report.kkt = std::max(report.kkt, violation / scale);
      }
    }
    if (!std::isfinite(report.energy))
      throw std::runtime_error("Nonfinite shared-simplex energy");
    if (report.kkt <= 1e-9 && report.partition <= 1e-12) return report;
  }
  throw std::runtime_error("Shared-simplex original KKT bound not met");
}
} // namespace dome::detail
