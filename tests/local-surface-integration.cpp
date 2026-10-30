// SPDX-License-Identifier: GPL-3.0-or-later
#include "brush_kernel.hpp"
#include "dome/document.hpp"
#include "dome/local-surface.hpp"
#include <atomic>
#include <bit>
#include <cstdlib>
#include <iostream>
#include <nlohmann/json.hpp>
#include <thread>
namespace {
thread_local int64_t fail_allocation = -1, fail_after_backend = -1;
thread_local bool arm_backend = false, count_allocations = false;
thread_local size_t allocations = 0;
std::atomic_bool fail_materialization{false};
void reset_allocations() {
  fail_allocation = -1;
  fail_after_backend = -1;
  arm_backend = false;
  count_allocations = false;
}
} // namespace
void *operator new(size_t n) {
  if (count_allocations)
    ++allocations;
  if (fail_allocation == 0)
    throw std::bad_alloc();
  if (fail_allocation > 0)
    --fail_allocation;
  if (auto p = std::malloc(n ? n : 1))
    return p;
  throw std::bad_alloc();
}
void *operator new[](size_t n) { return ::operator new(n); }
void operator delete(void *p) noexcept { std::free(p); }
void operator delete[](void *p) noexcept { std::free(p); }
void operator delete(void *p, size_t) noexcept { std::free(p); }
void operator delete[](void *p, size_t) noexcept { std::free(p); }
namespace dome {
// Exact selected CPU kernel, with caller-thread allocation faults armed only
// AFTER backend completion so preparation/publication boundaries are exercised.
void apply_brush(std::span<Sample> samples, const Brush &b) {
  for (auto &s : samples)
    s.distance = brush_sample(s, b);
  if (arm_backend) {
    arm_backend = false;
    count_allocations = true;
    allocations = 0;
    fail_allocation = fail_after_backend;
  }
}
std::string_view compute_backend() {
  return "CPU (shared kernel; allocation instrumentation)";
}
} // namespace dome
extern "C" dome::Mesh real_materialize(
    const dome::LocalSurfaceCache *,
    dome::LocalSurfaceCache::Cancel) asm("__real__"
                                         "ZNK4dome17LocalSurfaceCache11material"
                                         "izeESt8functionIFbvEE");
extern "C" dome::Mesh wrapped_materialize(
    const dome::LocalSurfaceCache *,
    dome::LocalSurfaceCache::Cancel) asm("__wrap__"
                                         "ZNK4dome17LocalSurfaceCache11material"
                                         "izeESt8functionIFbvEE");
