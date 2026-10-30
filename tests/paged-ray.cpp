// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/volume.hpp"
#include "ray-index.hpp"
#include <atomic>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <nlohmann/json.hpp>
#include <random>
#include <thread>
namespace {
thread_local int64_t fail = -1;
thread_local size_t allocation_count = 0;
} // namespace
void *operator new(size_t n) {
  ++allocation_count;
  if (fail == 0)
    throw std::bad_alloc();
  if (fail > 0)
    --fail;
  if (auto p = std::malloc(n ? n : 1))
    return p;
  throw std::bad_alloc();
}
void *operator new[](size_t n) { return ::operator new(n); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
void operator delete[](void *p, size_t) noexcept { std::free(p); }
namespace dome::test {
std::optional<Vec3> reference_raycast(const Volume &, Vec3, Vec3, float);
}
using namespace dome;
using namespace dome::sculpt_detail;
namespace {
unsigned checks = 0;
void check(bool b, const char *s) {
  if (!b)
    throw std::runtime_error(s);
  ++checks;
  std::cout << "PASS " << s << std::endl;
}
void stroke(Volume &v, Vec3 p, float r, Tool tool = Tool::Clay) {
  v.begin_stroke();
  Brush b;
  b.from = b.to = p;
  b.radius = r;
  b.blend = 0;
  b.tool = tool;
  v.stamp(b);
  v.end_stroke();
}
Volume ball(float voxel, Vec3 p = {}) {
  Volume v(voxel);
  stroke(v, p, voxel * 2.1f);
  return v;
}
void equal(const Volume &v, Vec3 origin, Vec3 dir, float maximum) {
  auto actual = v.raycast(origin, dir, maximum);
  auto expected = test::reference_raycast(v, origin, dir, maximum);
  if (bool(actual) != bool(expected))
    throw std::runtime_error(
        "Hit presence differs from independent whole-cell oracle");
  if (actual)
    for (int k = 0; k < 3; ++k) {
      float a = (&actual->x)[k], b = (&expected->x)[k];
      float ulp = std::max(std::abs(std::nextafter(a, INFINITY) - a),
                           std::abs(std::nextafter(b, INFINITY) - b));
      if (std::abs(a - b) > std::max(ulp * 8, v.voxel_size() * 1e-5f))
        throw std::runtime_error(
            "First hit differs from independent cubic/cell oracle");
    }
}
Volume corridor() {
  Volume::Voxels values;
  for (int z = -12000; z <= 12000; ++z)
    values[{1, 1, z}] = -.00002f;
  return Volume::from_samples(.00001f, std::move(values));
}
} // namespace
int main() try {
  clear_ray_index_cache();
  for (float spacing : {.00001f, .0001f, .003f, .1f})
    for (Int3 cell : {Int3{-17, -16, -1}, Int3{0, 0, 0}, Int3{15, 16, 31}}) {
      Vec3 center{cell.x * spacing, cell.y * spacing, cell.z * spacing};
      auto v = ball(spacing, center);
      equal(v, center + Vec3{0, 0, 2}, {0, 0, -1}, 3);
      equal(v, center + Vec3{-.75f, 0, 0}, {1, 0, 0}, 2);
      equal(v, center, {1, 1, 1}, 1);
    }
  check(true, "Fine10um/0.1mm/coarse fields and negative/page-boundary "
              "translations match whole-cell first-hit oracle");
  for (int axis = 0; axis < 3; ++axis)
    for (int sign : {-1, 1}) {
      auto v = ball(.00001f);
      Vec3 ray{}, origin{};
      (&ray.x)[axis] = float(sign);
      (&origin.x)[axis] = -float(sign) * 5;
      equal(v, origin, ray, 6);
    }
  check(true, "Both directions and all zero-direction axes skip five metres of "
              "empty fine-grid space correctly");
  Volume::Voxels positive;
  positive[{0, 0, 0}] = .0000006f;
  auto tolerant = Volume::from_samples(.00001f, std::move(positive));
  check(bool(tolerant.raycast({0, 0, .1f}, {0, 0, -1}, .2f)),
        "Strictly positive samples below established tolerance remain pickable "
        "candidates");
  equal(tolerant, {0, 0, .1f}, {0, 0, -1}, .2f);
  auto context = tolerant.sculpt_settings();
  context.mask_visible = false;
  tolerant.sculpt_settings(context);
  auto wide = tolerant.encode();
  wide["version"] = 4;
  wide["distance_band"] = .001f;
  wide["sculpt_layers"][0]["delta"][0][3] = double(.0000006f) - double(.001f);
  auto physical = Volume::decode(wide);
  equal(physical, {0, 0, .1f}, {0, 0, -1}, .2f);
  check(true, "Changed physical-band contexts retain the precise positive "
              "tolerance semantics");
  // These binary-valued samples have two independently known roots, or one
  // exact tangent, inside a single cell. The camera is five metres away: an
  // erroneous later root cannot hide behind a camera-space ULP tolerance.
  const float analytic_voxel = std::ldexp(25.f, -21);
  for (bool tangent : {false, true})
    for (int shift : {0, 15, -16}) {
      Volume::Voxels samples;
      const double a = tangent ? .5 : .25, b = tangent ? .5 : .265625;
      const double threshold = double(analytic_voxel) * .12;
      for (int z = 0; z < 2; ++z)
        for (int y = 0; y < 2; ++y)
          for (int x = 0; x < 2; ++x)
            samples[{shift + x, shift + y, shift + z}] = float(
                threshold + double(analytic_voxel) * 2 * (x - a) * (y - b));
      auto field = Volume::from_samples(analytic_voxel, std::move(samples));
      const float offset = shift * analytic_voxel;
      Vec3 origin{offset - 5, offset - 5, (shift + .5f) * analytic_voxel};
      auto found = field.raycast(origin, {1, 1, 0}, 8);
      const float earliest = (shift + float(a)) * analytic_voxel;
      if (!found || std::abs(found->x - earliest) > analytic_voxel * .001f ||
          std::abs(found->y - earliest) > analytic_voxel * .001f)
        throw std::runtime_error(
            "Analytic earliest double crossing/tangency was skipped");
      equal(field, origin, {1, 1, 0}, 8);
    }
  check(true, "Analytic close double crossings and exact tangencies retain the "
              "earliest root after five metres of empty space");
  Volume::Voxels slab_samples;
  const float slab_value = analytic_voxel * .06f;
  for (int y = -2; y <= 2; ++y)
    for (int x = 14; x <= 17; ++x)
      slab_samples[{x, y, 0}] = slab_value;
  auto slab = Volume::from_samples(analytic_voxel, std::move(slab_samples));
  const double slab_z = (double(analytic_voxel) * .12 - double(slab_value)) /
                        (double(slab.band()) - slab_value) * analytic_voxel;
  for (int sign : {-1, 1}) {
    auto found = slab.raycast(
        {15.25f * analytic_voxel, -.4f * analytic_voxel, sign * 2.f},
        {0, 0, float(-sign)}, 3);
    if (!found || std::abs(found->z - sign * slab_z) > analytic_voxel * 1e-5f)
      throw std::runtime_error(
          "Positive tolerant slab or page incident-cell halo was skipped");
  }
  check(true, "Positive-only slabs retain both incident-cell halos across page "
              "boundaries with an analytic first hit");
  std::mt19937 random(0x7a112u);
  for (int fixture = 0; fixture < 24; ++fixture) {
    Volume::Voxels samples;
    const float spacing = fixture % 2 ? .00001f : .003f;
    const int shift = (fixture % 3 - 1) * 16;
    for (int z = -1; z <= 2; ++z)
      for (int y = -1; y <= 2; ++y)
        for (int x = -1; x <= 2; ++x)
          samples[{x + shift, y + shift, z + shift}] =
              float(int(random() % 2001) - 1000) * spacing * .003f;
    auto field = Volume::from_samples(spacing, std::move(samples));
    for (int r = 0; r < 16; ++r) {
      Vec3 direction{float(int(random() % 2001) - 1000),
                     float(int(random() % 2001) - 1000),
                     float(int(random() % 2001) - 1000)};
      direction = normalized(direction);
      Vec3 center{(shift + .5f) * spacing, (shift + .5f) * spacing,
                  (shift + .5f) * spacing};
      equal(field, center - direction * (spacing * 20), direction,
            spacing * 50);
    }
  }
  check(true, "Fixed-seed trilinear fields and oblique rays match the "
              "independent whole-cell first-root oracle");
  Volume::Voxels close_features;
  constexpr int large_cell = 6000000;
  close_features[{large_cell, 0, 0}] = -analytic_voxel;
  close_features[{large_cell + 8, 0, 0}] = -analytic_voxel;
  auto translated_features =
      Volume::from_samples(analytic_voxel, std::move(close_features));
  const float first_coordinate = large_cell * analytic_voxel;
  auto earliest_feature =
      translated_features.raycast({first_coordinate - 5, 0, 0}, {1, 0, 0}, 6);
  check(earliest_feature &&
            earliest_feature->x < first_coordinate + analytic_voxel * 2,
        "Large translated world-coordinate rounding cannot admit the later of "
        "two close candidate features");

  auto v = ball(.01f);
  clear_ray_index_cache();
  auto first = v.raycast({0, 0, 1}, {0, 0, -1}, 2);
  auto before = ray_index_stats();
  allocation_count = 0;
  fail = 0;
  for (int i = 0; i < 100; ++i)
    v.raycast({0, 0, 1}, {0, 0, -1}, 2);
  fail = -1;
  auto warmed = ray_index_stats();
  check(allocation_count == 0 && warmed.pages_scanned == before.pages_scanned &&
            warmed.cold_builds == before.cold_builds,
        "One hundred warmed immutable-owner queries allocate nothing and never "
        "scan source pages");
  std::atomic_bool thread_failed = false;
  std::array<std::thread, 4> threads;
  before = ray_index_stats();
  for (auto &thread : threads)
    thread = std::thread([&] {
      try {
        for (int i = 0; i < 100; ++i) {
          auto hit = v.raycast({0, 0, 1}, {0, 0, -1}, 2);
          if (!hit || hit->z != first->z)
            thread_failed = true;
        }
      } catch (...) {
        thread_failed = true;
      }
    });
  for (auto &thread : threads)
    thread.join();
  check(!thread_failed &&
            ray_index_stats().pages_scanned == before.pages_scanned,
        "Concurrent immutable-owner queries reuse the shared index with "
        "isolated per-thread interval storage");
  bool recursion_rejected = false;
  auto outer = v.raycast({0, 0, 1}, {0, 0, -1}, 2, [&] {
    try {
      v.raycast({0, 0, 1}, {0, 0, -1}, 2);
    } catch (const std::logic_error &) {
      recursion_rejected = true;
    }
    return false;
  });
  check(recursion_rejected && outer && outer->z == first->z,
        "Recursive picking from cancellation callbacks refuses safely before "
        "invalidating outer interval storage");
  auto owner = v.geometry_owner();
  auto old_at = v.at({0, 0, 0});
  stroke(v, {0, 0, .019f}, .015f, Tool::Carve);
  auto changed = ray_index_stats();
  equal(v, {0, 0, 1}, {0, 0, -1}, 2);
  auto updated = ray_index_stats();
  check(updated.local_updates == changed.local_updates + 1 &&
            updated.pages_scanned - changed.pages_scanned <= 128 &&
            owner->at({0, 0, 0}) == old_at,
        "Authenticated local geometry changes update only dirty pages while "
        "retained source stays immutable");
  v.fill_mask(.75f);
  auto settings = v.sculpt_settings();
  settings.symmetry_axes = 5;
  v.sculpt_settings(settings);
  auto scalar = ray_index_stats();
  v.raycast({0, 0, 1}, {0, 0, -1}, 2);
  auto scalar_after = ray_index_stats();
  check(scalar_after.pages_scanned == scalar.pages_scanned &&
            scalar_after.local_updates == scalar.local_updates,
        "Mask/control-only changes reuse exact geometry acceleration without "
        "scanning pages");
  v.undo();
  v.undo();
  v.undo();
  equal(v, {0, 0, 1}, {0, 0, -1}, 2);
  v.redo();
  equal(v, {0, 0, 1}, {0, 0, -1}, 2);
  check(true, "Undo/redo first hits remain identical to the independent "
              "original whole-cell solver");
  for (int i = 0; i < 4; ++i)
    stroke(v, {.1f + i * .03f, 0, 0}, .021f);
  before = ray_index_stats();
  equal(v, {0, 0, 1}, {0, 0, -1}, 2);
  updated = ray_index_stats();
  check(updated.local_updates == before.local_updates + 1,
        "Skipped authenticated revisions coalesce into one local index update");
  for (int i = 0; i < 70; ++i)
    stroke(v, {-.1f - i * .03f, .1f, 0}, .021f);
  before = ray_index_stats();
  equal(v, {0, 0, 1}, {0, 0, -1}, 2);
  updated = ray_index_stats();
  check(updated.cold_builds == before.cold_builds + 1,
        "Expired bounded ancestry rebuilds safely rather than trusting stale "
        "dirty regions");
  auto clearing = ball(.0001f);
  auto retained = clearing.geometry_snapshot();
  clearing.raycast({0, 0, 1}, {0, 0, -1}, 2);
  stroke(clearing, {}, .002f, Tool::Carve);
  check(clearing.voxels().empty() &&
            !clearing.raycast({0, 0, 1}, {0, 0, -1}, 2) &&
            retained.raycast({0, 0, 1}, {0, 0, -1}, 2).has_value(),
        "Complete field clearing never reuses a stale hit and retained "
        "original owners remain valid");
  stroke(clearing, {.01f, 0, 0}, .00021f);
  equal(clearing, {0, 0, 1}, {0, 0, -1}, 2);
  equal(clearing, {.01f, 0, 1}, {0, 0, -1}, 2);
  check(!clearing.raycast({0, 0, 1}, {0, 0, -1}, 2) &&
            clearing.raycast({.01f, 0, 1}, {0, 0, -1}, 2).has_value(),
        "Authenticated removals followed by new disjoint pages remove old "
        "candidates and retain new ones");
  auto layered = ball(.0001f);
  auto base = layered.layers()[0];
  layered.raycast({0, 0, 1}, {0, 0, -1}, 2);
  layered.layer_properties(base.id, base.name, false, false, 1);
  check(!layered.raycast({0, 0, 1}, {0, 0, -1}, 2),
        "Layer recomposition to empty never returns dormant cached geometry");
  layered.layer_properties(base.id, base.name, true, false, 1);
  equal(layered, {0, 0, 1}, {0, 0, -1}, 2);
  auto dense = corridor();
  bool exhausted = false;
  try {
    dense.raycast({0, 0, 1}, {0, 0, -1}, 2);
  } catch (const std::length_error &) {
    exhausted = true;
  }
  check(exhausted, "A genuinely dense nonempty candidate corridor still "
                   "exhausts the unchanged20000-cell budget");
  bool cancelled = false;
  auto baseline_cells = ray_index_stats().cells;
  try {
    dense.raycast({0, 0, 1}, {0, 0, -1}, 2, [&] {
      return ray_index_stats().cells > baseline_cells + 512;
    });
  } catch (const std::runtime_error &e) {
    cancelled = std::string(e.what()) == "Sculpt ray cancelled";
  }
  check(cancelled && dense.geometry_owner() != nullptr,
        "Cancellation interrupts actual candidate traversal without changing "
        "source ownership");
  clear_ray_index_cache();
  cancelled = false;
  int callbacks = 0;
  auto cold_owner = v.geometry_owner();
  try {
    v.raycast({0, 0, 1}, {0, 0, -1}, 2, [&] { return ++callbacks > 5; });
  } catch (const std::runtime_error &e) {
    cancelled = std::string(e.what()) == "Sculpt ray cancelled";
  }
  check(cancelled && v.geometry_owner() == cold_owner &&
            ray_index_stats().cached_entries == 0,
        "Cancellation during cold immutable-index construction publishes no "
        "partial cache or geometry");
  auto tiny = ball(.00001f);
  clear_ray_index_cache();
  allocation_count = 0;
  tiny.raycast({0, 0, 1}, {0, 0, -1}, 2);
  size_t boundaries = allocation_count;
  for (size_t i = 0; i < boundaries; ++i) {
    clear_ray_index_cache();
    auto unchanged = tiny.geometry_owner();
    bool rejected = false;
    fail = int64_t(i);
    try {
      tiny.raycast({0, 0, 1}, {0, 0, -1}, 2);
    } catch (const std::bad_alloc &) {
      rejected = true;
    }
    fail = -1;
    if (!rejected || tiny.geometry_owner() != unchanged ||
        ray_index_stats().cached_entries != 0)
      throw std::runtime_error(
          "Cold allocation failure partially published index or source");
  }
  check(boundaries > 0,
        "Every cold-build allocation boundary refuses atomically without "
        "altering geometry or publishing a partial index");
  std::cout << "MEASURE cold allocation boundaries " << boundaries << std::endl;
  auto local_base = ball(.001f), local_edit = local_base;
  stroke(local_edit, {0, 0, .002f}, .0015f, Tool::Carve);
  clear_ray_index_cache();
  local_base.raycast({0, 0, 1}, {0, 0, -1}, 2);
  allocation_count = 0;
  local_edit.raycast({0, 0, 1}, {0, 0, -1}, 2);
  const size_t local_boundaries = allocation_count;
  for (size_t i = 0; i < local_boundaries; ++i) {
    clear_ray_index_cache();
    local_base.raycast({0, 0, 1}, {0, 0, -1}, 2);
    const auto unchanged = local_edit.geometry_owner();
    bool rejected = false;
    fail = int64_t(i);
    try {
      local_edit.raycast({0, 0, 1}, {0, 0, -1}, 2);
    } catch (const std::bad_alloc &) {
      rejected = true;
    }
    fail = -1;
    if (!rejected || local_edit.geometry_owner() != unchanged ||
        ray_index_stats().cached_entries != 1)
      throw std::runtime_error("Local-update allocation fault published "
                               "partial acceleration or modified source");
    equal(local_base, {0, 0, 1}, {0, 0, -1}, 2);
  }
  check(local_boundaries > 0,
        "Every authenticated local-index allocation boundary preserves the "
        "prior immutable index and changed source atomically");
  std::cout << "MEASURE local allocation boundaries " << local_boundaries
            << std::endl;
  clear_ray_index_cache();
  tiny.raycast({0, 0, 1}, {0, 0, -1}, 2);
  bool workspace_fault = false, workspace_recovered = false;
  std::thread workspace_thread([&] {
    fail = 0;
    try {
      tiny.raycast({0, 0, 1}, {0, 0, -1}, 2);
    } catch (const std::bad_alloc &) {
      workspace_fault = true;
    }
    fail = -1;
    workspace_recovered = tiny.raycast({0, 0, 1}, {0, 0, -1}, 2).has_value();
  });
  workspace_thread.join();
  check(workspace_fault && workspace_recovered,
        "A real interval-workspace bad_alloc propagates and the same immutable "
        "query recovers without quota fallback");

  clear_ray_index_cache();
  auto clock = std::chrono::steady_clock::now();
  auto oracle = test::reference_raycast(tiny, {0, 0, 5}, {0, 0, -1}, 6);
  double old_ms = std::chrono::duration<double, std::milli>(
                      std::chrono::steady_clock::now() - clock)
                      .count();
  clock = std::chrono::steady_clock::now();
  auto accelerated = tiny.raycast({0, 0, 5}, {0, 0, -1}, 6);
  double new_ms = std::chrono::duration<double, std::milli>(
                      std::chrono::steady_clock::now() - clock)
                      .count();
  check(
      oracle && accelerated && length(*oracle - *accelerated) < 1e-7f,
      "Five-metre10um first hit agrees with independent500000-cell traversal");
  std::cout << "MEASURE fine-reference-ms " << old_ms << " indexed-cold-ms "
            << new_ms << std::endl;
  Volume::Voxels disjoint;
  disjoint[{0, 0, 0}] = -.00002f;
  disjoint[{5000000, 0, 0}] = -.00002f;
  auto separated = Volume::from_samples(.00001f, std::move(disjoint));
  auto hit = separated.raycast({-1, 0, 0}, {1, 0, 0}, 60);
  check(hit && hit->x < .0001f, "Far disconnected supports preserve globally "
                                "first hit across enormous positive gaps");
  clear_ray_index_cache();
  auto gesture = ball(.0001f);
  gesture.begin_stroke();
  std::vector<double> stamp_times;
  size_t maximum_scanned = 0;
  auto gesture_clock = std::chrono::steady_clock::now();
  for (int i = 0; i < 96; ++i) {
    Vec3 center{i * .00015f, 0, 0};
    auto ray_origin = center + Vec3{0, 0, .5f};
    auto begin_time = std::chrono::steady_clock::now();
    gesture.raycast(ray_origin, {0, 0, -1}, 1);
    auto previous_stats = ray_index_stats();
    Brush brush;
    brush.from = brush.to = center;
    brush.radius = .00021f;
    brush.blend = 0;
    gesture.stamp(brush);
    equal(gesture, ray_origin, {0, 0, -1}, 1);
    auto current_stats = ray_index_stats();
    maximum_scanned =
        std::max(maximum_scanned,
                 current_stats.pages_scanned - previous_stats.pages_scanned);
    stamp_times.push_back(std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - begin_time)
                              .count());
  }
  gesture.end_stroke();
  std::sort(stamp_times.begin(), stamp_times.end());
  check(maximum_scanned <= 8 && ray_index_stats().local_updates >= 96,
        "Ninety-six real gesture stamps and picks update only changed pages "
        "while matching the full-cell oracle");
  std::cout << "MEASURE gesture96-total-ms "
            << std::chrono::duration<double, std::milli>(
                   std::chrono::steady_clock::now() - gesture_clock)
                   .count()
            << " median-ray-stamp-oracle-ms " << stamp_times[48]
            << " maximum-pages-per-stamp " << maximum_scanned << std::endl;
  for (int trial = 0; trial < 3; ++trial) {
    auto legacy = ball(.0001f), indexed = legacy;
    auto run = [&](Volume &field, bool accelerated) {
      clear_ray_index_cache();
      const auto start = std::chrono::steady_clock::now();
      field.begin_stroke();
      for (int i = 0; i < 96; ++i) {
        Vec3 center{i * .00015f, 0, 0};
        Vec3 origin = center + Vec3{0, 0, .5f};
        if (accelerated)
          field.raycast(origin, {0, 0, -1}, 1);
        else
          test::reference_raycast(field, origin, {0, 0, -1}, 1);
        Brush b;
        b.from = b.to = center;
        b.radius = .00021f;
        b.blend = 0;
        field.stamp(b);
      }
      field.end_stroke();
      return std::chrono::duration<double, std::milli>(
                 std::chrono::steady_clock::now() - start)
          .count();
    };
    auto legacy_ms = run(legacy, false), indexed_ms = run(indexed, true);
    if (legacy.voxels() != indexed.voxels() ||
        legacy.layers()[0].delta.read() != indexed.layers()[0].delta.read())
      throw std::runtime_error(
          "Gesture benchmark changed authoritative field/layer state");
    std::cout << "MEASURE gesture96-trial " << trial << " reference-ms "
              << legacy_ms << " indexed-ms " << indexed_ms << std::endl;
  }
  check(true, "Three complete pick-and-stamp gesture trials preserve exact "
              "original field and layer state");
  clear_ray_index_cache();
  Volume::Voxels background;
  for (int i = 0; i < 8192; ++i)
    background[{i * 32, 0, 0}] = -.00002f;
  auto many_pages = Volume::from_samples(.00001f, std::move(background));
  many_pages.raycast({-.000005f, 0, 0}, {1, 0, 0}, .001f);
  before = ray_index_stats();
  allocation_count = 0;
  fail = 0;
  for (int i = 0; i < 50; ++i)
    many_pages.raycast({-.000005f, 0, 0}, {1, 0, 0}, .001f);
  fail = -1;
  check(allocation_count == 0 &&
            ray_index_stats().pages_scanned == before.pages_scanned,
        "Warm queries against8192 occupied pages neither flatten nor rescan "
        "geometry or allocate");
  clear_ray_index_cache();
  Volume::Voxels interval_samples;
  for (int i = 0; i < 40000; ++i)
    interval_samples[{i * 32, 0, 0}] = -.00002f;
  auto many_intervals =
      Volume::from_samples(.00001f, std::move(interval_samples));
  bool interval_refused = false;
  try {
    many_intervals.raycast({-1, 0, 0}, {1, 0, 0}, 20);
  } catch (const std::length_error &) {
    interval_refused = true;
  }
  before = ray_index_stats();
  auto short_hit = many_intervals.raycast({-.000005f, 0, 0}, {1, 0, 0}, .001f);
  updated = ray_index_stats();
  check(interval_refused && short_hit &&
            updated.fallback_queries == before.fallback_queries &&
            updated.pages_scanned == before.pages_scanned,
        "A ray-specific interval quota falls back without poisoning the "
        "reusable index for subsequent short rays");
  clear_ray_index_cache();
  Volume::Voxels huge;
  for (int i = 0; i < 220000; ++i)
    huge[{i * 32, 0, 0}] = -.00002f;
  auto bounded = Volume::from_samples(.00001f, std::move(huge));
  auto bounded_owner = bounded.geometry_owner();
  bool refused = false;
  try {
    bounded.raycast({-1, 0, 0}, {1, 0, 0}, 80);
  } catch (const std::length_error &) {
    refused = true;
  }
  check(refused && bounded.geometry_owner() == bounded_owner,
        "Oversized acceleration retains the original bounded traversal refusal "
        "without corrupting the source");
  before = ray_index_stats();
  auto nearby = bounded.raycast({-.000005f, 0, 0}, {1, 0, 0}, .001f);
  updated = ray_index_stats();
  check(nearby && updated.pages_scanned == before.pages_scanned &&
            updated.fallback_queries > before.fallback_queries,
        "Acceleration quotas preserve large-field near hits using bounded "
        "reference traversal with no repeated source scan");
  stroke(bounded, {0, 0, 0}, .000021f);
  before = ray_index_stats();
  nearby = bounded.raycast({-.000005f, 0, 0}, {1, 0, 0}, .001f);
  updated = ray_index_stats();
  check(nearby && updated.pages_scanned == before.pages_scanned,
        "Small edits to a demonstrably over-quota source use O64 quota proofs "
        "without rebuilding or scanning the whole field");
  clear_ray_index_cache();
  std::cout << checks << " paged ray checks passed" << std::endl;
} catch (const std::exception &e) {
  fail = -1;
  std::cerr << "FAIL " << e.what() << std::endl;
  return 1;
}
