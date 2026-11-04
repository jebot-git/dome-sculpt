// SPDX-License-Identifier: GPL-3.0-or-later
#include "geometry-weight-authoring.hpp"
#include <iostream>
using namespace dome;
int main() try {
  size_t checks = 0;
  auto check = [&](bool v, const char *s) {
    ++checks;
    if (!v)
      throw std::runtime_error(s);
  };
  const std::function<bool()> never = [] { return false; };
  detail::weight_surface::Budget work{0, 50000000, never};
  Mesh m;
  m.positions = {{-.22f, 0, 0}, {-.18f, 0, 0}, {-.18f, 1, 0}, {-.22f, 1, 0}};
  m.normals.assign(4, {0, 0, 1});
  m.uv = {{{0, 0}}, {{1, 0}}, {{1, 1}}, {{0, 1}}};
  m.indices = {0, 1, 2, 0, 2, 3};
  const std::vector<detail::HeatSourceSegment> sources{
      {{-.2f, 0, -.05f}, {-.2f, 1, -.05f}, 0},
      {{.2f, 0, -.05f}, {.2f, 1, -.05f}, 1},
      {{0, -2, -.05f}, {0, -1, -.05f}, 2}};
  const std::vector<detail::HeatJointMask> masks(4, {1, 0, 0, 0});
  check(
      detail::certify_unused_native_source(sources[1], sources, m, masks, work),
      "Opposite unselected arm is not certified genuinely unused");
  check(
      detail::certify_unused_native_source(sources[2], sources, m, masks, work),
      "Distant permitted leg is not certified genuinely unused");
  // Actual native partial part and full permitted skeleton; no artist skin.
  Scene scene;
  scene.active().painted = std::make_shared<PaintedMesh>(m, 64);
  scene.active().mesh_active = true;
  DeformationState d(m);
  d.skeleton =
      Skeleton{{{"native selected arm", -1, {{-.2f, 0, -.05f}}, 1, true},
                {"native other arm", -1, {{.2f, 0, -.05f}}, 1, true},
                {"native leg", -1, {{0, -2, -.05f}}, 1, true}}};
  scene.set_deformation(1, d);
  auto before = scene.encode();
  detail::DeclaredWeightSupport domain{
      detail::DeclaredSupportPolicy::UniqueSourceWitnesses, true, {{1, {}}}, 1};
  domain.row_anchors = detail::DeclaredRowAnchors::NearestPermitted;
  auto old = domain;
  bool refused = false;
  try {
    (void)detail::anatomical_continuation(
        scene, 0, {}, detail::BoneWallDomain::ConnectedSourceSurface,
        {50000000, 64u * 1024u * 1024u}, &old);
  } catch (const std::runtime_error &e) {
    refused = std::string(e.what()).find("no declared group support") !=
              std::string::npos;
  }
  check(refused && scene.encode() == before,
        "Stronger required-all control was silently weakened");
  domain.source_requirements =
      detail::DeclaredSourceRequirements::GeometryEvidence;
  domain.source_span = detail::DeclaredSourceSpan::AuthoredFinite;
  const auto f = detail::anatomical_continuation(
      scene, 0, {}, detail::BoneWallDomain::ConnectedSourceSurface,
      {50000000, 64u * 1024u * 1024u}, &domain);
  check(
      f.joints == std::vector<uint32_t>{0, 1, 2} &&
          f.report.supported_controls == 1,
      "Permitted joints were removed instead of retaining unused zero fields");
  for (size_t raw = 0; raw < 4; ++raw) {
    auto row = f.parts[0].raw_to_row[raw];
    check(std::abs(f.fields[row * 3] - 1) < 1e-9,
          "Selected arm lost owning weight");
    check(std::bit_cast<uint64_t>(f.fields[row * 3 + 1]) == 0 &&
              std::bit_cast<uint64_t>(f.fields[row * 3 + 2]) == 0,
          "Unused genuine bones gained nonzero fields");
  }
  detail::GeometryWeightBindOptions options;
  auto bound = scene.snapshot();
  check(detail::author_geometry_weight_skin(bound, std::array<uint64_t, 1>{1},
                                            domain, options),
        "Real native partial-part producer still refuses full rig");
  check(bound.active().deformation->skeleton->encode() ==
                d.skeleton->encode() &&
            bound.active().rig_source,
        "Full native skeleton or first source capsule was lost");
  for (const auto &row : bound.active().deformation->skin->vertices)
    check(row.size() == 1 && row[0].joint == 0 && row[0].weight == 1,
          "Producer does not emit the certified partial-part field");
  // A genuine source closest only in a triangle interior must NOT be declared
  // unused merely because no raw vertex observes its finite Voronoi cell.
  Mesh triangle;
  triangle.positions = {{-1, -1, 0}, {1, -1, 0}, {0, 1, 0}};
  triangle.normals.assign(3, {0, 0, 1});
  triangle.uv = {{{0, 0}}, {{1, 0}}, {{.5, 1}}};
  triangle.indices = {0, 1, 2};
  std::vector<detail::HeatSourceSegment> interior{
      {{-1, -1, -.01f}, {-1, -.95f, -.01f}, 0},
      {{1, -1, -.01f}, {1, -.95f, -.01f}, 1},
      {{0, 1, -.01f}, {0, 1.05f, -.01f}, 2},
      {{0, 0, -.01f}, {0, .05f, -.01f}, 3}};
  const std::vector<detail::HeatJointMask> vertex_masks(3, {7, 0, 0, 0});
  check(!detail::certify_unused_native_source(interior[3], interior, triangle,
                                              vertex_masks, work),
        "Interior-only control was silently classified unused");
  detail::NearestWeightSources nearest(interior, work);
  for (size_t i = 0; i < 3; ++i) {
    const auto p = triangle.positions[i];
    check(nearest.nearest({p.x, p.y, p.z}, work)[0] == (uint64_t(1) << i),
          "Interior control negative does not actually evade vertex witnesses");
  }
  check(nearest.nearest({0, 0, 0}, work)[0] == 8,
        "Interior-only control has no genuine nearer interior point");
  Scene unsupported;
  unsupported.active().painted = std::make_shared<PaintedMesh>(triangle, 64);
  unsupported.active().mesh_active = true;
  d = DeformationState(triangle);
  d.skeleton = Skeleton{};
  for (const auto &s : interior)
    d.skeleton->bones.push_back(
        {"native finite " + std::to_string(s.bone),
         -1,
         {{float(s.head[0]), float(s.head[1]), float(s.head[2])}},
         .05f,
         true});
  unsupported.set_deformation(1, d);
  const auto input = unsupported.encode();
  auto clipped = domain;
  clipped.source_span = detail::DeclaredSourceSpan::ComponentProjection;
  refused = false;
  try {
    (void)detail::anatomical_continuation(
        unsupported, 0, {}, detail::BoneWallDomain::ConnectedSourceSurface,
        {50000000, 64u * 1024u * 1024u}, &clipped);
  } catch (const std::runtime_error &e) {
    refused = std::string(e.what()).find("no declared group support bone=2") !=
              std::string::npos;
  }
  check(refused && unsupported.encode() == input,
        "Legacy projection-span endpoint counterexample was not preserved");
  refused = false;
  try {
    (void)detail::anatomical_continuation(
        unsupported, 0, {}, detail::BoneWallDomain::ConnectedSourceSurface,
        {50000000, 64u * 1024u * 1024u}, &domain);
  } catch (const std::runtime_error &e) {
    std::cout << "INTERIOR_CONTROL_REFUSAL " << e.what() << std::endl;
    refused = std::string(e.what()).find("whole-facet dominance bone=3") !=
              std::string::npos;
  }
  check(refused && unsupported.encode() == input,
        "Unresolved continuous facet support was discarded or mutated caller");
  detail::weight_surface::Budget little{0, 1, never};
  refused = false;
  try {
    (void)detail::certify_unused_native_source(sources[1], sources, m, masks,
                                               little);
  } catch (const std::length_error &) {
    refused = true;
  }
  check(refused, "Dominance proof work accounting is bypassed");
  const std::function<bool()> stop = [] { return true; };
  detail::weight_surface::Budget cancelled{0, 50000000, stop};
  refused = false;
  try {
    (void)detail::certify_unused_native_source(sources[1], sources, m, masks,
                                               cancelled);
  } catch (const std::runtime_error &) {
    refused = true;
  }
  check(refused, "Dominance proof cancellation is bypassed");
  std::cout << "PASS permitted_controls_checks=" << checks
            << " field_work=" << f.report.work << " proof_work=" << work.used
            << '\n';
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
