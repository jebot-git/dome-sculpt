// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/bone-line-integral.hpp"
#include <algorithm>
#include <iostream>
#include <numbers>
#include <random>
#include <vector>
using P = std::array<double, 3>;
struct Work {
  size_t used = 0, maximum = 500'000'000;
  bool cancel = false;
  void stop() const {
    if (cancel)
      throw std::runtime_error("cancelled");
  }
  void step(size_t n) {
    if (n > maximum - used)
      throw std::length_error("work");
    used += n;
    stop();
  }
};
// Independent long-double Gauss-Legendre quadrature. Spatial bands around
// closest approach resolve arbitrarily narrow regularized line peaks; no
// arctangent/closed-form antiderivative is reused from the native kernel.
long double oracle(P row, P head, P tail, double mass, double lo, double hi) {
  constexpr size_t order = 32;
  static const auto table = [] {
    std::array<std::array<long double, 2>, order> t{};
    for (size_t i = 0; i < order; ++i) {
      long double x = std::cos(std::numbers::pi_v<long double> * (i + .75L) /
                               (order + .5L)),
                  derivative = 0;
      for (size_t k = 0; k < 64; ++k) {
        long double p0 = 1, p1 = x;
        for (size_t n = 2; n <= order; ++n) {
          long double q = ((2 * n - 1) * x * p1 - (n - 1) * p0) / n;
          p0 = p1;
          p1 = q;
        }
        derivative = order * (x * p1 - p0) / (x * x - 1);
        long double next = x - p1 / derivative;
        if (std::abs(next - x) < 1e-19L) {
          x = next;
          break;
        }
        x = next;
      }
      t[i] = {x, 2 / ((1 - x * x) * derivative * derivative)};
    }
    return t;
  }();
  long double a = 0, b = 0, c = mass;
  std::array<long double, 3> r{}, d{};
  for (size_t k = 0; k < 3; ++k) {
    r[k] = (long double)head[k] - row[k];
    d[k] = (long double)tail[k] - head[k];
    a += d[k] * d[k];
    b += r[k] * d[k];
    c += r[k] * r[k];
  }
  std::vector<long double> cuts{lo, hi};
  if (a > 0) {
    long double center = -b / a, nearest = 0;
    for (size_t k = 0; k < 3; ++k) {
      long double x = r[k] + center * d[k];
      nearest += x * x;
    }
    long double spacing = std::sqrt((nearest + mass) / a) / 16;
    if (center > lo && center < hi)
      cuts.push_back(center);
    for (size_t k = 0; k < 256 && spacing < 4; ++k, spacing *= 2)
      for (int sign : {-1, 1}) {
        long double x = center + sign * spacing;
        if (x > lo && x < hi)
          cuts.push_back(x);
      }
  }
  std::sort(cuts.begin(), cuts.end());
  long double sum = 0;
  for (size_t n = 1; n < cuts.size(); ++n) {
    long double middle = (cuts[n] + cuts[n - 1]) / 2,
                half = (cuts[n] - cuts[n - 1]) / 2;
    for (auto [x, weight] : table) {
      long double t = middle + half * x, denominator = mass;
      for (size_t k = 0; k < 3; ++k) {
        long double v = r[k] + t * d[k];
        denominator += v * v;
      }
      sum += half * weight * mass / denominator;
    }
  }
  return sum;
}
int main() try {
  std::mt19937 random(0x7180bc);
  Work work;
  double maximum = 0, relative = 0, metamorphic = 0;
  for (size_t sample = 0; sample < 512; ++sample) {
    auto scalar = [&] { return (double(int(random() % 4097) - 2048)) / 1024; };
    P head{scalar(), scalar(), scalar()}, tail{scalar(), scalar(), scalar()},
        row{scalar(), scalar(), scalar()};
    double mass = std::pow(10., -int(sample % 13)),
           lo = double(random() % 256) / 512,
           hi = .5 + double(random() % 257) / 512;
    if (sample % 4 == 0) {
      double t = double(random() % 257) / 256;
      for (size_t k = 0; k < 3; ++k)
        row[k] = head[k] + t * (tail[k] - head[k]);
    }
    if (sample % 17 == 0)
      tail = head;
    auto value =
        dome::detail::bone_line_integral(row, head, tail, mass, lo, hi, work);
    auto expected = oracle(row, head, tail, mass, lo, hi);
    double error = double(std::abs(expected - value));
    maximum = std::max(maximum, error);
    relative = std::max(relative, error / std::max(1e-16, double(expected)));
    if (value < 0 || value > hi - lo + 1e-14)
      throw std::runtime_error("integral range");
    double middle = (lo + hi) / 2;
    auto left = dome::detail::bone_line_integral(row, head, tail, mass, lo,
                                                 middle, work);
    auto right = dome::detail::bone_line_integral(row, head, tail, mass, middle,
                                                  hi, work);
    auto reverse = dome::detail::bone_line_integral(row, tail, head, mass,
                                                    1 - hi, 1 - lo, work);
    metamorphic = std::max({metamorphic, std::abs(value - left - right),
                            std::abs(value - reverse)});
    for (double scale : {.125, 8.}) {
      auto transform = [&](P x) {
        return P{scale * (-x[2] + 16), scale * (x[0] - 8), scale * (-x[1] + 4)};
      };
      auto moved = dome::detail::bone_line_integral(
          transform(row), transform(head), transform(tail),
          mass * scale * scale, lo, hi, work);
      metamorphic = std::max(metamorphic, std::abs(value - moved));
    }
  }
  if (maximum > 4e-13 || relative > 4e-11 || metamorphic > 4e-13)
    throw std::runtime_error("independent integration/metamorphic error");
  Work small;
  small.maximum = 127;
  bool refused = false;
  try {
    dome::detail::bone_line_integral(P{}, P{}, P{1, 0, 0}, 1, 0, 1, small);
  } catch (const std::length_error &) {
    refused = true;
  }
  if (!refused)
    throw std::runtime_error("missing work refusal");
  Work cancelled;
  cancelled.cancel = true;
  refused = false;
  try {
    dome::detail::bone_line_integral(P{}, P{}, P{1, 0, 0}, 1, 0, 1, cancelled);
  } catch (const std::runtime_error &) {
    refused = true;
  }
  if (!refused)
    throw std::runtime_error("missing cancellation");
  size_t invalid = 0;
  for (double mass : {0., -1., std::numeric_limits<double>::infinity(),
                      std::numeric_limits<double>::quiet_NaN()}) {
    try {
      dome::detail::bone_line_integral(P{}, P{}, P{1, 0, 0}, mass, 0, 1, work);
    } catch (const std::invalid_argument &) {
      ++invalid;
    }
  }
  try {
    dome::detail::bone_line_integral(P{}, P{1e154, 0, 0}, P{1e154, 0, 0}, 1e308,
                                     0, 1, work);
  } catch (const std::invalid_argument &) {
    ++invalid;
  }
  if (invalid != 5)
    throw std::runtime_error("missing invalid/range refusal");
  std::cout << "PASS cases=512 quadrature_abs=" << maximum
            << " quadrature_relative=" << relative
            << " split_reverse_scale_rotation_translation=" << metamorphic
            << " work=" << work.used << " visibility_claim=0 field_claim=0\n";
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
