// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/creation.hpp"
#include "dome/polygon-scene.hpp"
#include "dome/skin-authoring.hpp"
#include "source-visible-pose.hpp"
#include <chrono>
#include <nlohmann/json.hpp>
#include <iomanip>
#include <iostream>
using namespace dome;
using namespace source_visible_test;
int main(int argc, char **argv) try {
  const std::string gender = argc > 1 ? argv[1] : "female";
  if (gender != "female" && gender != "male")
    throw std::invalid_argument("Unknown actual cohort");
  Scene scene;
  append_humanoid(scene,
                  gender == "male"
                      ? humanoid_baseline_defaults("vroid-beta-male")
                      : HumanoidControls{},
                  "Source-visible native", {}, 64);
  const auto input = scene.encode();
  const auto original = scene.objects();
  std::vector<uint64_t> targets;
  double low = INFINITY, high = -INFINITY;
  for (const auto &o : original)
    if (o.painted) {
      targets.push_back(o.id);
      for (auto p : o.painted->mesh().positions) {
        low = std::min(low, double(p.y));
        high = std::max(high, double(p.y));
      }
    }
  if (targets.size() != 19)
    throw std::runtime_error("Expected genuine19part creator");
  SkinAuthoringOptions options;
  options.reference = targets.front();
  options.mode = SkinAuthoringMode::SourceVisible;
  SourceVisibleSkinReport report;
  const auto start = std::chrono::steady_clock::now();
  if (!author_source_visible_skin(scene, targets, options, {}, &report))
    throw std::runtime_error("Genuine group bind changed nothing");
  const double elapsed =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
          .count();
  if (report.work > 3'000'000'000ULL ||
      report.planned_workspace > 256u * 1024u * 1024u ||
      report.group_parts != 19 || !report.changed_parts)
    throw std::runtime_error(
        "Native source-visible work/workspace/target report");
  size_t poses = 0, failures = 0;
  for (const auto &before : original)
    if (before.painted) {
      const auto &after = scene.object(before.id);
      if (before.painted != after.painted || before.volume != after.volume ||
          before.appearance != after.appearance ||
          before.material_bindings != after.material_bindings ||
          before.shape_source != after.shape_source ||
          before.polygon != after.polygon ||
          before.affine_edit != after.affine_edit)
        throw std::runtime_error("Native bind changed original "
                                 "geometry/paint/material/source owners");
      auto restored = *after.deformation;
      restored.skin = before.deformation->skin;
      if (restored.encode(before.painted->mesh()) !=
          before.deformation->encode(before.painted->mesh()))
        throw std::runtime_error(
            "Native bind changed nonweight authoring data");
      if (after.deformation != before.deformation &&
          (!after.rig_source ||
           after.rig_source->deformation != before.deformation ||
           after.rig_source->paint->mesh_owner() !=
               before.painted->mesh_owner()))
        throw std::runtime_error(
            "Native bind omitted exact first prebind source");
      for (const auto &row : after.deformation->skin->vertices)
        if (row.size() > 4)
          throw std::runtime_error("Source-assisted fixed4 preset");
      for (int p = 0; p < 5; ++p) {
        auto a = *before.deformation, b = *after.deformation;
        pose(a, p);
        pose(b, p);
        const auto expected = points(before.painted->mesh(), a),
                   current = points(after.painted->mesh(), b);
        double mean = 0, maximum = 0;
        for (size_t i = 0; i < expected.size(); ++i) {
          const double e = distance(expected[i], current[i]);
          mean += e;
          maximum = std::max(maximum, e);
        }
        mean /= expected.size();
        ++poses;
        const bool good =
            mean <= (high - low) * .005 && maximum <= (high - low) * .02;
        failures += !good;
        std::cout << std::setprecision(12) << "POSE object=" << before.id
                  << " pose=" << p << " mean=" << mean << " max=" << maximum
                  << " pass=" << good << '\n';
      }
    }
  const auto accepted = scene.encode();
  SourceVisibleSkinReport sentinel{7, 8, 9, 10, 11, 12};
  bool caught = false;
  try {
    (void)author_source_visible_skin(
        scene, targets, options, [] { return true; }, &sentinel);
  } catch (const std::runtime_error &) {
    caught = true;
  }
  if (!caught || scene.encode() != accepted || sentinel.work != 7 ||
      sentinel.controls != 12)
    throw std::runtime_error("Cancel changed native source/report");
  options.locks.assign(
      scene.object(targets.front()).deformation->skeleton->bones.size(), 1);
  if (author_source_visible_skin(scene, targets, options) ||
      scene.encode() != accepted)
    throw std::runtime_error("Alllocked native rebind must be exact no-op");
  std::cout << "SUMMARY gender=" << gender << " parts=19 poses=" << poses
            << " failures=" << failures << " work=" << report.work
            << " workspace=" << report.planned_workspace
            << " elapsed=" << elapsed
            << " source_retained=1 cancel=1 all_locked_noop=1\n";
  return failures ? 1 : 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 2;
}
