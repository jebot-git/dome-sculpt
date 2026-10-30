// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/local-surface.hpp"
#include "../src/surface-topology.hpp"
#include <bit>
#include <chrono>
#include <iostream>
#include <random>
#include <set>
using namespace dome;
namespace dome {
Mesh legacy_surface_mesh(const Volume &,
                         const std::function<bool()> &cancelled = {});
}
namespace {
int checks = 0;
void check(bool yes, const char *label) {
  if (!yes)
    throw std::runtime_error(label);
  ++checks;
  std::cout << "PASS " << label << '\n';
}
template <class F> bool fails(F f) {
  try {
    f();
    return false;
  } catch (const std::exception &) {
    return true;
  }
}
bool same(Vec3 a, Vec3 b) {
  return std::bit_cast<std::array<uint32_t, 3>>(a) ==
         std::bit_cast<std::array<uint32_t, 3>>(b);
}
bool same(const Mesh &a, const Mesh &b) {
  bool equal =
      a.indices == b.indices && a.positions.size() == b.positions.size() &&
      a.normals.size() == b.normals.size() &&
      std::equal(a.positions.begin(), a.positions.end(), b.positions.begin(),
                 [](auto a, auto b) { return same(a, b); }) &&
      std::equal(a.normals.begin(), a.normals.end(), b.normals.begin(),
                 [](auto a, auto b) { return same(a, b); });
  if (!equal) {
    std::cerr << "DIFF positions " << a.positions.size() << " vs "
              << b.positions.size() << " indices " << a.indices.size() << " vs "
              << b.indices.size() << '\n';
    for (size_t i = 0; i < std::min(a.indices.size(), b.indices.size()); ++i)
      if (a.indices[i] != b.indices[i]) {
        std::cerr << " first index " << i << " " << a.indices[i] << " vs "
                  << b.indices[i] << '\n';
        break;
      }
    for (size_t i = 0; i < std::min(a.positions.size(), b.positions.size());
         ++i)
      if (!same(a.positions[i], b.positions[i])) {
        std::cerr << " first position " << i << '\n';
        break;
      }
    for (size_t i = 0; i < std::min(a.normals.size(), b.normals.size()); ++i)
      if (!same(a.normals[i], b.normals[i])) {
        std::cerr << " first normal " << i << '\n';
        break;
      }
  }
  return equal;
}

bool closed(const Mesh &m) {
  std::map<std::pair<uint32_t, uint32_t>, std::pair<int, int>> count;
  for (size_t i = 0; i < m.indices.size(); i += 3)
    for (int j = 0; j < 3; ++j) {
      auto a = m.indices[i + j], b = m.indices[i + (j + 1) % 3];
      int sign = a < b ? 1 : -1;
      if (a > b)
        std::swap(a, b);
      auto &e = count[{a, b}];
      ++e.first;
      e.second += sign;
    }
  return !count.empty() && std::all_of(count.begin(), count.end(), [](auto &x) {
    return x.second.first == 2 && x.second.second == 0;
  });
}
std::set<std::array<float, 3>> positions(const Mesh &m) {
  std::set<std::array<float, 3>> result;
  for (auto p : m.positions)
    result.insert({p.x, p.y, p.z});
  return result;
}
double area(const Mesh &m) {
  double sum = 0;
  for (size_t t = 0; t < m.indices.size(); t += 3) {
    auto a = m.positions[m.indices[t]], b = m.positions[m.indices[t + 1]],
         c = m.positions[m.indices[t + 2]];
    double x = double(b.x) - a.x, y = double(b.y) - a.y, z = double(b.z) - a.z,
           u = double(c.x) - a.x, v = double(c.y) - a.y, w = double(c.z) - a.z;
    double nx = y * w - z * v, ny = z * u - x * w, nz = x * v - y * u;
    sum += std::sqrt(nx * nx + ny * ny + nz * nz) * .5;
  }
  return sum;
}

Volume shape(int mode, float voxel = .04f, Int3 offset = {},
             float radius = 9.35f) {
  Volume::Voxels samples;
  for (int x = -14; x <= 14; ++x)
    for (int y = -14; y <= 14; ++y)
      for (int z = -14; z <= 14; ++z) {
        Vec3 p{x * voxel, y * voxel, z * voxel};
        float d = length(p) - radius * voxel;
        if (mode == 1)
          d = std::max(d, 4 * voxel - length(p));
        if (mode == 2)
          d = std::sqrt(
                  std::pow(std::sqrt(p.x * p.x + p.z * p.z) - 7 * voxel, 2) +
                  p.y * p.y) -
              2.35f * voxel;
        if (mode == 3)
          d = (std::max({std::abs(x), std::abs(y), std::abs(z)}) - 8) * voxel;
        samples.emplace(Int3{x, y, z} + offset,
                        std::clamp(d, -4 * voxel, 4 * voxel));
      }
  return Volume::from_samples(voxel, std::move(samples));
}
std::vector<SurfaceSampleRegion> changed(const Volume &a, const Volume &b) {
  std::set<Int3> keys;
  for (auto &[p, d] : a.voxels())
    if (b.at(p) != d)
      keys.insert(p);
  for (auto &[p, d] : b.voxels())
    if (a.at(p) != d)
      keys.insert(p);
  if (keys.empty())
    return {};
  Int3 lo = *keys.begin(), hi = lo;
  for (auto p : keys) {
    lo = {std::min(lo.x, p.x), std::min(lo.y, p.y), std::min(lo.z, p.z)};
    hi = {std::max(hi.x, p.x), std::max(hi.y, p.y), std::max(hi.z, p.z)};
  }
  return {{lo, hi}};
}
void stamp(Volume &v, Vec3 p, Tool tool = Tool::Clay, float radius = .08f) {
  Brush b;
  b.from = b.to = p;
  b.radius = radius;
  b.tool = tool;
  b.blend = 0;
  v.begin_stroke();
  v.stamp(b);
  v.end_stroke();
}
double ms(auto action) {
  auto start = std::chrono::steady_clock::now();
  action();
  return std::chrono::duration<double, std::milli>(
             std::chrono::steady_clock::now() - start)
      .count();
}
} // namespace
int main(int argc, char **argv) try {
  if (argc > 1 && std::string(argv[1]) == "--benchmark") {
    Volume field(.025f);
    stamp(field, {}, Tool::Clay, .6f);
    LocalSurfaceCache::Ptr cache;
    double initial = ms([&] { cache = LocalSurfaceCache::build(field); });
    double local = 0, reference = 0, flatten = 0, legacy = 0;
    size_t built = 0, reused = 0;
    for (int i = 0; i < 24; ++i) {
      auto old = field.geometry_snapshot();
      stamp(field, {.48f, float(i - 12) * .012f, .25f},
            i % 2 ? Tool::Carve : Tool::Clay, .065f);
      auto regions = changed(old, field);
      if (regions.empty())
        continue;
      local += ms([&] {
        cache = cache->update(field, cache->source_owner(), regions);
      });
      built += cache->stats().tiles_built;
      reused += cache->stats().tiles_reused;
      Mesh ref, out;
      legacy += ms([&] {
        auto baseline = legacy_surface_mesh(field);
        if (baseline.indices.empty())
          throw std::runtime_error("Empty benchmark baseline");
      });
      reference += ms([&] { ref = surface_mesh(field); });
      flatten += ms([&] { out = cache->materialize(); });
      if (!same(ref, out))
        throw std::runtime_error("Benchmark output differs from reference");
    }
    std::cout << "BENCH initial_ms=" << initial << " local24_ms=" << local
              << " legacy24_ms=" << legacy << " reference24_ms=" << reference
              << " materialize24_ms=" << flatten << " rebuilt_tiles=" << built
              << " reused_tiles=" << reused
              << " retained_bytes=" << cache->stats().bytes << '\n';
    return 0;
  }
  {
    Mesh planar;
    planar.positions = {{0, 0, 0}, {2, 0, 0}, {0, 2, 0},
                        {1, 0, 0}, {1, 1, 0}, {0, 1, 0}};
    planar.normals.assign(6, {0, 0, 1});
    planar.indices = {0, 1, 2};
    std::array<surface_detail::Triangle, 3> requests{
        {{0, 3, 1}, {1, 4, 2}, {2, 5, 0}}};
    auto before = planar;
    surface_detail::complete_collinear_surface(planar, requests, {}, 16'000'000,
                                               48'000'000, false);
    planar.validate();
    std::set<uint32_t> used(planar.indices.begin(), planar.indices.end());
    check(planar.indices.size() == 12 && used.size() == 6 &&
              area(planar) == area(before),
          "Exact multi-edge subdivision preserves all collinear boundary "
          "vertices and face area");
    check(fails([&] {
            surface_detail::complete_collinear_surface(before, requests, {}, 1);
          }) &&
              before.indices == std::vector<uint32_t>{0, 1, 2},
          "Exact subdivision work limit refuses atomically");
  }
  check(LocalSurfaceCache::tile_coordinate({-1, -8, -9}) == Int3{-1, -1, -2},
        "Tile division floors negative lattice coordinates");
  for (int i = 0; i < 4; ++i) {
    auto field = shape(i, .04f, {-9, 0, 7});
    auto cache = LocalSurfaceCache::build(field);
    auto m = cache->materialize();
    m.validate();
    check(same(m, surface_mesh(field)),
          "Sphere/cavity/torus/exact-zero box matches reference bit-for-bit");
    check(closed(m), "Shared tile seam edge IDs yield a closed valid surface");
  }
  for (int seed = 0; seed < 8; ++seed) {
    Volume::Voxels data;
    std::mt19937 random(seed);
    for (int x = -3; x <= 3; ++x)
      for (int y = -3; y <= 3; ++y)
        for (int z = -3; z <= 3; ++z)
          data.emplace(Int3{x, y, z}, float(int(random() % 9) - 4) * .01f);
    auto field = Volume::from_samples(.04f, std::move(data));
    check(same(LocalSurfaceCache::build(field)->materialize(),
               surface_mesh(field)),
          "Random signed and exact-zero tetrahedra preserve reference ordering "
          "and degeneracy");
  }
  {
    auto field = shape(0, .04f, {-9, 0, 7}, 9);
    auto old = legacy_surface_mesh(field), fixed = surface_mesh(field);
    std::cout << "CLOSURE legacyV=" << old.positions.size()
              << " legacyF=" << old.indices.size() / 3
              << " fixedV=" << fixed.positions.size()
              << " fixedF=" << fixed.indices.size() / 3 << '\n';
    std::map<std::pair<uint32_t, uint32_t>, int> incidence;
    for (size_t t = 0; t < fixed.indices.size(); t += 3)
      for (int k = 0; k < 3; ++k) {
        auto a = fixed.indices[t + k], b = fixed.indices[t + (k + 1) % 3];
        if (a > b)
          std::swap(a, b);
        ++incidence[{a, b}];
      }
    for (auto [edge, count] : incidence)
      if (count != 2) {
        auto a = fixed.positions[edge.first], b = fixed.positions[edge.second];
        std::cerr << "OPEN " << edge.first << "," << edge.second
                  << " count=" << count << " a=" << a.x << "," << a.y << ","
                  << a.z << " b=" << b.x << "," << b.y << "," << b.z << '\n';
      }
    check(!closed(old) && closed(fixed),
          "Exact representable-position welding repairs inherited near-zero "
          "closure defect");
    check(positions(old) == positions(fixed) &&
              std::abs(area(old) - area(fixed)) < 1e-13,
          "Exact closure completion preserves every unique position and total "
          "surface area");
    check(same(LocalSurfaceCache::build(field)->materialize(), fixed),
          "Tiled extraction matches corrected global exact-position policy");
  }
  {
    auto original = shape(0);
    auto cache = LocalSurfaceCache::build(original);
    auto data = original.voxels();
    Int3 q{11, 1, 0};
    data[q] = original.at(q) * .5f;
    auto edited = Volume::from_samples(original.voxel_size(), std::move(data));
    std::array<SurfaceSampleRegion, 1> region{{{q, q}}};
    auto updated = cache->update(edited, cache->source_owner(), region);
    auto a = cache->materialize(), b = updated->materialize();
    check(a.indices == b.indices && a.positions.size() == b.positions.size() &&
              std::equal(a.positions.begin(), a.positions.end(),
                         b.positions.begin(),
                         [](auto a, auto b) { return same(a, b); }) &&
              !std::equal(a.normals.begin(), a.normals.end(), b.normals.begin(),
                          [](auto a, auto b) { return same(a, b); }),
          "Positive-only gradient support edit changes normals without moving "
          "crossings");
    check(same(b, surface_mesh(edited)),
          "Dirty halo refreshes positive-only central-difference dependencies");
    auto layer = original.add_layer("metadata");
    original.select_layer(layer);
    check(cache->update(original, cache->source_owner(), {}) == cache,
          "Layer/view metadata changes reuse exact geometry owner despite "
          "revision changes");
  }
  {
    auto field = shape(2, .0001f, {100000, -100000, 99999});
    auto cache = LocalSurfaceCache::build(field);
    auto data = field.voxels();
    Int3 q{100006, -99999, 99999};
    data[q] = -.0001f;
    auto edited = Volume::from_samples(field.voxel_size(), std::move(data));
    std::array<SurfaceSampleRegion, 1> region{{{q, q}}};
    auto next = cache->update(edited, cache->source_owner(), region);
    check(same(next->materialize(), surface_mesh(edited)),
          "Large signed lattice coordinates use measured float sampling halo "
          "exactly");
  }
  for (int i = 0; i < 6; ++i) {
    auto field = shape(i % 3, .03f + float(i) * .001f,
                       {i * 7 - 20, 3 - i, i * 5}, 9.35f);
    auto fixed = surface_mesh(field);
    check(
        same(fixed, legacy_surface_mesh(field)),
        "Ordinary well-resolved surfaces retain frozen legacy output exactly");
    check(closed(fixed), "Translated scale variants retain consistently "
                         "oriented closed topology");
  }
  auto field = shape(0);
  auto cache = LocalSurfaceCache::build(field);
  auto before = cache->materialize();
  auto old = field.geometry_snapshot();
  stamp(field, {.36f, .05f, .03f});
  auto regions = changed(old, field);
  auto update = cache->update(field, cache->source_owner(), regions);
  check(same(update->materialize(), surface_mesh(field)),
        "Localized geometry update equals full reference extraction");
  size_t shared = 0;
  for (auto &[key, tile] : cache->tiles())
    if (auto f = update->tiles().find(key);
        f != update->tiles().end() && f->second == tile)
      ++shared;
  check(shared > 0 && shared == update->stats().tiles_reused &&
            update->stats().samples_scanned == 0,
        "Untouched immutable tiles reused without scanning field samples");
  check(same(before, cache->materialize()),
        "An older cache and its strongly-held COW source survive editing "
        "unchanged");
  check(update->update(field, update->source_owner(), {}) == update,
        "Geometry owner no-op returns same cache without flattening");
  check(fails([&] { cache->update(field, update->source_owner(), regions); }),
        "Cross-object or wrong-branch base owner is refused");
  check(fails([&] { cache->update(field, cache->source_owner(), {}); }),
        "Changed geometry with missing dirty provenance is refused");
  auto active = update;
  check(field.undo(), "Fixture stroke can undo");
  auto undo = active->update(field, active->source_owner(), regions);
  check(same(undo->materialize(), before),
        "Undo dirty sample removals restore exact prior surface");
  check(field.redo(), "Fixture stroke can redo");
  auto redo = undo->update(field, undo->source_owner(), regions);
  check(same(redo->materialize(), active->materialize()),
        "Redo creates equivalent geometry on a new COW branch");
  auto empty = Volume(field.voxel_size());
  auto remove = changed(field, empty);
  auto gone = redo->update(empty, redo->source_owner(), remove);
  check(gone->tiles().empty() && gone->materialize().indices.empty(),
        "Removing all negative samples erases stale tile geometry");
  Volume blank(.04f);
  auto blankcache = LocalSurfaceCache::build(blank);
  check(blankcache->materialize().indices.empty(),
        "An empty volume produces a valid empty cache");
  for (int i = 0; i < 16; ++i) {
    auto previous = field.geometry_snapshot();
    stamp(field, {float(i - 8) * .045f, .34f, .05f},
          i % 3 == 0 ? Tool::Carve : Tool::Clay, .07f);
    auto delta = changed(previous, field);
    if (!delta.empty())
      redo = redo->update(field, redo->source_owner(), delta);
    check(same(redo->materialize(), surface_mesh(field)),
          "Long mixed-sign stroke sequence remains reference-equivalent");
  }
  int polls = 0;
  check(fails([&] {
          LocalSurfaceCache::build(field, {}, [&] { return ++polls > 5; });
        }),
        "Bootstrap cancellation interrupts bounded work");
  polls = 0;
  check(fails([&] { redo->materialize([&] { return ++polls > 3; }); }),
        "Materialization cancellation interrupts assembly");
  auto previous = field.geometry_snapshot();
  stamp(field, {.2f, .3f, .1f}, Tool::Carve, .1f);
  auto delta = changed(previous, field);
  polls = 0;
  check(fails([&] {
          redo->update(field, redo->source_owner(), delta,
                       [&] { return ++polls > 2; });
        }) &&
            same(redo->materialize(), surface_mesh(previous)),
        "Canceled local update preserves published cache atomically");
  for (int mode = 0; mode < 5; ++mode) {
    LocalSurfaceLimits l;
    if (mode == 0)
      l.samples = 1;
    if (mode == 1)
      l.tiles = 1;
    if (mode == 2)
      l.evaluated_cells = 1;
    if (mode == 3)
      l.bytes = 100;
    if (mode == 4)
      l.vertices = 1;
    check(fails([&] {
            auto x = LocalSurfaceCache::build(field, l);
            x->materialize();
          }),
          "Sample/tile/work/byte/vertex limits refuse bounded work");
  }
  std::array<SurfaceSampleRegion, 1> bad{{{{INT32_MIN, 0, 0}, {0, 0, 0}}}};
  check(fails([&] { redo->update(field, redo->source_owner(), bad); }),
        "Dirty coordinate overflow refuses before tile arithmetic");
  std::cout << checks << " local surface checks passed\n";
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
