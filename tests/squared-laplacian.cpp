// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/biharmonic-binding.cpp"
#include <iostream>
using namespace dome;
using namespace dome::detail;
struct Dense {
  std::array<std::array<double, 4>, 4> q{};
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
  // Independently certified signed SPD cycle: eigenvalues1±sqrt(.72).
  // Its zero-fill LDL has final pivot -0.182857..., so this genuinely exercises
  // positive modification in the preconditioner, never an altered QP energy.
  System a;
  a.diagonal.assign(4, 1);
  a.mass.assign(4, 1);
  a.starts = {0, 2, 4, 6, 8};
  a.columns = {1, 3, 0, 2, 1, 3, 0, 2};
  a.values = {.6, -.6, .6, .6, .6, .6, -.6, .6};
  std::function<bool()> cancel;
  Budget budget{0, 1'000'000, cancel};
  std::array<uint8_t, 4> none{};
  SquaredLaplacianPreconditioner preconditioner(a, none, 4096, budget);
  if (preconditioner.modified_pivots != 1)
    throw std::runtime_error(
        "Signed SPD incomplete-pivot control not exercised");
  long double p[4][4]{};
  double symmetry = 0;
  for (size_t j = 0; j < 4; ++j) {
    std::array<double, 4> r{}, z{};
    r[j] = 1;
    preconditioner.apply(r, z, none, budget);
    for (size_t i = 0; i < 4; ++i)
      p[i][j] = z[i];
  }
  for (size_t i = 0; i < 4; ++i)
    for (size_t j = 0; j < 4; ++j)
      symmetry = std::max(symmetry, double(std::abs(p[i][j] - p[j][i])));
  if (symmetry > 1e-12)
    throw std::runtime_error("Squared factor preconditioner is not symmetric");
  // Independent dense Cholesky of P certifies SPD, no original source matrix
  // or incomplete factor arrays are exposed by the private implementation.
  long double lower[4][4]{};
  for (size_t i = 0; i < 4; ++i)
    for (size_t j = 0; j <= i; ++j) {
      long double v = p[i][j];
      for (size_t k = 0; k < j; ++k)
        v -= lower[i][k] * lower[j][k];
      if (i == j) {
        if (!(v > 0))
          throw std::runtime_error("Preconditioner positive pivot failed "
                                   "independent dense certificate");
        lower[i][j] = std::sqrt(v);
      } else
        lower[i][j] = v / lower[j][j];
    }
  struct ActualOperator {
    System &a;
    SquaredLaplacianPreconditioner &p;
    std::vector<double> diagonal;
    void apply(std::span<const double> x, std::span<double> y, Budget &b) {
      a.multiply(x, y, b);
    }
    void precondition(std::span<const double> r, std::span<double> z,
                      std::span<const uint8_t> s, Budget &b) {
      p.apply(r, z, s, b);
    }
  } op{a, preconditioner, a.diagonal};
  Dense oracle;
  for (size_t i = 0; i < 4; ++i) {
    oracle.q[i][i] = 1;
    for (size_t at = a.starts[i]; at < a.starts[i + 1]; ++at)
      oracle.q[i][a.columns[at]] = a.values[at];
  }
  std::array<uint8_t, 4> fixed{1, 0, 0, 0};
  std::array<double, 4> values{1, 0, 0, 0}, x{1, .5, .5, .5};
  auto expected = independent(oracle, fixed, values);
  BoundedQuadraticOptions controls;
  controls.relative_tolerance = 1e-11;
  auto result = solve_bounded_quadratic(op, fixed, values, x, controls, budget);
  double error = 0;
  for (size_t i = 0; i < 4; ++i)
    error = std::max(error, double(std::abs(x[i] - expected[i])));
  if (error > 2e-9)
    throw std::runtime_error(
        "Modified preconditioner changed original constrained minimum");
  std::array<uint8_t, 4> active{3, 1, 0, 0};
  for (size_t j = 0; j < 4; ++j) {
    std::array<double, 4> r{}, z{};
    r[j] = 1;
    preconditioner.apply(r, z, active, budget);
    for (size_t i = 0; i < 4; ++i) {
      long double expected = active[i] || active[j] ? 0 : p[i][j];
      if (std::abs(z[i] - expected) > 1e-12)
        throw std::runtime_error(
            "Free projection is not the SPD principal preconditioner");
    }
  }
  bool refused = false;
  try {
    SquaredLaplacianPreconditioner shortcap(
        a, none, preconditioner.planned_bytes - 1, budget);
  } catch (const std::length_error &) {
    refused = true;
  }
  if (!refused)
    throw std::runtime_error("Incomplete factor byte cap ignored");
  std::cout << "PASS signed-SPD negative-pivot control, independent "
               "SPD/symmetry/free projection, unchanged exact constrained "
               "minimum maxerror="
            << error << " kkt=" << result.kkt_residual
            << " modified=" << preconditioner.modified_pivots << '\n';
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
