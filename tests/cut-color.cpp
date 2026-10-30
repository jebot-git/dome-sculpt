// SPDX-License-Identifier: GPL-3.0-or-later
#include "cut-color.hpp"
#include "cut-color-fixture.hpp"
#include "dome/document.hpp"
#include <iostream>
using namespace dome;
namespace {
size_t checks = 0;
void check(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
  ++checks;
  std::cout << "PASS " << message << std::endl;
}
template <class F> void rejects(F operation, const char *message) {
  bool caught = false;
  try {
    operation();
  } catch (const std::exception &) {
    caught = true;
  }
  check(caught, message);
}
} // namespace
int main(int argc, char **argv) try {
  if (argc == 3 && std::string_view(argv[1]) == "--ui-fixture") {
    cut_color_fixture::scene().save(argv[2]);
    return 0;
  }
  for (bool affine : {false, true})
    for (bool seam : {false, true}) {
      auto scene = cut_color_fixture::scene(seam, affine);
      const auto baseline = scene.encode();
      const auto original = scene.active();
      const auto source_owner = original.polygon->colors->source_owner();
      auto source = cut_color_fixture::source(scene);
      CutSettings settings;
      settings.offset_mm = 31;
      auto preview = preview_cut(source.mesh, settings);
      auto before = scene.snapshot();
      auto report =
          detail::publish_colored_cut(scene, preview, source, 64, {}, {});
      check(report.changed && report.shell && report.cap && report.projected,
            "Actual raw COLOR_0 shell/cap cut publishes from its immutable "
            "clipping source");
      const auto &shell = scene.object(report.shell),
                 &cap = scene.object(report.cap);
      check(shell.polygon && shell.polygon->colors &&
                shell.polygon->colors->source_owner() == source_owner,
            "Clipped CORNER colors share the exact original accessor basis "
            "rather than a raster or replacement samples");
      check(shell.polygon->origin &&
                shell.polygon->origin == cap.polygon->origin &&
                shell.polygon->origin->polygon == original.polygon &&
                shell.polygon->origin->shape_source == original.shape_source,
            "Shell/cap share one complete source capsule retaining prior "
            "polygon and durable shape source");
      check(scene.object(original.id).painted->encode() ==
                    before.active().painted->encode() &&
                !scene.object(original.id).visible,
            "Original editable paint, geometry and raw source remain exact and "
            "hidden");
      check(cap.material.color == settings.cap_material.color &&
                !cap.polygon->colors,
            "The generated cap retains independent material and has no "
            "fabricated original RGBA corners");
      check(shell.material.color == original.material.color &&
                shell.painted->baked() &&
                shell.painted->baked()->color_interpretation ==
                    BakedColorInterpretation::VertexComponent,
            "Only common paint/material components are baked; the current "
            "color field remains independent");
      check(scene.encode().at("version") == 16 &&
                scene.encode().at("polygon_sources").at("version") == 4,
            "Existing archive4 persists actual color, shape and placement "
            "authorities without a new schema");
      auto reopened = Scene::decode(scene.encode());
      check(reopened.encode() == scene.encode(),
            "Native restart preserves exact clipped geometry, current field, "
            "cap and source graph");
      auto restored = reopened;
      restore_polygon_origin(restored, report.shell);
      check(restored.active().painted->encode() == original.painted->encode() &&
                restored.active().polygon->topology.encode() ==
                    original.polygon->topology.encode() &&
                restored.world_matrix(report.shell).matrix() ==
                    before.world_matrix(original.id).matrix(),
            "Explicit source restore retains exact original content at the "
            "original effective affine placement");
      const auto &target = shell.polygon->topology;
      auto target_mesh = target.triangulate().mesh;
      detail::place_mesh(target_mesh,
                         detail::placement_frame(scene, report.shell));
      auto target_colors = shell.polygon->colors->packed(target);
      float maximum = 0;
      for (size_t t = 0, side = 0; t < preview.result->source_triangles.size();
           ++t) {
        auto source_triangle = preview.result->source_triangles[t];
        if (source_triangle == UINT32_MAX)
          continue;
        for (size_t k = 0; k < 3; ++k) {
          auto original_corner = t * 3 + k;
          size_t corner = side * 3 + k;
          if (affine &&
              detail::placement_frame(scene, report.shell).orientation() ==
                  AffineOrientation::Reversing &&
              k)
            corner = side * 3 + (k == 1 ? 2 : 1);
          auto vertex = target_mesh.indices.at(corner);
          auto p = preview.result->mesh.positions.at(
              preview.result->mesh.indices.at(original_corner));
          maximum =
              std::max(maximum, length(target_mesh.positions.at(vertex) - p));
          auto bary =
              preview.result->source_corner_barycentrics.at(original_corner);
          double sum = double(bary[0]) + bary[1] + bary[2];
          for (size_t c = 0; c < 4; ++c) {
            double expected = 0;
            for (size_t j = 0; j < 3; ++j)
              expected += source.colors.at(source.mesh.indices.at(
                              size_t(source_triangle) * 3 + j))[c] *
                          double(bary[j]) / sum;
            maximum = std::max(
                maximum, float(std::abs(double(target_colors.at(vertex)[c]) -
                                        expected)));
          }
        }
        ++side;
      }
      check(maximum < 3e-7f,
            "Independent barycentric oracle preserves straight linear RGBA, "
            "alpha seams and actual world geometry");
      auto tampered = source;
      tampered.colors[0][0] = .99f;
      auto unchanged = before.snapshot();
      rejects(
          [&] {
            detail::publish_colored_cut(unchanged, preview, tampered, 64, {},
                                        {});
          },
          "Wrong actual source colors are refused despite matching "
          "geometry and counts");
      check(unchanged.encode() == baseline,
            "Refused source authentication leaves every original source "
            "owner/data unchanged");
      tampered = source;
      tampered.mesh.positions[0].x += .001f;
      rejects(
          [&] {
            detail::publish_colored_cut(unchanged, preview, tampered, 64, {},
                                        {});
          },
          "Wrong ordered source geometry is refused before field transfer");
      auto forged = preview;
      auto altered = std::make_shared<PlaneCutResult>(*preview.result);
      altered->source_corner_barycentrics[0] = {.5f, .5f, 0};
      forged.result = altered;
      rejects(
          [&] {
            detail::publish_colored_cut(unchanged, forged, source, 64, {}, {});
          },
          "Forged clipping correspondence cannot be relabelled as "
          "authenticated corner authority");
      bool cancel = false;
      rejects(
          [&] {
            detail::publish_colored_cut(
                unchanged, preview, source, 64, [&] { return cancel; },
                [&](float p) {
                  if (p > .3f)
                    cancel = true;
                });
          },
          "Cancellation during common-component bake rejects the entire "
          "detached cut");
      check(unchanged.encode() == baseline,
            "Cancelled cut preserves exact original paint, field, shapes and "
            "hierarchy");
      rejects(
          [&] {
            detail::publish_colored_cut(
                unchanged, preview, source, 64, {}, [](float p) {
                  if (p == 1)
                    throw std::runtime_error("late callback");
                });
          },
          "A late fallible progress callback cannot commit a partial scene");
      check(unchanged.encode() == baseline,
            "Even the final callback refusal preserves exact pre-cut scene "
            "authority");
    }
  std::cout << checks << " actual colored plane-cut checks\n";
  return 0;
} catch (const std::exception &error) {
  std::cerr << "FAIL " << error.what() << '\n';
  return 1;
}