extern "C" dome::Mesh
wrapped_materialize(const dome::LocalSurfaceCache *cache,
                    dome::LocalSurfaceCache::Cancel cancel) {
  auto result = real_materialize(cache, std::move(cancel));
  if (fail_materialization.exchange(false))
    throw std::bad_alloc();
  return result;
}
using namespace dome;
namespace {
int checks = 0;
void check(bool yes, const char *label) {
  if (!yes)
    throw std::runtime_error(label);
  ++checks;
  std::cout << "PASS " << label << '\n';
}
bool same(Vec3 a, Vec3 b) {
  return std::bit_cast<std::array<uint32_t, 3>>(a) ==
         std::bit_cast<std::array<uint32_t, 3>>(b);
}
bool same(const Mesh &a, const Mesh &b) {
  return a.indices == b.indices && a.positions.size() == b.positions.size() &&
         a.normals.size() == b.normals.size() &&
         std::equal(a.positions.begin(), a.positions.end(), b.positions.begin(),
                    [](auto a, auto b) { return same(a, b); }) &&
         std::equal(a.normals.begin(), a.normals.end(), b.normals.begin(),
                    [](auto a, auto b) { return same(a, b); });
}
Brush brush(Vec3 p, Tool tool = Tool::Carve, float radius = .075f) {
  Brush b;
  b.from = b.to = p;
  b.radius = radius;
  b.tool = tool;
  b.blend = 0;
  return b;
}
void stroke(Volume &v, Brush b) {
  v.begin_stroke();
  v.stamp(b);
  v.end_stroke();
}
Volume sphere(float voxel = .04f, float radius = .36f) {
  Volume v(voxel);
  stroke(v, brush({}, Tool::Clay, radius));
  return v;
}
bool covered(Int3 p, const Volume::GeometryChange &change) {
  return std::any_of(
      change.regions().begin(), change.regions().end(), [&](auto r) {
        return p.x >= r.minimum.x && p.x <= r.maximum.x && p.y >= r.minimum.y &&
               p.y <= r.maximum.y && p.z >= r.minimum.z && p.z <= r.maximum.z;
      });
}
void complete(const Volume &a, const Volume &b,
              const Volume::GeometryChange &change) {
  check(change.kind == Volume::GeometryChange::Kind::Local,
        "Real geometry transition exposes authenticated local regions");
  for (auto [p, d] : a.voxels())
    if (b.at(p) != d && !covered(p, change))
      throw std::runtime_error("Removed/changed sample omitted from producer");
  for (auto [p, d] : b.voxels())
    if (a.at(p) != d && !covered(p, change))
      throw std::runtime_error("Created/changed sample omitted from producer");
  check(true, "Independent old/new whole-map oracle proves every changed "
              "sample is covered");
}
Mesh whole_scene(const Scene &scene) {
  Mesh result;
  for (auto &o : scene.preview_surfaces()) {
    auto m = o.preview_mesh();
    auto t = o.world_transform();
    auto offset = uint32_t(result.positions.size());
    for (auto p : m.positions) {
      auto q = ob::transform(t, {p.x, p.y, p.z});
      result.positions.push_back({q.x, q.y, q.z});
    }
    for (auto n : m.normals) {
      auto q = ob::rotate(t.orientation, {n.x, n.y, n.z});
      result.normals.push_back({q.x, q.y, q.z});
    }
    for (auto i : m.indices)
      result.indices.push_back(offset + i);
  }
  return result;
}
void ready(Document &d) {
  if (!d.wait_mesh(std::chrono::seconds(30)))
    throw std::runtime_error("Document mesh timeout: " +
                             d.frame().state->error);
  if (!d.frame().state->error.empty())
    throw std::runtime_error(d.frame().state->error);
}
void doc_stroke(Document &d, Brush b) {
  auto scene = d.copy_scene();
  auto t = scene.world_transform(scene.active().id);
  auto world = [&](Vec3 p) {
    auto q = ob::transform(t, {p.x, p.y, p.z});
    return Vec3{q.x, q.y, q.z};
  };
  b.from = world(b.from);
  b.to = world(b.to);
  b.radius *= t.scale;
  b.blend *= t.scale;
  auto n = ob::rotate(t.orientation, {b.normal.x, b.normal.y, b.normal.z});
  b.normal = {n.x, n.y, n.z};
  if (!d.begin_desktop())
    throw std::runtime_error("Document rejected fixture stroke");
  d.stamp_desktop(b);
  d.end_desktop();
}
void late_faults(auto factory, auto operation, const char *label) {
  size_t total = 0;
  {
    auto probe = factory();
    auto held = probe.geometry_snapshot();
    (void)held;
    allocations = 0;
    count_allocations = true;
    operation(probe);
    total = allocations;
    reset_allocations();
  }
  check(total > 0, "Transactional operation has allocation boundaries to test");
  for (size_t last = 1; last <= std::min(size_t(6), total); ++last) {
    auto candidate = factory();
    auto held = candidate.geometry_snapshot();
    auto owner = candidate.geometry_owner();
    auto revision = candidate.revision(), history = candidate.undo_count();
    bool editing = candidate.in_stroke();
    auto before = editing ? nlohmann::json{} : candidate.encode();
    fail_allocation = int64_t(total - last);
    bool refused = false;
    try {
      operation(candidate);
    } catch (const std::bad_alloc &) {
      refused = true;
    }
    reset_allocations();
    bool layers_unchanged = candidate.layers().size() == held.layers().size();
    for (size_t i = 0; layers_unchanged && i < candidate.layers().size(); ++i)
      layers_unchanged =
          candidate.layers()[i].delta.owner() == held.layers()[i].delta.owner();
    check(layers_unchanged && refused && candidate.geometry_owner() == owner &&
              candidate.revision() == revision &&
              candidate.undo_count() == history &&
              candidate.in_stroke() == editing &&
              (editing || candidate.encode() == before),
          label);
    if (editing) {
      fail_allocation = 0;
      candidate.cancel_stroke();
      reset_allocations();
    }
  }
}
double timed(auto action) {
  auto start = std::chrono::steady_clock::now();
  action();
  return std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now() - start)
      .count();
}
} // namespace
int main(int argc, char **argv) try {
  if (argc > 1 && std::string_view(argv[1]) == "--benchmark") {
    Document d;
    d.replace(sphere(.025f, .6f));
    ready(d);
    double wall = 0, worker = 0, extract = 0, assembly = 0, reference = 0;
    size_t rebuilt = 0, reused = 0, full = 0, local = 0;
    for (int i = 0; i < 24; ++i) {
      wall += timed([&] {
        auto b = brush({.48f, float(i - 12) * .012f, .25f},
                       i % 2 ? Tool::Carve : Tool::Clay, .065f);
        if (!d.begin_desktop())
          throw std::runtime_error("Benchmark stroke rejected");
        d.stamp_desktop(b);
        d.end_desktop();
        ready(d);
      });
      auto f = d.frame();
      auto s = f.surface->sculpt_meshing;
      worker += s.worker_ms;
      extract += s.extraction_ms;
      assembly += s.materialize_ms;
      rebuilt += s.rebuilt_tiles;
      reused += s.reused_tiles;
      full += s.full_rebuilds;
      local += s.local_updates;
      Mesh oracle;
      auto scene = d.copy_scene();
      reference += timed([&] { oracle = whole_scene(scene); });
      if (!same(oracle, f.surface->mesh))
        throw std::runtime_error(
            "End-to-end benchmark differs from whole-scene oracle");
    }
    std::cout << "BENCH Document24 wall_ms=" << wall << " worker_ms=" << worker
              << " extraction_ms=" << extract << " assembly_ms=" << assembly
              << " corrected_whole_ms=" << reference
              << " rebuilt_tiles=" << rebuilt << " reused_tiles=" << reused
              << " full=" << full << " local=" << local << '\n';
    return 0;
  }
  {
    auto volume = sphere();
    auto retired = std::weak_ptr(volume.layers()[0].delta.owner());
    auto cache = LocalSurfaceCache::build(volume);
    auto expected = surface_mesh(volume);
    volume = Volume(.04f);
    check(retired.expired() && same(cache->materialize(), expected),
          "Minimal cache SDF owner releases retired displacement layers while "
          "preserving samples");
    check(cache->stats().source_bytes > 0 &&
              cache->stats().bytes >= cache->stats().source_bytes,
          "Retained SDF sample-map storage is included in the mesh-cache byte "
          "budget");
  }
  auto v = sphere();
  auto base = v.geometry_snapshot();
  auto owner = base.geometry_owner();
  auto cache = LocalSurfaceCache::build(v);
  std::weak_ptr<const Volume::VoxelStorage> expired;
  for (int i = 0; i < 8; ++i) {
    stroke(v, brush({.32f, float(i - 4) * .016f, .07f},
                    i % 2 ? Tool::Clay : Tool::Carve));
    if (i == 2)
      expired = v.geometry_owner();
  }
  check(expired.expired(), "Intermediate COW maps expire without being "
                           "retained by the bounded journal");
  auto change = v.geometry_change(owner);
  complete(base, v, change);
  check(
      change.events == 8,
      "Skipped preview revisions coalesce through weak control-block ancestry");
  LocalSurfaceUpdateReport report;
  cache = cache->update(v, {}, &report);
  check(report.kind == LocalSurfaceUpdateReport::Kind::Local &&
            cache->stats().samples_scanned == 0 &&
            same(cache->materialize(), surface_mesh(v)),
        "Skipped-revision local extraction matches independent whole-surface "
        "oracle without field scan");
  auto original = v.geometry_snapshot();
  auto before = cache;
  v.begin_stroke();
  v.stamp(brush({.28f, .07f, .02f}));
  auto preview = cache->update(v);
  v.cancel_stroke();
  check(
      v.geometry_owner() == original.geometry_owner() &&
          cache->update(v) == cache,
      "Cancel restores exact source identity and old cache without allocation");
  preview = preview->update(v, {}, &report);
  check(report.kind == LocalSurfaceUpdateReport::Kind::Rebuild &&
            same(preview->materialize(), surface_mesh(v)),
        "Canceled preview branch safely rebuilds when restored ancestry does "
        "not contain its owner");
  check(v.undo(), "Geometry undo available");
  auto undone = cache->update(v, {}, &report);
  check(
      report.kind == LocalSurfaceUpdateReport::Kind::Local &&
          same(undone->materialize(), surface_mesh(v)),
      "Undo publishes authenticated removal/change regions on a new COW owner");
  check(v.redo(), "Geometry redo available");
  cache = undone->update(v, {}, &report);
  check(report.kind == LocalSurfaceUpdateReport::Kind::Local &&
            same(cache->materialize(), surface_mesh(v)),
        "Redo publishes authenticated regions and preserves original cache "
        "immutability");
  auto mask_owner = v.geometry_owner();
  auto settings = v.sculpt_settings();
  settings.mask_edit = true;
  v.sculpt_settings(settings);
  v.fill_mask(.25f);
  v.invert_mask();
  stroke(v, brush({.3f, 0, 0}, Tool::Clay, .1f));
  v.undo();
  v.redo();
  check(v.geometry_owner() == mask_owner &&
            v.geometry_change(mask_owner).kind ==
                Volume::GeometryChange::Kind::Unchanged &&
            cache->update(v) == cache,
        "Mask/settings/Fill/Invert/stroke/undo/redo preserve geometry identity "
        "and skip tile work");
  auto encoded = v.encode();
  auto loaded = Volume::decode(encoded);
  cache = cache->update(loaded, {}, &report);
  check(report.kind == LocalSurfaceUpdateReport::Kind::Rebuild &&
            same(cache->materialize(), surface_mesh(loaded)),
        "Native reload starts unknown ephemeral ancestry and safely rebuilds "
        "without archive changes");
  auto smaller = Volume::from_samples(.05f, loaded.voxels());
  cache = cache->update(smaller, {}, &report);
  check(report.kind == LocalSurfaceUpdateReport::Kind::Rebuild &&
            same(cache->materialize(), surface_mesh(smaller)),
        "Voxel context replacement safely rebuilds");
  auto history = sphere();
  auto history_base = history.geometry_owner();
  auto history_cache = LocalSurfaceCache::build(history);
  for (int i = 0; i < 70; ++i)
    stroke(history, brush({.32f, float(i % 4 - 2) * .01f, .07f},
                          i % 2 ? Tool::Clay : Tool::Carve));
  check(history.geometry_change(history_base).kind ==
            Volume::GeometryChange::Kind::Rebuild,
        "Expired bounded ancestry explicitly requires a rebuild");
  history_cache = history_cache->update(history, {}, &report);
  check(report.kind == LocalSurfaceUpdateReport::Kind::Rebuild &&
            same(history_cache->materialize(), surface_mesh(history)),
        "Expired ancestry fallback preserves exact whole-surface geometry");
  {
    Volume mirrored(.04f);
    for (float x : {-1.f, 0.f, 1.f})
      stroke(mirrored, brush({x, 0, 0}, Tool::Clay, .2f));
    auto old = mirrored.geometry_snapshot();
    auto tiles = LocalSurfaceCache::build(mirrored);
    auto settings = mirrored.sculpt_settings();
    settings.symmetry_axes = 1;
    mirrored.sculpt_settings(settings);
    stroke(mirrored, brush({1, 0, .18f}));
    auto delta = mirrored.geometry_change(old.geometry_owner());
    complete(old, mirrored, delta);
    check(delta.count >= 2 &&
              std::none_of(
                  delta.regions().begin(), delta.regions().end(),
                  [](auto r) { return r.minimum.x <= 0 && r.maximum.x >= 0; }),
          "Distant mirrored edits retain disjoint regions instead of "
          "invalidating the empty gap");
    auto next = tiles->update(mirrored, {}, &report);
    size_t central = 0;
    for (auto &[key, tile] : tiles->tiles())
      if (key.x >= -1 && key.x <= 0) {
        auto f = next->tiles().find(key);
        if (f != next->tiles().end() && f->second == tile)
          ++central;
      }
    check(central > 0 && report.kind == LocalSurfaceUpdateReport::Kind::Local &&
              same(next->materialize(), surface_mesh(mirrored)),
          "Unrelated central geometry keeps immutable tiles across mirrored "
          "updates");
  }
  {
    auto seed = sphere();
    auto b = brush({.32f, .03f, .02f});
    size_t total;
    {
      auto probe = seed.geometry_snapshot();
      probe.begin_stroke();
      arm_backend = true;
      fail_after_backend = -1;
      probe.stamp(b);
      total = allocations;
      reset_allocations();
      probe.cancel_stroke();
    }
    check(total > 8, "Allocation probe reaches staged map and immutable "
                     "journal preparation");
    for (size_t last = 1; last <= 8; ++last) {
      auto attempt = seed.geometry_snapshot();
      auto old = attempt.geometry_snapshot();
      auto revision = attempt.revision();
      attempt.begin_stroke();
      arm_backend = true;
      fail_after_backend = int64_t(total - last);
      bool rejected = false;
      try {
        attempt.stamp(b);
      } catch (const std::bad_alloc &) {
        rejected = true;
      }
      reset_allocations();
      check(rejected && attempt.geometry_owner() == old.geometry_owner() &&
                attempt.revision() == revision && !attempt.pending_changes() &&
                attempt.layers()[0].delta.owner() ==
                    old.layers()[0].delta.owner(),
            "Late preparation/journal allocation fault leaves geometry, delta "
            "and provenance atomically unchanged");
      fail_allocation = 0;
      attempt.cancel_stroke();
      reset_allocations();
      check(attempt.geometry_change(old.geometry_owner()).kind ==
                Volume::GeometryChange::Kind::Unchanged,
            "Allocation-disabled Cancel restores exact geometry and journal");
    }
    auto active = seed.geometry_snapshot();
    auto start = active.geometry_owner();
    active.begin_stroke();
    active.stamp(b);
    fail_allocation = 0;
    active.cancel_stroke();
    reset_allocations();
    check(active.geometry_owner() == start &&
              active.geometry_change(start).kind ==
                  Volume::GeometryChange::Kind::Unchanged,
          "Allocation-disabled Cancel after successful geometry publication "
          "restores exact start state");
  }
  {
    auto base = sphere();
    auto b = brush({.32f, .03f, .02f});
    auto committed = [&] {
      auto v = base.geometry_snapshot();
      stroke(v, b);
      return v;
    };
    late_faults(
        committed,
        [](Volume &v) {
          if (!v.undo())
            throw std::runtime_error("fixture undo missing");
        },
        "Undo allocation failure preserves geometry, history and journal "
        "atomically");
    auto undone = [&] {
      auto v = committed();
      v.undo();
      return v;
    };
    late_faults(
        undone,
        [](Volume &v) {
          if (!v.redo())
            throw std::runtime_error("fixture redo missing");
        },
        "Redo allocation failure preserves geometry, history and journal "
        "atomically");
    late_faults([&] { return base.geometry_snapshot(); },
                [](Volume &v) {
                  v.layer_properties(v.active_layer(), "opacity", true, false,
                                     .75f);
                },
                "Layer recomposition allocation failure preserves layers, "
                "geometry and journal atomically");
    auto active = [&] {
      auto v = base.geometry_snapshot();
      v.begin_stroke();
      v.stamp(b);
      return v;
    };
    late_faults(
        active, [](Volume &v) { v.end_stroke(); },
        "End-stroke allocation failure preserves pending geometry and "
        "immutable provenance");
    auto mixed = [] {
      Volume v(.03f);
      v.begin_stroke();
      v.stamp(brush({}, Tool::Clay, .04f));
      v.stamp(brush({}, Tool::Carve, .12f));
      return v;
    };
    late_faults(
        mixed, [](Volume &v) { v.end_stroke(); },
        "Mixed net-zero cleanup allocation failure preserves active-layer "
        "displacement owner");
    auto probe = base.geometry_snapshot();
    probe.begin_stroke();
    size_t polls = 0;
    probe.stamp(b, 4'000'000, [&] {
      ++polls;
      return false;
    });
    probe.cancel_stroke();
    auto attempt = base.geometry_snapshot();
    auto owner = attempt.geometry_owner();
    attempt.begin_stroke();
    size_t seen = 0;
    bool rejected = false;
    try {
      attempt.stamp(b, 4'000'000, [&] { return ++seen == polls; });
    } catch (const std::runtime_error &) {
      rejected = true;
    }
    check(rejected && attempt.geometry_owner() == owner &&
              !attempt.pending_changes(),
          "Cancellation at final post-journal checkpoint publishes neither "
          "geometry nor provenance");
    attempt.cancel_stroke();
    auto v = committed();
    auto owner0 = base.geometry_owner();
    fail_allocation = 0;
    auto query = v.geometry_change(owner0);
    reset_allocations();
    check(query.kind == Volume::GeometryChange::Kind::Local,
          "Authenticated ancestry query allocates no memory");
  }
  {
    Document d;
    Scene scene;
    scene.active().volume = std::make_shared<Volume>(sphere());
    auto first = scene.active().id;
    CanvasTransform root;
    root.position = {.12f, -.04f, .03f};
    root.scale = 1.25f;
    auto group = scene.add_group("local mesh fixtures", root);
    scene.active().group = group;
    CanvasTransform second;
    second.position = {1, 0, 0};
    auto other = scene.add("other", sphere(), second);
    scene.object(other).group = group;
    scene.select(first);
    d.replace(std::move(scene));
    ready(d);
    auto held = d.frame().surface;
    check(bool(held->geometry_identity),
          "Product mesh publishes a strong opaque geometry identity");
    check(held->sculpt_meshing.full_rebuilds == 2 &&
              same(held->mesh, whole_scene(d.copy_scene())),
          "Actual Document worker initially builds both grouped source objects "
          "exactly");
    for (int i = 0; i < 12; ++i) {
      doc_stroke(d, brush({.32f, float(i - 6) * .012f, .06f},
                          i % 2 ? Tool::Clay : Tool::Carve));
      if (i % 3 == 2)
        ready(d);
    }
    ready(d);
    auto frame = d.frame();
    check(frame.surface->sculpt_meshing.local_updates == 1 &&
              frame.surface->sculpt_meshing.objects_reused == 1 &&
              frame.surface->sculpt_meshing.samples_scanned == 0 &&
              same(frame.surface->mesh, whole_scene(d.copy_scene())),
          "Real asynchronous worker coalesces long strokes while retaining "
          "another grouped object's tiles");
    check(frame.surface->geometry_identity != held->geometry_identity,
          "Actual geometry edits invalidate the upload identity");
    check(held->mesh.indices.size() != 0 &&
              held->sculpt_meshing.full_rebuilds == 2,
          "Previously acquired immutable Document snapshot remains unchanged");
    check(d.undo(), "Document undo available");
    ready(d);
    check(same(d.frame().surface->mesh, whole_scene(d.copy_scene())),
          "Document history undo preserves whole-scene oracle equivalence");
    check(d.redo(), "Document redo available");
    ready(d);
    check(same(d.frame().surface->mesh, whole_scene(d.copy_scene())),
          "Document history redo preserves whole-scene oracle equivalence");
    auto geometry_token = d.frame().surface->geometry_identity;
    auto settings = d.frame().state->sculpt_settings;
    settings.mask_edit = true;
    d.sculpt_settings(settings);
    ready(d);
    check(d.frame().surface->geometry_identity == geometry_token,
          "Mask-control publications retain exact static geometry identity");
    auto stats = d.frame().surface->sculpt_meshing;
    check(stats.objects_reused == 2 && stats.rebuilt_tiles == 0 &&
              stats.materialize_ms == 0,
          "Mask-only Document publication reuses both meshes without "
          "extraction or materialization");
    doc_stroke(d, brush({.3f, 0, 0}, Tool::Clay, .1f));
    ready(d);
    stats = d.frame().surface->sculpt_meshing;
    check(stats.objects_reused == 2 && stats.rebuilt_tiles == 0 &&
              same(d.frame().surface->mesh, whole_scene(d.copy_scene())),
          "Actual mask stroke changes overlay while preserving cached grouped "
          "geometry");
    check(d.frame().surface->geometry_identity == geometry_token,
          "Mask painting preserves static geometry identity while scalar "
          "overlay changes");
    auto object = d.copy_scene().active();
    auto material = object.material;
    material.color = {.1f, .6f, .2f, 1};
    d.object_properties(object.id, object.name, object.visible, object.locked,
                        object.transform, material);
    ready(d);
    check(d.frame().surface->geometry_identity == geometry_token,
          "Material-factor-only publication reuses geometry identity");
    auto moved = object.transform;
    moved.position.x += .01f;
    d.object_properties(object.id, object.name, object.visible, object.locked,
                        moved, material);
    ready(d);
    check(d.frame().surface->geometry_identity != geometry_token &&
              same(d.frame().surface->mesh, whole_scene(d.copy_scene())),
          "World/group transform dependencies invalidate static geometry "
          "identity");
    d.replace(sphere());
    ready(d);
    check(d.frame().surface->sculpt_meshing.full_rebuilds == 1 &&
              same(d.frame().surface->mesh, whole_scene(d.copy_scene())),
          "Generation replacement cannot reuse old-object cache ancestry");
  }
  {
    Mesh triangle;
    triangle.positions = {{-.2f, -.2f, 0}, {.2f, -.2f, 0}, {0, .2f, 0}};
    triangle.normals.assign(3, {0, 0, 1});
    triangle.indices = {0, 1, 2};
    triangle.uv = {{0, 0}, {1, 0}, {.5f, 1}};
    Scene scene;
    scene.active().painted = std::make_shared<PaintedMesh>(triangle, 64);
    scene.active().mesh_active = true;
    Document d;
    d.replace(std::move(scene));
    ready(d);
    auto before = d.frame().surface;
    auto source = d.copy_scene().active().painted->mesh_owner();
    auto settings = d.frame().state->paint;
    settings.front_only = false;
    settings.color = {.1f, .6f, .2f, 1};
    d.paint_settings(settings);
    doc_stroke(d, brush({0, 0, 0}, Tool::Clay, .16f));
    ready(d);
    auto after = d.frame().surface;
    check(d.copy_scene().active().painted->has_paint_data() &&
              after->ranges[0].textures->color.rgba !=
                  before->ranges[0].textures->color.rgba,
          "Product color painting actually changes texture data");
    check(d.copy_scene().active().painted->mesh_owner() == source &&
              after->geometry_identity == before->geometry_identity,
          "Color painting retains immutable SurfaceMap mesh owner and geometry "
          "upload token");
    d.edit_scene("fixture UV replacement", [](Scene &s, const auto &) {
      auto mesh = s.active().painted->mesh();
      mesh.uv[0][0] += .02f;
      s.active().painted = std::make_shared<PaintedMesh>(std::move(mesh), 64);
    });
    if (!d.wait_asset())
      throw std::runtime_error("UV replacement asset fixture timeout");
    ready(d);
    check(d.frame().surface->geometry_identity != after->geometry_identity &&
              d.frame().surface->mesh.uv != after->mesh.uv,
          "Actual UV/layout replacement invalidates static geometry identity");
    auto token = d.frame().surface->geometry_identity;
    auto same_scene = d.copy_scene();
    d.replace(std::move(same_scene));
    ready(d);
    check(d.frame().surface->geometry_identity != token,
          "Identical geometry in a replacement generation still receives a new "
          "token");
  }
  {
    Document d;
    d.replace(sphere());
    ready(d);
    auto held = d.frame().surface;
    fail_materialization = true;
    doc_stroke(d, brush({.32f, .03f, .01f}));
    auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
    while (d.frame().state->error.empty() &&
           std::chrono::steady_clock::now() < deadline)
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    check(!d.frame().state->error.empty() && d.frame().surface == held,
          "Actual worker allocation failure retains prior immutable geometry "
          "and upload identity");
    d.report_error({});
    doc_stroke(d, brush({.32f, -.03f, .02f}, Tool::Clay));
    ready(d);
    check(d.frame().surface->sculpt_meshing.local_updates == 1 &&
              same(d.frame().surface->mesh, whole_scene(d.copy_scene())),
          "Mesh worker recovers after failed publication using authenticated "
          "skipped-event ancestry");
  }
  std::cout << checks << " authenticated local surface checks passed\n";
} catch (const std::exception &e) {
  reset_allocations();
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
