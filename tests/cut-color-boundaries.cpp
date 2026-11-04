// SPDX-License-Identifier: GPL-3.0-or-later
#include "cut-color-fixture.hpp"
#include "cut-color.hpp"
#include "dome/material-bindings.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
namespace {
thread_local bool deny = false, hit = false;
thread_local size_t allocations = 0, boundary = SIZE_MAX;
void allocation_check() {
  if (deny && ++allocations >= boundary) {
    hit = true;
    throw std::bad_alloc();
  }
}
using namespace dome;
size_t checks = 0;
void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
  ++checks;
  std::cout << "PASS " << message << std::endl;
}
template <class F> void refuses(F action, const char *message) {
  bool caught = false;
  try {
    action();
  } catch (const std::exception &) {
    caught = true;
  }
  check(caught, message);
}
Scene posed(bool affine) {
  auto scene = cut_color_fixture::scene(true, affine);
  const auto &mesh = scene.active().painted->mesh();
  DeformationState pose(mesh);
  pose.skeleton = Skeleton{
      {{"root", -1, {}, .4f}, {"upper", 0, {{0, .1f, 0}, {}, {1, 1, 1}}, .2f}}};
  pose.skin = SkinWeights{};
  for (auto p : mesh.positions)
    pose.skin->vertices.push_back(
        p.y > 0 ? std::vector<Influence>{{0, .25f}, {1, .75f}}
                : std::vector<Influence>{{0, 1}});
  pose.pose.resize(2);
  pose.pose[0].translation = {.013f, -.009f, .007f};
  pose.pose[1].translation = {0, .1f, 0};
  pose.pose[1].rotation = {.99875026f, 0, 0, .04997917f};
  pose.morphs = MorphSet(mesh);
  auto target = mesh.positions;
  for (auto &p : target) {
    p.x += .04f * p.y;
    p.z += .02f * p.x;
  }
  pose.morphs->set_target(mesh, "Native artist offset", target);
  pose.morph_weights = {.6f};
  scene.set_deformation(scene.active().id, std::move(pose));
  return scene;
}
Mesh world_surface(const Scene &scene) {
  Mesh out;
  for (auto surface : scene.preview_surfaces()) {
    auto mesh = surface.preview_mesh();
    auto frame = detail::placement_frame(surface);
    detail::place_mesh(mesh, frame);
    if (frame.orientation() == AffineOrientation::Reversing)
      for (size_t c = 0; c < mesh.indices.size(); c += 3)
        std::swap(mesh.indices[c + 1], mesh.indices[c + 2]);
    auto offset = uint32_t(out.positions.size());
    out.positions.insert(out.positions.end(), mesh.positions.begin(),
                         mesh.positions.end());
    out.normals.insert(out.normals.end(), mesh.normals.begin(),
                       mesh.normals.end());
    for (auto i : mesh.indices)
      out.indices.push_back(offset + i);
  }
  return out;
}
void source_exact(const Scene &scene, const nlohmann::json &expected,
                  const Scene &owners) {
  check(scene.encode() == expected &&
            scene.active().polygon == owners.active().polygon &&
            scene.active().shape_source == owners.active().shape_source &&
            scene.active().painted->mesh_owner() ==
                owners.active().painted->mesh_owner() &&
            scene.active().deformation == owners.active().deformation,
        "Refusal preserves complete scene, field/paint/shape/pose owners and "
        "ID counters");
}
void field_transfers() {
  auto scene = cut_color_fixture::scene(true);
  const auto &topology = scene.active().polygon->topology;
  const auto &field = *scene.active().polygon->colors;
  const auto before = topology.encode();
  std::vector<PolygonCornerSource> transfers;
  for (const auto &face : topology.faces())
    for (const auto &c : face.corners)
      transfers.push_back({c.id, {{c.id, 1}}});
  auto result = transfer_polygon_colors(topology, field, topology, transfers);
  check(result.source_owner() == field.source_owner() &&
            result.packed(topology) == field.packed(topology),
        "Exact field transfer shares immutable source basis and preserves seam "
        "RGBA without recoding");
  auto bad = transfers;
  bad.pop_back();
  refuses([&] { transfer_polygon_colors(topology, field, topology, bad); },
          "Missing target corner witness refuses");
  bad = transfers;
  bad.push_back(bad.front());
  refuses([&] { transfer_polygon_colors(topology, field, topology, bad); },
          "Duplicate target corner witness refuses");
  bad = transfers;
  bad.back().corner = UINT64_MAX;
  refuses([&] { transfer_polygon_colors(topology, field, topology, bad); },
          "Orphan target witness refuses");
  for (double weight : {-.1, 0., 1.1, std::numeric_limits<double>::infinity(),
                        std::numeric_limits<double>::quiet_NaN()}) {
    bad = transfers;
    bad.front().weights.front().second = weight;
    refuses([&] { transfer_polygon_colors(topology, field, topology, bad); },
            "Invalid nonconvex or nonfinite field weights refuse");
  }
  bad = transfers;
  bad.front().weights.front().first = UINT64_MAX;
  refuses([&] { transfer_polygon_colors(topology, field, topology, bad); },
          "Unknown original source corner refuses");
  bad = transfers;
  auto id = bad.front().weights.front().first;
  bad.front().weights = {{id, .5}, {id, .5}};
  refuses([&] { transfer_polygon_colors(topology, field, topology, bad); },
          "Repeated source-corner alias cannot conceal an invalid witness");
  size_t calls = 0;
  PolygonLimits limits;
  limits.cancelled = [&] {
    ++calls;
    return false;
  };
  transfer_polygon_colors(topology, field, topology, transfers, limits);
  const auto measured = calls;
  for (size_t at = 1; at <= measured; ++at) {
    calls = 0;
    limits.cancelled = [&] { return ++calls >= at; };
    bool caught = false;
    try {
      transfer_polygon_colors(topology, field, topology, transfers, limits);
    } catch (const std::runtime_error &) {
      caught = true;
    }
    if (!caught || topology.encode() != before ||
        field.source_owner() != result.source_owner())
      throw std::runtime_error(
          "Every actual field cancellation must retain original input/basis");
  }
  check(measured > 100, "Every measured field-transfer cancellation boundary "
                        "is reached and transactional");
  std::cout << measured << " actual field-transfer cancellation boundaries\n";
}
void ordinary() {
  field_transfers();
  auto original = posed(true);
  auto baseline = original.encode();
  auto source = cut_color_fixture::source(original);
  CutSettings settings;
  settings.offset_mm = 31;
  auto preview = preview_cut(source.mesh, settings);
  auto invoke = [&](Scene &scene, const CutPreview &view,
                    const BakeSource &high,
                    const detail::CutColorLimits &limits = {}) {
    return detail::publish_colored_cut(scene, view, high, 64, {}, {}, limits);
  };
  auto result = original.snapshot();
  auto report = invoke(result, preview, source);
  check(report.changed &&
            result.object(report.shell).polygon->origin->deformation ==
                original.active().deformation,
        "Current posed/morphed colored cut retains complete editable artist "
        "deformation in its origin");
  check(inspect_mesh(world_surface(result)).closed_manifold(),
        "Reflected posed shell and independent cap remain geometrically closed "
        "in world space");
  const auto &cap = result.object(report.cap).polygon->topology;
  check(
      cap.vertices().size() < cap.triangulate().mesh.positions.size() &&
          std::any_of(cap.edges().begin(), cap.edges().end(),
                      [&](const auto &edge) {
                        size_t faces = 0;
                        for (const auto &face : cap.faces())
                          for (size_t c = 0; c < face.corners.size(); ++c)
                            if (std::minmax(
                                    face.corners[c].vertex,
                                    face.corners[(c + 1) % face.corners.size()]
                                        .vertex) == std::minmax(edge.a, edge.b))
                              ++faces;
                        return faces == 2;
                      }),
      "Generated cap topology shares real internal edges instead of "
      "disconnected display triangles");
  auto saved = result.encode();
  auto reopened = Scene::decode(saved);
  check(
      reopened.encode() == saved,
      "Posed current-field/source graph roundtrip preserves exact source data");
  restore_polygon_origin(reopened, report.shell);
  check(
      reopened.active().deformation->encode(
          reopened.active().painted->mesh()) ==
          original.active().deformation->encode(
              original.active().painted->mesh()),
      "Source restore recovers actual skin, pose and artist morph identities");
  for (int kind = 0; kind < 9; ++kind) {
    auto candidate = original.snapshot();
    auto forged = preview;
    if (kind == 0)
      forged.plane.offset += .001f;
    if (kind == 1)
      forged.plane.normal.y += .01f;
    if (kind == 2)
      forged.cap_mesh.positions[0].y += .001f;
    if (kind == 3)
      forged.cap_mesh.uv[0][0] += .001f;
    if (kind >= 4) {
      auto data = std::make_shared<PlaneCutResult>(*preview.result);
      if (kind == 4)
        data->mesh.positions[0].x += .001f;
      if (kind == 5)
        data->mesh.normals[0].y += .01f;
      if (kind == 6)
        data->source_triangles[0] = UINT32_MAX;
      if (kind == 7)
        data->source_corner_barycentrics[0] = {.5f, .5f, 0};
      if (kind == 8)
        data->mesh.uv_seams[0] ^= 1;
      forged.result = std::move(data);
    }
    refuses([&] { invoke(candidate, forged, source); },
            "Forged plane/cap/geometry/normal/corner metadata cannot acquire "
            "cut authority");
    source_exact(candidate, baseline, original);
  }
  for (int kind = 0; kind < 7; ++kind) {
    auto candidate = original.snapshot();
    auto bad = source;
    if (kind == 0)
      bad.colors.clear();
    if (kind == 1)
      bad.colors.back()[3] = .125f;
    if (kind == 2)
      bad.mesh.normals.back().x += .01f;
    if (kind == 3)
      bad.mesh.uv.back()[1] += .01f;
    if (kind == 4)
      bad.materials[0].material.roughness = .123f;
    if (kind == 5) {
      auto maps = std::make_shared<TextureMaps>(*bad.materials[0].maps);
      // Raw untextured captures have no dense derived color image. Forge the
      // actual immutable role-bundle owner, preserving its equal values.
      maps->source_maps = std::make_shared<const PaintSourceMaps>(*maps->source_maps);
      bad.materials[0].maps = maps;
    }
    if (kind == 6) {
      bad.mesh.positions.push_back(bad.mesh.positions[0]);
      bad.mesh.normals.push_back(bad.mesh.normals[0]);
      bad.mesh.uv.push_back(bad.mesh.uv[0]);
      bad.colors.push_back(bad.colors[0]);
    }
    refuses([&] { invoke(candidate, preview, bad); },
            "Missing or changed actual source attributes/material cannot pass "
            "by matching counts");
    source_exact(candidate, baseline, original);
  }
  for (int kind = 0; kind < 6; ++kind) {
    detail::CutColorLimits limits;
    if (kind == 0)
      limits.maximum_work = 1;
    if (kind == 1)
      limits.maximum_work = 128;
    if (kind == 2)
      limits.maximum_vertices = 1;
    if (kind == 3)
      limits.maximum_corners = 3;
    if (kind == 4)
      limits.maximum_faces = 12;
    if (kind == 5)
      limits.maximum_raster_samples = 1;
    auto candidate = original.snapshot();
    refuses([&] { invoke(candidate, preview, source, limits); },
            "Explicit preparation/component/raster quota exhaustion refuses "
            "without partial output");
    source_exact(candidate, baseline, original);
  }
  for (size_t at : {size_t(1), size_t(2), size_t(16), size_t(64), size_t(256),
                    size_t(1024)}) {
    auto candidate = original.snapshot();
    size_t calls = 0;
    refuses(
        [&] {
          detail::publish_colored_cut(candidate, preview, source, 64,
                                      [&] { return ++calls >= at; }, {});
        },
        "Actual preparation cancellation before and through geometry/field "
        "phases unwinds");
    source_exact(candidate, baseline, original);
  }
  for (float phase : {.05f, .2f, .9f, 1.f}) {
    auto candidate = original.snapshot();
    bool cancel = false;
    refuses(
        [&] {
          detail::publish_colored_cut(
              candidate, preview, source, 64, [&] { return cancel; },
              [&](float progress) {
                if (progress >= phase)
                  cancel = true;
              });
        },
        "Late progress-phase cancellation remains transactional through final "
        "staging");
    source_exact(candidate, baseline, original);
  }
}
void faults(size_t first = 1, size_t last = SIZE_MAX) {
  set_paint_composite_cache_budget(0);
  auto original = cut_color_fixture::scene(true, true);
  auto baseline = original.encode();
  auto source = cut_color_fixture::source(original);
  CutSettings settings;
  settings.offset_mm = 31;
  auto preview = preview_cut(source.mesh, settings);
  auto operation = [&](Scene &scene) {
    detail::publish_colored_cut(scene, preview, source, 64, {}, {});
  };
  auto warm = original.snapshot();
  operation(warm);
  const auto expected = warm.encode();
  auto measured_scene = original.snapshot();
  allocations = 0;
  boundary = SIZE_MAX;
  deny = true;
  operation(measured_scene);
  deny = false;
  const auto measured = allocations;
  check(measured > 100, "Real detached colored cut exposes meaningful owned "
                        "allocation boundaries");
  if (!first || first > measured)
    throw std::invalid_argument("Fault range is outside measured cut allocations");
  last = std::min(last, measured);
  std::cout << measured << " measured cut allocations; checking " << first << " through " << last << std::endl;
  for (size_t at = first; at <= last; ++at) {
    auto candidate = original.snapshot();
    allocations = 0;
    boundary = at;
    hit = false;
    deny = true;
    bool caught = false;
    try {
      operation(candidate);
    } catch (const std::bad_alloc &) {
      caught = true;
    }
    deny = false;
    if (!hit || !caught)
      throw std::runtime_error("Every measured cut allocation boundary must "
                               "actually fail and unwind");
    if (candidate.encode() != baseline ||
        candidate.active().polygon != original.active().polygon ||
        candidate.active().polygon->colors->source_owner() !=
            original.active().polygon->colors->source_owner() ||
        candidate.active().painted->mesh_owner() !=
            original.active().painted->mesh_owner() ||
        candidate.active().shape_source != original.active().shape_source ||
        original.encode() != baseline)
      throw std::runtime_error("Owned allocation refusal cannot replace any "
                               "complete source/counter or live owner");
    operation(candidate);
    if ((at & 255) == 0)
      std::cout << "Reached cut allocation boundary " << at << std::endl;
    if (candidate.encode() != expected)
      throw std::runtime_error(
          "Successful cut retry must reproduce the exact intended authority");
  }
  std::cout << (last-first+1)
            << " actual persistent colored-cut allocation boundaries preserve "
               "exact originals and retry\n";
}
} // namespace
[[gnu::noinline]] void *operator new(size_t n) {
  allocation_check();
  if (auto p = std::malloc(n ? n : 1))
    return p;
  throw std::bad_alloc();
}
[[gnu::noinline]] void *operator new[](size_t n) { return ::operator new(n); }
[[gnu::noinline]] void operator delete(void *p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete[](void *p) noexcept { std::free(p); }
[[gnu::noinline]] void operator delete(void *p, size_t) noexcept {
  std::free(p);
}
[[gnu::noinline]] void operator delete[](void *p, size_t) noexcept {
  std::free(p);
}
int main(int argc, char **argv) try {
  if (argc == 4 && std::string_view(argv[1]) == "--fault-range")
    faults(std::stoull(argv[2]), std::stoull(argv[3]));
  else if (argc == 2 && std::string_view(argv[1]) == "--faults")
    faults();
  else
    ordinary();
  std::cout << checks << " cut color boundary checks\n";
  return 0;
} catch (const std::exception &e) {
  deny = false;
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
