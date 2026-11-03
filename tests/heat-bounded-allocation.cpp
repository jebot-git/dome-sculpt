// SPDX-License-Identifier: GPL-3.0-or-later
// Persistent CPU allocation failures through the actual private bind entry.
#include "heat-binding.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <nlohmann/json.hpp>
#include <tuple>
using namespace dome;
using namespace dome::detail;
namespace {
thread_local bool tracking = false, hit = false;
thread_local size_t calls = 0, fail_at = SIZE_MAX;
void *allocate(size_t size, size_t alignment = alignof(std::max_align_t)) {
  if (tracking && calls++ >= fail_at) {
    hit = true;
    throw std::bad_alloc();
  }
  void *p = nullptr;
  if (alignment <= alignof(std::max_align_t))
    p = std::malloc(std::max(size, size_t(1)));
  else if (posix_memalign(&p, alignment, std::max(size, size_t(1))))
    p = nullptr;
  if (!p)
    throw std::bad_alloc();
  return p;
}
std::vector<std::byte> bytes(const Mesh &m) {
  std::vector<std::byte> out;
  auto add = [&](const auto &v) {
    uint64_t n = v.size();
    auto count = std::as_bytes(std::span(&n, 1));
    out.insert(out.end(), count.begin(), count.end());
    auto data = std::as_bytes(std::span(v));
    out.insert(out.end(), data.begin(), data.end());
  };
  add(m.positions);
  add(m.normals);
  add(m.uv);
  add(m.indices);
  add(m.uv_pins);
  add(m.uv_seams);
  return out;
}
auto report_fields(const HeatBindingReport &r) {
  return std::tuple(r.vertices, r.components, r.seeded_bones, r.iterations,
                    r.work, r.planned_bytes, r.negative_edges,
                    r.matrix_vertices, r.bound_sweeps,
                    r.removed_negative_stiffness, r.maximum_relative_residual,
                    r.minimum_unclamped_weight, r.maximum_partition_error,
                    r.maximum_kkt_residual);
}
} // namespace
void *operator new(size_t n) { return allocate(n); }
void *operator new[](size_t n) { return allocate(n); }
void *operator new(size_t n, std::align_val_t a) {
  return allocate(n, size_t(a));
}
void *operator new[](size_t n, std::align_val_t a) {
  return allocate(n, size_t(a));
}
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
void operator delete[](void *p, size_t) noexcept { std::free(p); }
void operator delete(void *p, std::align_val_t) noexcept { std::free(p); }
void operator delete[](void *p, std::align_val_t) noexcept { std::free(p); }
void operator delete(void *p, size_t, std::align_val_t) noexcept {
  std::free(p);
}
void operator delete[](void *p, size_t, std::align_val_t) noexcept {
  std::free(p);
}
int main() try {
  Mesh m;
  m.positions = {{0, 0, 0}, {20, 0, 0}, {5, 1, 0}, {0, 20, 0}};
  m.normals.assign(4, {0, 0, 1});
  m.uv.assign(4, {0, 0});
  m.indices = {0, 1, 2, 0, 2, 3};
  Skeleton rig;
  rig.bones.resize(2);
  rig.bones[0].name = "A";
  rig.bones[1].name = "B";
  rig.bones[0].rest.translation = {0, 0, -.1f};
  rig.bones[1].rest.translation = {20, 0, -.1f};
  HeatBindingOptions options;
  options.operation = HeatOperator::BoundedFiniteElements;
  options.maximum_seed_distance = 100;
  // Four genuine synthetic point handles give A=L+I and b=(1,0,0,0)
  // for bone A. This is the independent negative-potential case above, so
  // actual entry/failure tests must exercise bound optimization, not just an
  // already feasible unconstrained field. These are not artist-source IDs.
  const std::array<HeatSourceSegment, 4> sources{
      {{{0, 0, -std::sqrt(20.) * 1.0005}, {0, 0, -std::sqrt(20.) * 1.0005}, 0},
       {{20, 0, -std::sqrt(10. / 3) * 1.0005},
        {20, 0, -std::sqrt(10. / 3) * 1.0005},
        1},
       {{5, 1, -std::sqrt(20.) * 1.0005}, {5, 1, -std::sqrt(20.) * 1.0005}, 1},
       {{0, 20, -std::sqrt(50. / 3) * 1.0005},
        {0, 20, -std::sqrt(50. / 3) * 1.0005},
        1}}};
  options.seed_rule = HeatSeedRule::ReferenceSegments;
  const auto mesh_before = bytes(m);
  const auto rig_before = rig.encode();
  const auto position_owner = m.positions.data(),
             normal_owner = m.normals.data();
  const auto joint_owner = rig.bones.data();
  HeatBindingReport sentinel;
  sentinel.vertices = 31;
  sentinel.work = 173;
  sentinel.maximum_kkt_residual = .125;
  auto unchanged = [&](const HeatBindingReport &r) {
    if (report_fields(r) != report_fields(sentinel) ||
        bytes(m) != mesh_before || rig.encode() != rig_before ||
        m.positions.data() != position_owner ||
        m.normals.data() != normal_owner || rig.bones.data() != joint_owner)
      throw std::runtime_error(
          "Refusal/failure changed exact source owners, values or report");
  };
  HeatBindingReport measured;
  size_t checkpoints = 0;
  calls = 0;
  fail_at = SIZE_MAX;
  hit = false;
  tracking = true;
  auto expected = heat_bind_surface(
      m, rig, options,
      [&] {
        ++checkpoints;
        return false;
      },
      &measured, sources);
  tracking = false;
  if (!measured.bound_sweeps)
    throw std::runtime_error(
        "Actual allocation fixture did not exercise coordinate optimization");
  const size_t allocations = calls;
  for (size_t prefix = 0; prefix < allocations; ++prefix) {
    auto report = sentinel;
    bool failed = false;
    calls = 0;
    fail_at = prefix;
    hit = false;
    tracking = true;
    try {
      heat_bind_surface(m, rig, options, {}, &report, sources);
    } catch (const std::bad_alloc &) {
      failed = true;
    } catch (...) {
      tracking = false;
      throw;
    }
    tracking = false;
    if (!hit || !failed)
      throw std::runtime_error(
          "Measured persistent allocation failure did not actually hit");
    unchanged(report);
    auto retry = heat_bind_surface(m, rig, options, {}, nullptr, sources);
    if (retry.encode() != expected.encode())
      throw std::runtime_error(
          "Allocation-failure retry differs from exact successful field");
  }
  for (size_t at = 1; at <= checkpoints; ++at) {
    size_t seen = 0;
    auto report = sentinel;
    bool failed = false;
    try {
      heat_bind_surface(
          m, rig, options, [&] { return ++seen >= at; }, &report, sources);
    } catch (const std::runtime_error &) {
      failed = true;
    }
    if (!failed || seen < at)
      throw std::runtime_error("Measured cancellation did not actually hit");
    unchanged(report);
    if (heat_bind_surface(m, rig, options, {}, nullptr, sources).encode() !=
        expected.encode())
      throw std::runtime_error("Cancellation retry changed field");
  }
  for (size_t limit : {size_t(1), measured.work / 2, measured.work - 1}) {
    auto o = options;
    o.maximum_work = limit;
    auto report = sentinel;
    bool failed = false;
    try {
      heat_bind_surface(m, rig, o, {}, &report, sources);
    } catch (const std::length_error &) {
      failed = true;
    }
    if (!failed)
      throw std::runtime_error("Actual work cap did not refuse");
    unchanged(report);
  }
  auto limited = options;
  limited.maximum_bytes = measured.planned_bytes - 1;
  auto report = sentinel;
  bool failed = false;
  try {
    heat_bind_surface(m, rig, limited, {}, &report, sources);
  } catch (const std::length_error &) {
    failed = true;
  }
  if (!failed)
    throw std::runtime_error("Planned-byte cap did not refuse");
  unchanged(report);
  limited.maximum_bytes = measured.planned_bytes;
  limited.maximum_work = measured.work;
  if (heat_bind_surface(m, rig, limited, {}, nullptr, sources).encode() !=
      expected.encode())
    throw std::runtime_error("Exact measured resource limits changed result");
  std::cout << "PASS actual bounded bind persistent_allocations=" << allocations
            << " cancellation_checkpoints=" << checkpoints
            << " work=" << measured.work
            << " planned_bytes=" << measured.planned_bytes
            << " kkt=" << measured.maximum_kkt_residual
            << " bound_sweeps=" << measured.bound_sweeps
            << " source/report/owner exact and successful retries\n";
} catch (const std::exception &e) {
  tracking = false;
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
