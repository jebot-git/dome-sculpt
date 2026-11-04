// SPDX-License-Identifier: GPL-3.0-or-later
#include "bone-shadow-query.hpp"
#include <iostream>
using namespace dome::detail;
struct Work {
  size_t used = 0, maximum = 50000000;
  void stop() const {}
  void step(size_t n = 1) {
    if (n > maximum - used)
      throw std::length_error("work");
    used += n;
  }
};
using P = bone_shadow::Point;
struct Outcome {
  double value = 0;
  int kind = -1;
  size_t candidates = 0, work = 0;
};
int main() try {
  size_t checks = 0, oldwork = 0, newwork = 0;
  auto check = [&](bool v, const char *s) {
    ++checks;
    if (!v)
      throw std::runtime_error(s);
  };
  // Point is outside this triangle but inside its exact planar AABB. This
  // distinguishes the new plane certificate from ordinary AABB separation.
  for (int axis = 0; axis < 3; ++axis)
    for (float sign : {-1.f, 1.f}) {
      auto turn = [&](P p) {
        P q{};
        for (int k = 0; k < 3; ++k)
          q[(k + axis) % 3] = p[k] * (k == 1 ? 1.f : sign);
        return q;
      };
      auto compare = [&](P row, P head, P tail, bool expect_visible,
                         bool expect_contact) {
        std::vector<P> points{turn(row), turn({0, 0, 0}), turn({1, 0, 0}),
                              turn({0, 1, 0})};
        const std::vector<uint32_t> ix{1, 2, 3};
        Work build;
        BoneShadowQuery query(points, ix, 16u * 1024u * 1024u, build);
        auto run = [&](bool filter) {
          Work w;
          BoneShadowQuery::Scratch s(query);
          BoneQueryReport r;
          Outcome o;
          try {
            o.value =
                query.integrate(0, turn(head), turn(tail), .1, 0, 1, s, w, r,
                                BoneWallDomain::WholeGroup, filter, false);
          } catch (const BoneQueryAmbiguity &e) {
            o.kind = int(e.kind);
          }
          o.candidates = r.candidates;
          o.work = w.used;
          return o;
        };
        auto fast = run(true), exact = run(false);
        check(fast.kind == exact.kind,
              "Planar cull erased or invented contact ambiguity");
        check(fast.value == exact.value,
              "Planar cull differs from exact complete shadow");
        check(expect_contact ? fast.kind >= 0 : fast.kind < 0,
              "Wrong independent contact expectation");
        if (expect_visible) {
          check(fast.value > 0, "Visible finite source was blocked");
          check(fast.candidates == 0 && exact.candidates == 1,
                "Planar node shortcut not sensitive to exact control");
          check(fast.work < exact.work,
                "Planar node cull adds work without skipping predicates");
          oldwork += exact.work;
          newwork += fast.work;
        }
        return fast;
      };
      compare({.75, .75, 0}, {.75, .75, -1}, {.75, 1.5, -1}, true, false);
      compare({.75, .75, 0}, {.75, .75, -1}, {.75, .75, -1}, true, false);
      // Foreign origin remains a refusal before ANY source fan cull.
      compare({.25, .25, 0}, {.25, .25, -1}, {.25, .5, -1}, false, true);
      // A source touching/crossing the plane cannot receive the shortcut.
      compare({.75, .75, 0}, {.25, .25, 0}, {.25, .25, -1}, false, true);
      compare({.75, .75, 0}, {.25, .25, 1}, {.25, .25, -1}, false, true);
      const float gap = std::ldexp(1.f, -30);
      auto thin =
          compare({.25, .25, gap}, {.25, .25, -1}, {.25, .5, -1}, false, false);
      std::cout << "THIN axis=" << axis << " sign=" << sign
                << " value=" << thin.value << " kind=" << thin.kind
                << std::endl;
      check(thin.value == 0,
            "Real subcell wall was culled as a zero-thickness origin plane");
    }
  std::cout << "PASS planar_node_checks=" << checks << " exact_work=" << oldwork
            << " culled_work=" << newwork << '\n';
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
