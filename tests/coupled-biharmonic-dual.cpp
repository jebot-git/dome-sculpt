// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/coupled-biharmonic-dual.hpp"
#include <array>
#include <iostream>
#include <random>
using namespace dome::detail;
struct Budget {
  size_t work = 0, maximum = 20'000'000;
  bool cancel = false;
  void stop() const {
    if (cancel)
      throw std::runtime_error("cancel");
  }
  void step(size_t n = 1) {
    if (n > maximum - work)
      throw std::length_error("work");
    work += n;
    stop();
  }
};
using Row = std::array<long double, 6>;
Row primal_oracle(const std::array<long double, 4> &a, const Row &r) {
  long double best = INFINITY;
  Row answer{};
  // Exhaust every zero/free face of the2-row3-label simplex, solve its
  // equality-constrained primal KKT using independent long-double elimination.
  for (unsigned mask = 1; mask < 64; ++mask) {
    if (!(mask & 7) || !(mask & 56))
      continue;
    int free[6], n = 0;
    for (int i = 0; i < 6; ++i)
      if (mask & (1u << i))
        free[n++] = i;
    long double m[8][9]{};
    for (int u = 0; u < n; ++u) {
      int row = free[u] / 3, label = free[u] % 3;
      for (int v = 0; v < n; ++v)
        if (free[v] % 3 == label)
          m[u][v] = a[row * 2 + free[v] / 3];
      m[u][n + row] = 1;
      for (int s = 0; s < 2; ++s)
        m[u][n + 2] += a[row * 2 + s] * r[s * 3 + label];
    }
    for (int row = 0; row < 2; ++row) {
      for (int v = 0; v < n; ++v)
        if (free[v] / 3 == row)
          m[n + row][v] = 1;
      m[n + row][n + 2] = 1;
    }
    bool singular = false;
    for (int c = 0; c < n + 2; ++c) {
      int p = c;
      for (int j = c + 1; j < n + 2; ++j)
        if (std::abs(m[j][c]) > std::abs(m[p][c]))
          p = j;
      if (std::abs(m[p][c]) < 1e-20L) {
        singular = true;
        break;
      }
      for (int j = c; j <= n + 2; ++j)
        std::swap(m[c][j], m[p][j]);
      auto d = m[c][c];
      for (int j = c; j <= n + 2; ++j)
        m[c][j] /= d;
      for (int u = 0; u < n + 2; ++u)
        if (u != c) {
          d = m[u][c];
          for (int j = c; j <= n + 2; ++j)
            m[u][j] -= d * m[c][j];
        }
    }
    if (singular)
      continue;
    Row x{};
    bool valid = true;
    for (int u = 0; u < n; ++u) {
      x[free[u]] = m[u][n + 2];
      valid &= x[free[u]] >= -1e-14L;
    }
    if (!valid)
      continue;
    long double energy = 0;
    for (int k = 0; k < 3; ++k)
      for (int i = 0; i < 2; ++i)
        for (int j = 0; j < 2; ++j)
          energy += (x[i * 3 + k] - r[i * 3 + k]) * a[i * 2 + j] *
                    (x[j * 3 + k] - r[j * 3 + k]) / 2;
    if (energy < best) {
      best = energy;
      answer = x;
    }
  }
  if (!std::isfinite(best))
    throw std::runtime_error("oracle infeasible");
  return answer;
}
struct DenseInverse {
  std::array<double, 4> inverse;
  double lipschitz;
  void apply_inverse(std::span<const double> x, std::span<double> y,
                     Budget &b) const {
    b.step(8);
    for (int i = 0; i < 2; ++i)
      y[i] = inverse[i * 2] * x[0] + inverse[i * 2 + 1] * x[1];
  }
};
int main() try {
  std::mt19937_64 rng(884292);
  std::uniform_real_distribution<double> positive(1, 4), signedv(-1, 1);
  double maximum = 0, kkt = 0, partition = 0;
  size_t iterations = 0, bounded = 0;
  for (int trial = 0; trial < 128; ++trial) {
    double a = positive(rng), d = positive(rng),
           c = .8 * signedv(rng) * std::sqrt(a * d), det = a * d - c * c;
    DenseInverse op{{d / det, -c / det, -c / det, a / det},
                    std::max((d + std::abs(c)) / det, (a + std::abs(c)) / det)};
    Row r{};
    std::array<double, 6> input{}, x{}, lambda{};
    for (int row = 0; row < 2; ++row) {
      input[row * 3] = signedv(rng);
      input[row * 3 + 1] = signedv(rng);
      input[row * 3 + 2] = 1 - input[row * 3] - input[row * 3 + 1];
    }
    for (int i = 0; i < 6; ++i)
      r[i] = input[i];
    auto expected = primal_oracle({a, c, c, d}, r);
    Budget budget;
    CoupledDualOptions options;
    auto report = solve_coupled_biharmonic_dual(op, 2, 3, input, x, lambda,
                                                options, budget);
    for (int i = 0; i < 6; ++i) {
      maximum = std::max(maximum, double(std::abs(x[i] - expected[i])));
      bounded += expected[i] < 1e-14L;
    }
    kkt = std::max(kkt, report.kkt);
    partition = std::max(partition, report.partition_error);
    iterations += report.iterations;
    // Fresh original primal stationarity incl row multipliers, independent
    // from the dual projected stopping residual.
    for (int row = 0; row < 2; ++row) {
      long double mean = 0, g[3]{};
      for (int k = 0; k < 3; ++k) {
        for (int s = 0; s < 2; ++s)
          g[k] += (row == s ? (row == 0 ? a : d) : c) *
                  (x[s * 3 + k] - input[s * 3 + k]);
        g[k] -= lambda[row * 3 + k];
        mean += g[k] / 3;
      }
      for (auto value : g)
        if (std::abs(value - mean) > 1e-8L)
          throw std::runtime_error("primal stationarity");
    }
    auto tiny_bound = op;
    tiny_bound.lipschitz *= 1e-8;
    std::array<double, 6> certified{}, certified_lambda{};
    Budget backtrack_budget;
    auto backtracked = solve_coupled_biharmonic_dual(
        tiny_bound, 2, 3, input, certified, certified_lambda, options,
        backtrack_budget);
    for (size_t i = 0; i < 6; ++i)
      if (std::abs(certified[i] - expected[i]) > 1e-8)
        throw std::runtime_error(
            "Backtracked step changed independent primal solution");
    if (backtracked.backtracks == 0 &&
        std::any_of(input.begin(), input.end(), [](double v) { return v < 0; }))
      throw std::runtime_error(
          "Deliberately undersized step bound was not rejected");
    auto unchanged = x;
    std::fill(x.begin(), x.end(), 173);
    std::fill(lambda.begin(), lambda.end(), 181);
    budget.cancel = true;
    bool refused = false;
    try {
      solve_coupled_biharmonic_dual(op, 2, 3, input, x, lambda, options,
                                    budget);
    } catch (const std::runtime_error &) {
      refused = true;
    }
    if (!refused || x != std::array<double, 6>{173, 173, 173, 173, 173, 173} ||
        lambda != std::array<double, 6>{181, 181, 181, 181, 181, 181})
      throw std::runtime_error("cancel mutated output");
    budget.cancel = false;
    budget.maximum = budget.work;
    refused = false;
    try {
      solve_coupled_biharmonic_dual(op, 2, 3, input, x, lambda, options,
                                    budget);
    } catch (const std::length_error &) {
      refused = true;
    }
    if (!refused || x != std::array<double, 6>{173, 173, 173, 173, 173, 173} ||
        lambda != std::array<double, 6>{181, 181, 181, 181, 181, 181})
      throw std::runtime_error("work refusal mutated outputs");
    // Force a real incomplete solve on a known active-bound case.
    if (trial == 0) {
      budget = Budget{};
      options.maximum_iterations = 1;
      refused = false;
      try {
        solve_coupled_biharmonic_dual(op, 2, 3, input, x, lambda, options,
                                      budget);
      } catch (const std::runtime_error &) {
        refused = true;
      }
      if (!refused ||
          x != std::array<double, 6>{173, 173, 173, 173, 173, 173} ||
          lambda != std::array<double, 6>{181, 181, 181, 181, 181, 181})
        throw std::runtime_error("unfinished solve mutated outputs");
    }
    (void)unchanged;
  }
  if (maximum > 1e-8 || kkt > 1e-10 || partition > 1e-12 || !bounded)
    throw std::runtime_error("independent primal oracle mismatch");
  std::cout << "PASS128 coupled simplex QPs,64 primal active faces each max="
            << maximum << " kkt=" << kkt << " partition=" << partition
            << " bounded=" << bounded << " iterations=" << iterations
            << " cancel exact\n";
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
