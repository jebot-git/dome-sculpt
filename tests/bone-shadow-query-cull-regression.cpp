// SPDX-License-Identifier: GPL-3.0-or-later
#include "../src/bone-shadow-query.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
namespace {
thread_local long fail_after = -1;
thread_local size_t allocations = 0, hits = 0;
void *allocate(size_t n) {
  if (fail_after >= 0) {
    if (!fail_after) {
      ++hits;
      throw std::bad_alloc();
    }
    --fail_after;
  }
  ++allocations;
  if (void *p = std::malloc(n ? n : 1))
    return p;
  throw std::bad_alloc();
}
} // namespace
void *operator new(size_t n) { return allocate(n); }
void *operator new[](size_t n) { return allocate(n); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
void operator delete[](void *p, size_t) noexcept { std::free(p); }
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
using P = bone_shadow::Point;
int main() try {
  Work work;
  std::vector<P> points{P{0, 0, 1}, P{-2, -2, 0}, P{2, -2, 0}, P{0, 2, 0}};
  std::vector<uint32_t> indices{1, 2, 3};
  const auto before = points;
  const auto old_indices = indices;
  BoneShadowQuery query(points, indices, 16u * 1024u * 1024u, work);
  BoneShadowQuery::Scratch scratch(query);
  BoneQueryReport report;
  const P head{-3, 0, -1}, tail{3, 0, -1};
  double value =
      query.integrate(0, head, tail, .1, 0, 1, scratch, work, report);
  const std::array<double, 3> row_d{0, 0, 1}, head_d{-3, 0, -1},
      tail_d{3, 0, -1};
  double expected =
      bone_line_integral(row_d, head_d, tail_d, .1, 0, 1. / 6, work) +
      bone_line_integral(row_d, head_d, tail_d, .1, 5. / 6, 1, work);
  if (std::abs(value - expected) > 1e-13 || value > expected + 1e-14 ||
      report.unions != 1)
    throw std::runtime_error("analytic BVH/interval integration");
  points[1] = {-20, -20, -20};
  indices[0] = 0;
  BoneQueryReport repeat;
  if (query.integrate(0, head, tail, .1, 0, 1, scratch, work, repeat) != value)
    throw std::runtime_error("caller alias leaked into owned BVH");
  points = before;
  indices = old_indices;
  points.push_back({.375f, -1, 0});
  points.push_back({.625f, -1, 0});
  points.push_back({.5f, 1, 0});
  indices.insert(indices.end(), {4, 5, 6});
  // Duplicate/nested occluders may not subtract visible energy twice.
  indices.insert(indices.end(), old_indices.begin(), old_indices.end());
  BoneShadowQuery overlap(points, indices, 16u * 1024u * 1024u, work);
  BoneShadowQuery::Scratch overlap_scratch(overlap);
  BoneQueryReport overlap_report;
  if (overlap.integrate(0, head, tail, .1, 0, 1, overlap_scratch, work,
                        overlap_report) != value ||
      overlap_report.unions != 1 || overlap_report.intervals != 3)
    throw std::runtime_error("complete exact shadow union");
  // Two disjoint thin shadows leave a genuine central visible gap. Neither
  // broad-phase bounds nor numerical union may fabricate a bridge over it.
  points = {P{0, 0, 1},      P{-.625f, -1, 0}, P{-.375f, -1, 0}, P{-.5f, 1, 0},
            P{.375f, -1, 0}, P{.625f, -1, 0},  P{.5f, 1, 0}};
  indices = {1, 2, 3, 4, 5, 6};
  BoneShadowQuery gaps(points, indices, 16u * 1024u * 1024u, work);
  BoneShadowQuery::Scratch gap_scratch(gaps);
  BoneQueryReport gap_report;
  double g =
      gaps.integrate(0, head, tail, .1, 0, 1, gap_scratch, work, gap_report);
  if (!(g > value) || gap_report.unions != 2)
    throw std::runtime_error("positive gap disappeared");
  const auto pre_cancel = gap_report;
  Work small;
  small.maximum = 1;
  bool refused = false;
  try {
    BoneShadowQuery fail(points, indices, 16u * 1024u * 1024u, small);
  } catch (const std::length_error &) {
    refused = true;
  }
  if (!refused)
    throw std::runtime_error("missing construction forecast");
  Work cancelled;
  cancelled.cancel_at = 1;
  refused = false;
  try {
    gaps.integrate(0, head, tail, .1, 0, 1, gap_scratch, cancelled, gap_report);
  } catch (const std::runtime_error &) {
    refused = true;
  }
  if (!refused)
    throw std::runtime_error("missing query cancellation");
  if (gap_report.nodes != pre_cancel.nodes ||
      gap_report.candidates != pre_cancel.candidates ||
      gap_report.intervals != pre_cancel.intervals ||
      gap_report.unions != pre_cancel.unions)
    throw std::runtime_error("partial report publication on cancel");
  Work retry;
  BoneQueryReport retry_report;
  if (gaps.integrate(0, head, tail, .1, 0, 1, gap_scratch, retry,
                     retry_report) != g)
    throw std::runtime_error("cancel retry changed immutable query");
  size_t measured = allocations;
  {
    Work measured_work;
    BoneShadowQuery q(points, indices, 16u * 1024u * 1024u, measured_work);
    BoneShadowQuery::Scratch s(q);
    BoneQueryReport r;
    if (q.integrate(0, head, tail, .1, 0, 1, s, measured_work, r) != g)
      throw std::runtime_error("measurement input changed");
  }
  measured = allocations - measured;
  const auto unchanged_points = points;
  const auto unchanged_indices = indices;
  for (size_t boundary = 0; boundary < measured; ++boundary) {
    bool caught = false;
    const auto old_hits = hits;
    fail_after = long(boundary);
    try {
      Work fault;
      BoneShadowQuery q(points, indices, 16u * 1024u * 1024u, fault);
      BoneShadowQuery::Scratch s(q);
      BoneQueryReport r;
      (void)q.integrate(0, head, tail, .1, 0, 1, s, fault, r);
    } catch (const std::bad_alloc &) {
      caught = true;
    }
    fail_after = -1;
    if (!caught || hits == old_hits || points != unchanged_points ||
        indices != unchanged_indices)
      throw std::runtime_error(
          "persistent allocation boundary did not preserve input");
  }
  Work noalloc;
  BoneQueryReport noalloc_report;
  bool allocated = false;
  fail_after = 0;
  try {
    if (gaps.integrate(0, head, tail, .1, 0, 1, gap_scratch, noalloc,
                       noalloc_report) != g)
      allocated = true;
  } catch (const std::bad_alloc &) {
    allocated = true;
  }
  fail_after = -1;
  if (allocated)
    throw std::runtime_error("admitted query allocated");
  // A real connected folded surface retains its wall in the explicit local
  // domain. Selecting a domain cannot erase its own opposite/entry face.
  auto connected_points = before;
  auto connected_indices = old_indices;
  connected_indices.insert(connected_indices.end(), {0, 2, 1});
  Work folded_work;
  BoneShadowQuery folded(connected_points, connected_indices,
                         16u * 1024u * 1024u, folded_work);
  BoneShadowQuery::Scratch folded_scratch(folded);
  BoneQueryReport folded_report;
  if (folded.integrate(0, head, tail, .1, 0, 1, folded_scratch, folded_work,
                       folded_report,
                       BoneWallDomain::ConnectedSourceSurface) != value)
    throw std::runtime_error("connected self-wall was bypassed");
  // Exact positive gaps below representable endpoint precision must refuse,
  // rather than being silently merged/coarsened into an opaque interval.
  const float eps = 1e-40f;
  std::vector<P> tiny_points{P{0, 0, 1},  P{-2, -2, 0}, P{-eps, 0, 0},
                             P{-2, 2, 0}, P{eps, 0, 0}, P{2, -2, 0},
                             P{2, 2, 0}};
  std::vector<uint32_t> tiny_indices{1, 2, 3, 4, 5, 6};
  Work tiny_work;
  BoneShadowQuery tiny(tiny_points, tiny_indices, 16u * 1024u * 1024u,
                       tiny_work);
  BoneShadowQuery::Scratch tiny_scratch(tiny);
  BoneQueryReport tiny_report;
  refused = false;
  try {
    (void)tiny.integrate(0, P{-1, 0, -1}, P{1, 0, -1}, .1, 0, 1, tiny_scratch,
                         tiny_work, tiny_report);
  } catch (const std::runtime_error &e) {
    refused = std::string(e.what()).find("Unresolved") != std::string::npos;
  }
  if (!refused)
    throw std::runtime_error("unresolved positive gap was hidden");
  // A complete opaque shadow may stop later ray work, but it must NEVER hide
  // an unowned coincident origin. Origin ownership is independently complete.
  std::vector<P> owned_points{P{0, 0, 1},  P{-16, -16, 0}, P{16, -16, 0},
                              P{0, 16, 0}, P{-1, -1, 1},   P{1, -1, 1},
                              P{0, 1, 1}};
  std::vector<uint32_t> owned_indices{1, 2, 3, 4, 5, 6};
  Work origin_work;
  BoneShadowQuery origin(owned_points, owned_indices, 16u * 1024u * 1024u,
                         origin_work);
  BoneShadowQuery::Scratch origin_scratch(origin);
  BoneQueryReport origin_report;
  refused = false;
  try {
    (void)origin.integrate(0, P{0, 0, -1}, P{0, 0, -1}, .1, 0, 1,
                           origin_scratch, origin_work, origin_report);
  } catch (const BoneQueryAmbiguity &e) {
    refused = e.kind == bone_shadow::Kind::OriginContact;
  }
  if (!refused || origin_report.nodes || origin_report.intervals)
    throw std::runtime_error("opaque shadow hid unowned origin");
  // Exact full coverage is a mathematical early-exit certificate, compared
  // against the complete deferred-union/exact-predicate control.
  std::vector<P> opaque_points{P{0, 0, 1}};
  std::vector<uint32_t> opaque_indices;
  for (uint32_t i = 0; i < 32; ++i) {
    const float z = -float(i) / 32;
    const auto base = uint32_t(opaque_points.size());
    opaque_points.insert(opaque_points.end(),
                         {P{-16, -16, z}, P{16, -16, z}, P{0, 16, z}});
    opaque_indices.insert(opaque_indices.end(), {base, base + 1, base + 2});
  }
  Work opaque_build;
  BoneShadowQuery opaque(opaque_points, opaque_indices, 16u * 1024u * 1024u,
                         opaque_build);
  BoneShadowQuery::Scratch opaque_fast(opaque), opaque_full(opaque);
  BoneQueryReport opaque_fast_report, opaque_full_report;
  Work opaque_fast_work, opaque_full_work;
  const double early =
      opaque.integrate(0, P{0, 0, -2}, P{0, 0, -2}, .1, 0, 1, opaque_fast,
                       opaque_fast_work, opaque_fast_report);
  const double exhaustive = opaque.integrate(
      0, P{0, 0, -2}, P{0, 0, -2}, .1, 0, 1, opaque_full, opaque_full_work,
      opaque_full_report, BoneWallDomain::WholeGroup, false, false);
  if (early || exhaustive || opaque_fast_report.intervals != 1 ||
      opaque_full_report.intervals != 32 ||
      opaque_fast_work.used >= opaque_full_work.used)
    throw std::runtime_error("exact complete-coverage pruning control");
  uint32_t random = 0x9e3779b9;
  auto sample = [&]() {
    random ^= random << 13;
    random ^= random >> 17;
    random ^= random << 5;
    return float(int(random % 65) - 32) / 8;
  };
  double maximum_control_error = 0;
  for (size_t trial = 0; trial < 128; ++trial) {
    std::vector<P> p{P{0, 0, 1}};
    std::vector<uint32_t> ix;
    for (size_t wall = 0; wall < 12; ++wall) {
      const float x = sample(), y = sample(),
                  width = (std::abs(sample()) + 1) / 4;
      const uint32_t first = uint32_t(p.size());
      p.insert(p.end(), {P{x - width, y - width, 0}, P{x + width, y - width, 0},
                         P{x, y + width, 0}});
      ix.insert(ix.end(), {first, first + 1, first + 2});
    }
    Work build;
    BoneShadowQuery q(p, ix, 16u * 1024u * 1024u, build);
    BoneShadowQuery::Scratch a(q), b(q);
    BoneQueryReport ar, br;
    Work aw, bw;
    P h{sample(), sample(), -1}, t = trial % 3 ? P{sample(), sample(), -1} : h;
    double left = double(trial % 3) / 8, right = 1 - double(trial % 5) / 16;
    double actual = q.integrate(0, h, t, .1, left, right, a, aw, ar);
    double reference = q.integrate(0, h, t, .1, left, right, b, bw, br,
                                   BoneWallDomain::WholeGroup, false, false);
    maximum_control_error =
        std::max(maximum_control_error, std::abs(actual - reference));
    if (std::abs(actual - reference) > 1e-13)
      throw std::runtime_error("filtered/incremental vs exact complete query");
  }
  points = before;
  indices = old_indices;
  if (points != before || indices != old_indices)
    throw std::runtime_error("input changed");
  std::cout << "PASS owning_BVH=1 exact_union=1 outward_integral_error="
            << std::abs(value - expected)
            << " duplicate_nested=1 positive_gap=1 work=" << work.used
            << " allocation_boundaries=" << measured
            << " admitted_query_noalloc=1 connected_wall=1 "
               "unresolved_gap_refusal=1 opaque_origin_refusal=1 "
               "complete_control=128 control_error="
            << maximum_control_error << " opaque_work=" << opaque_fast_work.used
            << "/" << opaque_full_work.used << " source_field_claim=0\n";
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
