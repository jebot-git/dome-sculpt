// SPDX-License-Identifier: GPL-3.0-or-later
#include "weight-nearest-support.hpp"
#include "weight-surface-system.hpp"
#include <iostream>
using namespace dome::detail;
int main() try {
  size_t checks = 0;
  const std::function<bool()> never = [] { return false; };
  weight_surface::Budget work{0, 50'000'000, never};
  auto check = [&](bool ok, const char *why) {
    ++checks;
    if (!ok)
      throw std::runtime_error(why);
  };
  using P = std::array<float, 3>;
  auto mask = [](uint64_t a) { return std::array<uint64_t, 4>{a, 0, 0, 0}; };
  for (float scale : {.25f, 1.f, 2.f})
    for (bool turn : {false, true})
      for (P shift : {P{0, 0, 0}, P{1024, -256, 128}}) {
        auto transform = [&](P p) {
          if (turn)
            p = {-p[1], p[0], p[2]};
          for (size_t k = 0; k < 3; ++k)
            p[k] = p[k] * scale + shift[k];
          return p;
        };
        auto d = [](P p) { return std::array<double, 3>{p[0], p[1], p[2]}; };
        std::vector<HeatSourceSegment> sources{
            {d(transform({-1, 0, 0})), d(transform({-1, 2, 0})), 0},
            {d(transform({1, 0, 0})), d(transform({1, 2, 0})), 2}};
        NearestWeightSources q(sources, work);
        check(q.nearest(transform({0, 1, 0}), work) == mask(5),
              "Exact interior tie includes both genuine controls");
        check(q.nearest(transform({.125f, 1, 0}), work) == mask(4),
              "Actual nearer finite segment wins");
        check(q.nearest(transform({-.125f, 3, 0}), work) == mask(1),
              "Endpoint region uses actual endpoint distance");
        check(q.nearest(transform({0, 3, 0}), work) == mask(5),
              "Exact endpoint tie includes both controls");
        std::reverse(sources.begin(), sources.end());
        NearestWeightSources reverse(sources, work);
        check(reverse.nearest(transform({.125f, 1, 0}), work) ==
                  q.nearest(transform({.125f, 1, 0}), work),
              "Control permutation cannot resolve tie/nearest differently");
        sources.front().bone = 100;
        check(q.nearest(transform({.125f, 1, 0}), work) == mask(4),
              "Query owns detached captured control identity");
      }
  std::vector<HeatSourceSegment> extreme{
      {{-10000, -10000, 0}, {-10000, 10000, 0}, 0},
      {{10000, -10000, 0}, {10000, 10000, 0}, 2}};
  NearestWeightSources q(extreme, work);
  check(q.nearest({0, 0, 0}, work) == mask(5),
        "Extreme exact rational tie within bounded capacity");
  check(q.nearest({std::numeric_limits<float>::denorm_min(), 0, 0}, work) ==
            mask(4),
        "Subnormal real nearer source cannot disappear in rounded squared "
        "distances");
  check(q.nearest({-std::numeric_limits<float>::denorm_min(), 0, 0}, work) ==
            mask(1),
        "Subnormal mirrored nearer source retains sign");
  bool limited = false;
  try {
    weight_surface::Budget short_work{0, 1, never};
    (void)q.nearest({0, 0, 0}, short_work);
  } catch (const std::length_error &) {
    limited = true;
  }
  check(limited, "Exact query work bound enforced");
  bool cancelled = false;
  const std::function<bool()> stop = [] { return true; };
  try {
    weight_surface::Budget stopped{0, 50'000'000, stop};
    (void)q.nearest({0, 0, 0}, stopped);
  } catch (const std::runtime_error &) {
    cancelled = true;
  }
  check(cancelled, "Exact query cancellation honored before work");
  std::cout << "PASS exact_nearest_controls=" << checks << " work=" << work.used
            << '\n';
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
