// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/bone-shadow-filter.hpp"
#include <iostream>
#include <random>
using namespace dome::detail;
struct Work {
  size_t used = 0, maximum = 500'000'000;
  void stop() const {}
  void step(size_t n = 1) {
    if (n > maximum - used)
      throw std::length_error("work");
    used += n;
  }
};
int main() try {
  using P = bone_shadow::Point;
  using T = bone_shadow::Triangle;
  Work work;
  bone_shadow::FilterReport report;
  volume_exact::Arithmetic exact(work);
  size_t cases = 0;
  auto compare = [&](P row, P head, P tail, T face) {
    auto old = bone_shadow::triangle_shadow(row, head, tail, face, work);
    auto current = bone_shadow::filtered_triangle_shadow(row, head, tail, face,
                                                         work, report);
    if (old.kind != current.kind ||
        (old.kind == bone_shadow::Kind::Interval &&
         (exact.compare(old.interval.enter, current.interval.enter) ||
          exact.compare(old.interval.exit, current.interval.exit))))
      throw std::runtime_error("certified filter changed exact result");
    ++cases;
  };
  std::mt19937 random(0xb8f440);
  for (size_t n = 0; n < 4096; ++n) {
    auto scalar = [&] {
      if (n < 1024)
        return float(int(random() % 129) - 64) / 32;
      return std::ldexp(1.f + float(random() % 8388608) / 8388608,
                        int(random() % 151) - 140) *
             (random() % 2 ? -1 : 1);
    };
    auto point = [&] { return P{scalar(), scalar(), scalar()}; };
    auto row = point(), head = point(), tail = n % 5 ? point() : head;
    compare(row, head, tail, {point(), point(), point()});
  }
  const T face{P{-2, -2, 0}, P{2, -2, 0}, P{0, 2, 0}};
  compare({0, 0, 1}, {0, 0, -1}, {0, 0, -1}, face);
  compare({0, 0, 1}, {-3, 0, -1}, {3, 0, -1}, face);
  compare({0, 0, 0}, {-3, 0, -1}, {3, 0, -1}, face);
  compare({-3, 0, 0}, {3, 0, 0}, {4, 0, 0}, face);
  compare({-3, 0, 0}, {0, 0, -1}, {0, 0, 1}, face);
  const auto previous = std::fegetround();
  if (std::fesetround(FE_UPWARD))
    throw std::runtime_error("rounding test unavailable");
  if (bone_shadow::filtered_orientation(face[0], face[1], face[2], P{0, 0, 1},
                                        work))
    throw std::runtime_error("non-nearest filter was used");
  compare({0, 0, 1}, {-3, 0, -1}, {3, 0, -1}, face);
  std::fesetround(previous);
  if (!report.exact || !report.rejected || !report.full)
    throw std::runtime_error("unexercised filter branch");
  std::cout
      << "PASS exact_equivalence=" << cases << " exact_calls=" << report.exact
      << " certified_misses=" << report.rejected
      << " certified_full=" << report.full << " work=" << work.used
      << " original_ambiguities_preserved=1 rounding_guard=1 field_claim=0\n";
} catch (const std::exception &e) {
  std::fesetround(FE_TONEAREST);
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
