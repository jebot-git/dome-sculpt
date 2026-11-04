// SPDX-License-Identifier: GPL-3.0-or-later
// Explicit read-only related-geometry diagnostic; no public/default expansion.
#include "document-authority.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/rig-rest.hpp"
#include "dome/skin-authoring.hpp"
#include "dome/source-placement.hpp"
#include "geometry-weight-authoring.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
void check(bool v, const char *s) {
  if (!v)
    throw std::runtime_error(s);
  std::cout << "PASS " << s << std::endl;
}
int main(int argc, char **argv) try {
  if (argc != 3)
    throw std::invalid_argument("Choose completed full-field directory and "
                                "explicit partial-context output");
  const std::filesystem::path data = argv[1], out = argv[2];
  std::filesystem::create_directories(out);
  auto scene = Scene::load(data / "fitted-unskinned.dsculpt"),
       full = Scene::load(data / "bound.dsculpt");
  uint64_t target = 0;
  for (const auto &o : scene.objects())
    if (o.deformation && o.deformation->skeleton)
      target = o.id;
  check(
      target && scene.object(target).appearance &&
          scene.object(target).appearance->asset,
      "Actual fitted native reference and retained original source authority");
  const auto &ref = scene.object(target);
  check(ref.appearance->asset->skins.empty(),
        "Actual original input has no artist skin payload");
  for (const auto &p : ref.appearance->asset->primitives)
    check(p.influences.empty(),
          "Original primitive has no artist influence values");
  const auto &rig = *ref.deformation->skeleton;
  std::cout << "FAILED_CONTROL index=6 name=" << rig.bones.at(6).name
            << std::endl;
  if (ref.deformation->humanoid)
    for (const auto &[role, j] : ref.deformation->humanoid->joints)
      if (j == 6)
        std::cout << "FAILED_CONTROL role=" << role << std::endl;
  std::vector<uint64_t> context;
  for (const auto &o : scene.objects())
    if (o.mesh_active && o.painted && o.appearance) {
      check(o.appearance->asset == ref.appearance->asset &&
                !o.appearance->asset.owner_before(ref.appearance->asset) &&
                !ref.appearance->asset.owner_before(o.appearance->asset),
            "Every explicit context part shares authentic immutable original "
            "model owner");
      check(!o.deformation || !o.deformation->skin,
            "Every current context input remains unskinned");
      context.push_back(o.id);
      std::cout << "CONTEXT " << o.id << " group=" << o.group
                << " node=" << o.appearance->source_node;
      for (const auto &g : scene.group_path(o.group))
        std::cout << " /" << g.id << ":"
                  << (g.source_placement
                          ? std::to_string(g.source_placement->source_instance)
                          : "native");
      std::cout << std::endl;
    }
  check(context.size() == 19, "Exact complete genuine19-part related geometry, "
                              "explicitly declared for this diagnostic");
  const auto before = scene.encode();
  const auto protected_before = scene;
  const auto paint = scene.object(target).painted;
  SkinAuthoringOptions prior;
  prior.mode = SkinAuthoringMode::NativeVisible;
  prior.reference = target;
  prior.native_visible.contact_handling =
      NativeContactHandling::FiniteLineIntegral;
  bool failed = false;
  std::string error;
  try {
    author_native_visible_skin(scene, std::array<uint64_t, 1>{target}, prior);
  } catch (const std::runtime_error &e) {
    error = e.what();
    failed = error.find("whole-facet dominance bone=6") != std::string::npos;
  }
  check(failed && scene.encode() == before,
        "Original ordinary partial-target failure remains exact; no fallback "
        "or guard exemption");
  const size_t wrapper_work = 256 + context.size() * 16 + rig.bones.size() * 4;
  const size_t wrapper_bytes =
      sizeof(detail::DeclaredWeightSupport) +
      sizeof(detail::GeometryWeightBindOptions) +
      context.size() * sizeof(detail::DeclaredWeightPart) + 128;
  detail::DeclaredWeightSupport support;
  support.policy = detail::DeclaredSupportPolicy::UniqueSourceWitnesses;
  support.world_metric = true;
  support.reference = target;
  support.rig_geometry = detail::DeclaredRigGeometry::NativeBoneAxes;
  support.row_anchors = detail::DeclaredRowAnchors::NearestPermitted;
  support.source_requirements =
      detail::DeclaredSourceRequirements::GeometryEvidence;
  support.source_span = detail::DeclaredSourceSpan::AuthoredFinite;
  support.origin_boundary =
      detail::DeclaredOriginBoundary::NativeFiniteLineIntegral;
  support.parts.reserve(context.size());
  for (auto id : context)
    support.parts.push_back({id, {}});
  detail::GeometryWeightBindOptions options;
  options.limits = prior.limits;
  options.maximum_work = 500000000 - wrapper_work;
  options.maximum_workspace = 256u * 1024u * 1024u - wrapper_bytes;
  detail::GeometryWeightBindReport report;
  const auto start = std::chrono::steady_clock::now();
  check(detail::author_geometry_weight_skin(scene,
                                            std::array<uint64_t, 1>{target},
                                            support, options, {}, &report),
        "Actual owning factory computes whole explicit context and emits ONLY "
        "requested target");
  const double elapsed =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
          .count();
  detail::require_isolated_scene(protected_before, scene, target);
  check(scene.object(target).painted == paint,
        "All non-output object/group/COW owners and current target paint stay "
        "exact");
  check(scene.object(target).deformation->encode(
            scene.object(target).painted->mesh()) ==
            full.object(target).deformation->encode(
                full.object(target).painted->mesh()),
        "Partial output field/skeleton/morph/pose exactly equals the "
        "independently qualified full-context baseline");
  check(report.changed == 1 &&
            report.field.rows >
                scene.object(target).painted->mesh().positions.size() &&
            report.work + wrapper_work <= 500000000 &&
            report.workspace + wrapper_bytes <= 256u * 1024u * 1024u,
        "Actual readonly context charged under original500M/256MiB; exactly "
        "one changed recipient");
  scene.save(out / "bound.dsculpt");
  check(Scene::load(out / "bound.dsculpt").encode() == scene.encode(),
        "Actual partial field and complete first source restart exactly");
  auto restored = Scene::load(out / "bound.dsculpt");
  restore_rig_source(restored, target, false);
  check(restored.encode() == before,
        "Partial source Restore returns exact whole pre-bind Scene including "
        "all readonly context");
  std::ofstream(out / "report.json")
      << nlohmann::json{{"target", target},
                        {"context", context},
                        {"failed_control", rig.bones.at(6).name},
                        {"prior_error", error},
                        {"work", report.work + wrapper_work},
                        {"workspace", report.workspace + wrapper_bytes},
                        {"seconds", elapsed},
                        {"field_rows", report.field.rows},
                        {"changed", report.changed},
                        {"field_equal_full_baseline", true},
                        {"protected_context_exact", true},
                        {"save_restore_exact", true},
                        {"scope",
                         "explicit test-only related context, not automatic "
                         "context discovery or public/default admission"}}
             .dump(2);
  std::cout << "PASS PARTIAL_CONTEXT work=" << report.work + wrapper_work
            << " bytes=" << report.workspace + wrapper_bytes
            << " seconds=" << elapsed << std::endl;
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << std::endl;
  return 1;
}
