// SPDX-License-Identifier: GPL-3.0-or-later
#include "fixed-quadratic.hpp"
#include <array>
#include <functional>
#include <iostream>
using namespace dome::detail;
struct Budget {
  size_t used = 0, maximum = 1'000'000;
  std::function<bool()> cancel;
  void stop() {
    if (cancel && cancel())
      throw std::runtime_error("cancelled");
  }
  void step(size_t n) {
    if (n > maximum - used)
      throw std::length_error("work");
    used += n;
    stop();
  }
};
struct Operator {
  const std::array<std::array<double, 4>, 4> q{
      {{3, -1, 1, 0}, {-1, 2, 1, 0}, {1, 1, 2, 0}, {0, 0, 0, 1}}};
  std::array<double, 4> diagonal{3, 2, 2, 1};
  void apply(std::span<const double> x, std::span<double> out, Budget &b) {
    b.step(16);
    for (int i = 0; i < 4; ++i) {
      out[i] = 0;
      for (int j = 0; j < 4; ++j)
        out[i] += q[i][j] * x[j];
    }
  }
};
int main() try {
  Operator op;
  std::array<uint8_t, 4> fixed{1, 0, 0, 1};
  std::array<double, 4> values{1, 0, 0, 0}, field{1, .5, .5, 0};
  Budget budget;
  BoundedQuadraticOptions o;
  o.relative_tolerance = 1e-11;
  auto unbounded = solve_fixed_quadratic(op, fixed, values, field, o, budget);
  if (std::abs(field[1] - 1) > 1e-10 || std::abs(field[2] + 1) > 1e-10 ||
      unbounded.kkt_residual > o.relative_tolerance)
    throw std::runtime_error(
        "Unconstrained field differs from exact analytic1/-1");
  for (auto &v : field)
    v = std::clamp(v, 0., 1.);
  if (std::abs(field[1] - .5) < .4)
    throw std::runtime_error("Clipped control lost its known wrong optimum");
  auto bounded = solve_bounded_quadratic(op, fixed, values, field, o, budget);
  if (std::abs(field[1] - .5) > 1e-10 || field[2] != 0 ||
      bounded.kkt_residual > o.relative_tolerance)
    throw std::runtime_error("Warm-start solve accepted wrong clipped minimum");
  bool refused = false;
  auto shortcap = o;
  shortcap.maximum_bytes = 1;
  auto before = field;
  try {
    solve_fixed_quadratic(op, fixed, values, field, shortcap, budget);
  } catch (const std::length_error &) {
    refused = true;
  }
  if (!refused || field != before)
    throw std::runtime_error("Fixed-solve byte admission changed output");
  Budget cancelled;
  cancelled.cancel = [] { return true; };
  refused = false;
  try {
    solve_fixed_quadratic(op, fixed, values, field, o, cancelled);
  } catch (const std::runtime_error &) {
    refused = true;
  }
  if (!refused || field != before)
    throw std::runtime_error(
        "Fixed-solve cancellation admission changed output");
  std::cout
      << "PASS analytic unbounded1/-1, wrong clipped1/0 negative corrected to "
         "true bounded.5/0, actual KKT, byte/cancel admission; warmcg="
      << unbounded.cg_iterations << " active=" << bounded.active_iterations
      << '\n';
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
