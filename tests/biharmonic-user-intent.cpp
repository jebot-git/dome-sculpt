// SPDX-License-Identifier: GPL-3.0-or-later
#include "biharmonic-binding.hpp"
#include "heat-component-intent.hpp"
#include <iostream>
using namespace dome;
using namespace dome::detail;
int main() try {
  Mesh mesh;
  mesh.positions = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
                    {2, 0, 0}, {3, 0, 0}, {3, 1, 0}, {2, 1, 0}};
  mesh.normals.assign(8, {0, 0, 1});
  mesh.uv = {{.05, .05}, {.45, .05}, {.45, .45}, {.05, .45},
             {.55, .05}, {.95, .05}, {.95, .45}, {.55, .45}};
  mesh.indices = {0, 1, 2, 0, 2, 3, 4, 5, 6, 4, 6, 7};
  SculptObject object;
  object.id = 23;
  object.painted = std::make_shared<PaintedMesh>(mesh, 64);
  object.mesh_active = true;
  auto deform = std::make_shared<DeformationState>(mesh);
  deform->skeleton = Skeleton{};
  deform->skeleton->bones.resize(2);
  deform->skeleton->bones[0].name = "User landmark A";
  deform->skeleton->bones[1].name = "User landmark B";
  object.deformation = deform;
  const std::array<HeatComponentDeclaration, 2> declarations{
      {{0, {0}}, {4, {1}}}};
  HeatBindingOptions options;
  auto intent = capture_user_component_intent(object, declarations, options);
  if (intent.kind() != HeatComponentIntentKind::UserDeclared ||
      intent.components() != 2 || object.deformation->skin)
    throw std::runtime_error(
        "Unskinned explicit component intent was not retained");
  auto masks = intent.masks(object);
  for (size_t i = 0; i < 8; ++i)
    if (masks[i] != HeatJointMask{uint64_t(1) << (i < 4 ? 0 : 1), 0, 0, 0})
      throw std::runtime_error(
          "User component assignment changed inside topology");
  BiharmonicBindingOptions o;
  o.projection = BiharmonicProjection::IndependentComponent;
  o.source.distance_rule = HeatDistanceRule::EuclideanSurface;
  o.source.seed_rule = HeatSeedRule::ReferenceSegments;
  o.source.visibility = HeatVisibility::EntryFaces;
  o.source.occlusion = HeatOcclusion::ConnectedSurface;
  o.source.maximum_seed_distance = 5;
  const std::array<HeatSourceSegment, 2> sources{
      {{{.5, 0, -.1}, {.5, 1, -.1}, 0}, {{2.5, 0, -.1}, {2.5, 1, -.1}, 1}}};
  BiharmonicBindingReport report;
  auto result = bounded_biharmonic_surface(object.painted->mesh(),
                                           *object.deformation->skeleton, o,
                                           sources, {}, {}, {}, &report, masks);
  for (size_t i = 0; i < 8; ++i)
    if (result.vertices[i] != VertexWeights{{uint32_t(i < 4 ? 0 : 1), 1}})
      throw std::runtime_error(
          "Explicit unskinned components leaked source influence");
  auto foreign = object;
  foreign.deformation = std::make_shared<DeformationState>(*object.deformation);
  bool refused = false;
  try {
    intent.masks(foreign);
  } catch (const std::invalid_argument &) {
    refused = true;
  }
  if (!refused)
    throw std::runtime_error("Foreign equal-bytes rig owner admitted");
  std::vector<HeatJointMask> forged(masks.begin(), masks.end());
  forged[1][0] = 2;
  BiharmonicBindingReport sentinel;
  sentinel.work = 173;
  refused = false;
  try {
    bounded_biharmonic_surface(mesh, *deform->skeleton, o, sources, {}, {}, {},
                               &sentinel, forged);
  } catch (const std::invalid_argument &) {
    refused = true;
  }
  if (!refused || sentinel.work != 173)
    throw std::runtime_error("Per-vertex fabricated intent admitted");
  refused = false;
  try {
    capture_user_component_intent(object, std::span(declarations).first(1),
                                  options);
  } catch (const std::invalid_argument &) {
    refused = true;
  }
  if (!refused)
    throw std::runtime_error("Undeclared component was silently filled");
  std::cout << "PASS unskinned user-landmark2components, independent visible "
               "Dirichlet, exact rigid fields, "
               "foreign-owner/per-vertex/omitted-component negatives; kkt="
            << report.maximum_kkt_residual << '\n';
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
