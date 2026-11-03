// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/screened-continuation-solve.hpp"
#include "../src/screened-sparse-ldl.hpp"
#include <array>
#include <iostream>
#include <map>
#include <random>
using namespace dome::detail;
struct Work {
  size_t used = 0, maximum = 500'000'000, cancel_at = SIZE_MAX;
  void stop() const {
    if (used >= cancel_at)
      throw std::runtime_error("cancelled");
  }
  void step(size_t n = 1) {
    if (n > maximum - used)
      throw std::length_error("work");
    used += n;
    stop();
  }
};
struct Matrix {
  std::vector<size_t> starts;
  std::vector<uint32_t> columns;
  std::vector<double> values, diagonal, mass;
  void multiply(std::span<const double> x, std::span<double> y, Work &w) const {
    w.step(x.size() + columns.size());
    for (size_t i = 0; i < x.size(); ++i) {
      y[i] = diagonal[i] * x[i];
      for (size_t at = starts[i]; at < starts[i + 1]; ++at)
        y[i] += values[at] * x[columns[at]];
    }
  }
};
// Independent long-double dense Gaussian elimination, not the sparse factor
// or PCG implementation under test.
std::vector<long double> dense(std::vector<std::vector<long double>> a,
                               std::vector<long double> b) {
  for (size_t k = 0; k < b.size(); ++k) {
    size_t pivot = k;
    for (size_t i = k + 1; i < b.size(); ++i)
      if (std::abs(a[i][k]) > std::abs(a[pivot][k]))
        pivot = i;
    std::swap(a[k], a[pivot]);
    std::swap(b[k], b[pivot]);
    if (!a[k][k])
      throw std::runtime_error("singular dense oracle");
    for (size_t i = k + 1; i < b.size(); ++i) {
      long double f = a[i][k] / a[k][k];
      for (size_t j = k; j < b.size(); ++j)
        a[i][j] -= f * a[k][j];
      b[i] -= f * b[k];
    }
  }
  std::vector<long double> x(b.size());
  for (size_t i = b.size(); i-- > 0;) {
    long double v = b[i];
    for (size_t j = i + 1; j < b.size(); ++j)
      v -= a[i][j] * x[j];
    x[i] = v / a[i][i];
  }
  return x;
}
int main() try {
  std::mt19937 random(0x54bc91);
  double maximum = 0, ic_maximum = 0;
  size_t cases = 0, work = 0;
  for (size_t sample = 0; sample < 128; ++sample) {
    const size_t n = 3 + sample % 18, labels = 3;
    std::vector<std::vector<long double>> a(n, std::vector<long double>(n));
    std::vector<std::vector<double>> b(labels, std::vector<double>(n));
    for (size_t i = 0; i < n; ++i) {
      for (size_t j = i + 1; j < n; ++j)
        if (j == i + 1 || random() % 4 == 0) {
          double c = .1 + double(random() % 100) / 50;
          a[i][i] += c;
          a[j][j] += c;
          a[i][j] -= c;
          a[j][i] -= c;
        }
      for (size_t l = 0; l < labels; ++l) {
        double c = (i == l || random() % 3 == 0)
                       ? .01 + double(random() % 100) / 100
                       : 0;
        b[l][i] = c;
        a[i][i] += c;
      }
    }
    Matrix s;
    s.starts.push_back(0);
    s.mass.assign(n, 1);
    for (size_t i = 0; i < n; ++i) {
      s.diagonal.push_back(double(a[i][i]));
      for (size_t j = 0; j < n; ++j)
        if (i != j && a[i][j] != 0) {
          s.columns.push_back(j);
          s.values.push_back(double(a[i][j]));
        }
      s.starts.push_back(s.columns.size());
    }
    Work budget;
    ScreenedSparseLDL factor(s, 16u * 1024u * 1024u, budget);
    std::vector<uint8_t> status(n);
    struct Inverse {
      ScreenedSparseLDL &factor;
      std::vector<uint8_t> &status;
      void apply(std::span<const double> r, std::span<double> z, Work &w) {
        factor.apply(r, z, status, w);
      }
    } inverse{factor, status};
    ContinuationIC<Matrix, Work> ic(s, budget);
    std::vector<double> sum(n);
    for (size_t l = 0; l < labels; ++l) {
      auto expected =
          dense(a, std::vector<long double>(b[l].begin(), b[l].end()));
      std::vector<double> x(n), control(n);
      ContinuationReport report, ic_report;
      continuation_solve(s, b[l], x, inverse, budget, report);
      continuation_solve(s, b[l], control, ic, budget, ic_report);
      for (size_t i = 0; i < n; ++i) {
        maximum = std::max(maximum, double(std::abs(expected[i] - x[i])));
        ic_maximum =
            std::max(ic_maximum, double(std::abs(expected[i] - control[i])));
        if (x[i] < -1e-9 || x[i] > 1 + 1e-9)
          throw std::runtime_error("maximum principle");
        sum[i] += x[i];
      }
    }
    for (double value : sum)
      if (std::abs(value - 1) > 1e-8)
        throw std::runtime_error("partition");
    if (factor.modified_pivots)
      throw std::runtime_error("modified pivot");
    Work short_work;
    short_work.maximum = 1;
    bool refused = false;
    try {
      ScreenedSparseLDL fail(s, 16384, short_work);
    } catch (const std::length_error &) {
      refused = true;
    }
    if (!refused)
      throw std::runtime_error("missing work refusal");
    Work cancelled;
    cancelled.cancel_at = 16;
    refused = false;
    try {
      ScreenedSparseLDL fail(s, 16u * 1024u * 1024u, cancelled);
    } catch (const std::runtime_error &) {
      refused = true;
    }
    if (!refused)
      throw std::runtime_error("missing cancel refusal");
    work += budget.used;
    ++cases;
  }
  if (maximum > 1e-9 || ic_maximum > 1e-8)
    throw std::runtime_error("independent dense error");
  std::cout << "PASS cases=" << cases << " exact_factor_dense_max=" << maximum
            << " IC_dense_max=" << ic_maximum << " work=" << work
            << " original_residual=1e-9 partition_max_bound=1e-8 "
               "pivot_changes=0 cancellation=128 budget_refusals=128\n";
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
