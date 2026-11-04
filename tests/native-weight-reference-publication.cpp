// SPDX-License-Identifier: GPL-3.0-or-later
#include "geometry-weight-authoring.hpp"
#define main original_reference_main
#include "geometry-weight-reference.cpp"
#undef main
int main() try {
  auto scene = fixture();
  const auto original = scene.snapshot();
  detail::DeclaredWeightSupport support{
      detail::DeclaredSupportPolicy::UniqueSourceWitnesses, true, {{2, {}}}, 1};
  support.row_anchors = detail::DeclaredRowAnchors::NearestPermitted;
  support.source_requirements =
      detail::DeclaredSourceRequirements::GeometryEvidence;
  support.source_span = detail::DeclaredSourceSpan::AuthoredFinite;
  detail::GeometryWeightBindOptions options;
  auto owner = scene.object(1).deformation;
  const auto baseline = owner.use_count();
  bool captured = false, changed = false, accepted = false, refused = false;
  try {
    accepted = detail::author_geometry_weight_skin(
        scene, std::array<uint64_t, 1>{2}, support, options, [&] {
          const auto current = owner.use_count();
          if (current >= baseline + 3)
            captured = true;
          if (captured && !changed && current <= baseline + 1) {
            auto paint = scene.object(1).painted->snapshot();
            PaintSettings settings;
            settings.color = {.8f, .1f, .4f, .7f};
            Brush brush;
            brush.from = brush.to = paint->mesh().positions[0];
            brush.normal = paint->mesh().normals[0];
            brush.radius = .4f;
            paint->begin_stroke();
            paint->stamp(brush, settings);
            if (!paint->end_stroke())
              throw std::runtime_error("Test source paint failed to change");
            scene.object(1).painted = std::move(paint);
            changed = true;
          }
          return false;
        });
  } catch (const std::runtime_error &e) {
    refused = true;
    std::cout << "REFUSAL " << e.what() << '\n';
  }
  auto expected = original.snapshot();
  expected.object(1).painted = scene.object(1).painted;
  const bool exact = scene.encode() == expected.encode();
  std::cout << "LATE_REFERENCE captured=" << captured << " changed=" << changed
            << " accepted=" << accepted << " refused=" << refused
            << " target_skin=" << bool(scene.object(2).deformation->skin)
            << " expected_only_source_edit=" << exact << '\n';
  if (!captured || !changed || !refused || accepted || !exact)
    throw std::runtime_error(
        "Late changed source paint owner authorized stale target publication");
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
