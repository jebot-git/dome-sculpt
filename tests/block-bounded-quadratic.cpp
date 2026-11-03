// SPDX-License-Identifier: GPL-3.0-or-later
#include "block-bounded-quadratic.hpp"
#include <array>
#include <functional>
#include <iostream>
#include <random>
using namespace dome::detail;
struct Budget {
  size_t used = 0, maximum = 1'000'000;
  std::function<bool()> cancelled;
  void stop() {
    if (cancelled && cancelled())
      throw std::runtime_error("cancelled");
  }
  void step(size_t n) {
    if (n > maximum - used)
      throw std::length_error("work");
    used += n;
    stop();
  }
};
struct Dense {
  std::array<std::array<double, 4>, 4> q{};
  std::array<double, 4> diagonal{};
  void apply(std::span<const double> x, std::span<double> y, Budget &budget) {
    budget.step(16);
    for (int i = 0; i < 4; ++i) {
      y[i] = 0;
      for (int j = 0; j < 4; ++j)
        y[i] += q[i][j] * x[j];
    }
  }
};
std::array<long double, 4> independent(const Dense &a,
                                       const std::array<uint8_t, 4> &fixed,
                                       const std::array<double, 4> &values) {
  long double best = INFINITY;
  std::array<long double, 4> answer{};
  for (int code = 0; code < 81; ++code) {
    int digits = code;
    std::array<int, 4> state{}, free{};
    int n = 0;
    std::array<long double, 4> x{};
    for (int i = 0; i < 4; ++i) {
      state[i] = digits % 3;
      digits /= 3;
      if (fixed[i]) {
        state[i] = 3;
        x[i] = values[i];
      } else if (state[i] == 0)
        free[n++] = i;
      else
        x[i] = state[i] == 1 ? 0 : 1;
    }
    long double matrix[4][5]{};
    for (int r = 0; r < n; ++r) {
      for (int c = 0; c < n; ++c)
        matrix[r][c] = a.q[free[r]][free[c]];
      for (int j = 0; j < 4; ++j)
        if (state[j])
          matrix[r][n] -= a.q[free[r]][j] * x[j];
    }
    bool singular = false;
    for (int c = 0; c < n; ++c) {
      int pivot = c;
      for (int r = c + 1; r < n; ++r)
        if (std::abs(matrix[r][c]) > std::abs(matrix[pivot][c]))
          pivot = r;
      if (std::abs(matrix[pivot][c]) < 1e-24L) {
        singular = true;
        break;
      }
      for (int k = c; k <= n; ++k)
        std::swap(matrix[c][k], matrix[pivot][k]);
      long double divisor = matrix[c][c];
      for (int k = c; k <= n; ++k)
        matrix[c][k] /= divisor;
      for (int r = 0; r < n; ++r)
        if (r != c) {
          long double factor = matrix[r][c];
          for (int k = c; k <= n; ++k)
            matrix[r][k] -= factor * matrix[c][k];
        }
    }
    if (singular)
      continue;
    bool feasible = true;
    for (int r = 0; r < n; ++r) {
      x[free[r]] = matrix[r][n];
      feasible &= x[free[r]] >= -1e-14L && x[free[r]] <= 1 + 1e-14L;
    }
    if (!feasible)
      continue;
    long double e = 0;
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j)
        e += x[i] * a.q[i][j] * x[j] / 2;
    if (e < best) {
      best = e;
      answer = x;
    }
  }
  if (!std::isfinite(best))
    throw std::runtime_error("Independent active-state enumeration infeasible");
  return answer;
}
int main() try {
  std::mt19937 rng(0x1b0a);
  std::uniform_int_distribution<int> dist(-5, 5);
  double max_error = 0, max_kkt = 0;
  size_t constrained = 0, activated = 0, released = 0;
  for (int trial = 0; trial < 128; ++trial) {
    Dense a;
    double m[4][4];
    for (auto &row : m)
      for (double &x : row)
        x = dist(rng) / 3.;
    for (int i = 0; i < 4; ++i)
      for (int j = 0; j < 4; ++j) {
        a.q[i][j] = i == j ? .125 : 0;
        for (int k = 0; k < 4; ++k)
          a.q[i][j] += m[k][i] * m[k][j];
      }
    for (int i = 0; i < 4; ++i)
      a.diagonal[i] = a.q[i][i];
    std::array<uint8_t, 4> fixed{};
    fixed[trial % 4] = 1;
    std::array<double, 4> values{}, x{.5, .5, .5, .5};
    values[trial % 4] = 1;
    if (trial % 3 == 1)
      x.fill(0);
    if (trial % 3 == 2)
      x.fill(1);
    if (trial % 3 == 0) {
      fixed[(trial + 2) % 4] = 1;
      values[(trial + 2) % 4] = .25;
    }
    auto expected = independent(a, fixed, values);
    BoundedQuadraticOptions options;
    options.relative_tolerance = 1e-11;
    options.maximum_cg_iterations = 64;
    Budget budget;
    auto report =
        solve_block_bounded_quadratic(a, fixed, values, x, options, budget);
    max_kkt = std::max(max_kkt, report.kkt_residual);
    activated += report.activated;
    released += report.released;
    bool bound = false;
    for (int i = 0; i < 4; ++i) {
      max_error = std::max(max_error, double(std::abs(x[i] - expected[i])));
      if (!fixed[i] && (x[i] == 0 || x[i] == 1))
        bound = true;
      if (x[i] < 0 || x[i] > 1 || (fixed[i] && x[i] != values[i]))
        throw std::runtime_error("Box/Dirichlet feasibility failed");
    }
    constrained += bound;
    if (max_error > 2e-8 || report.kkt_residual > options.relative_tolerance)
      throw std::runtime_error(
          "Native QP differs from exhaustive long-double active-state oracle");
  }
  if (!activated || !released || constrained < 16)
    throw std::runtime_error("Active-set release/block controls not exercised");
  std::cout << "PASS systems128 exhaustive81 states each maxerror=" << max_error
            << " kkt=" << max_kkt << " constrained=" << constrained
            << " activated=" << activated << " released=" << released << '\n';
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
