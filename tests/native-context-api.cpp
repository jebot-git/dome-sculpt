// SPDX-License-Identifier: GPL-3.0-or-later
// Explicit read-only related-geometry diagnostic; no public/default expansion.
#include "document-authority.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/rig-rest.hpp"
#include "dome/skin-authoring.hpp"
#include "dome/source-placement.hpp"
#include "native-weight-context.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
void check(bool v, const char *s) {
  if (!v)
    throw std::runtime_error(s);
  std::cout << "PASS " << s << std::endl;
}
std::vector<uint64_t> siblings(const Scene &scene, uint64_t reference) {
  const auto result =
      detail::native_node_context(scene, reference, std::span(&reference, 1));
  return {result.ids.begin(), result.ids.begin() + result.count};
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
  auto context = siblings(scene, target);
  check(
      context.size() == 9,
      "Complete unique nine current siblings of the chosen original mesh node");
  auto reject = [&](Scene bad) {
    bool no = false;
    try {
      (void)siblings(bad, target);
    } catch (const std::exception &) {
      no = true;
    }
    check(no, "Ambiguous/incomplete/foreign current primitive context refuses");
  };
  {
    auto bad = scene;
    bad.object(context.back()).group = 0;
    reject(std::move(bad));
  }
  {
    auto bad = scene;
    const auto id = bad.add("Duplicate source-node primitive", Volume{});
    auto &duplicate = bad.object(id);
    duplicate = scene.object(context.back());
    duplicate.id = id;
    reject(std::move(bad));
  }
  {
    auto bad = scene;
    auto app = std::make_shared<CreatorAppearance>(
        *bad.object(context.back()).appearance);
    app->asset = std::make_shared<const ImportedAsset>(*app->asset);
    bad.object(context.back()).appearance = std::move(app);
    reject(std::move(bad));
  }
  {
    auto bad = scene;
    auto app = std::make_shared<CreatorAppearance>(
        *bad.object(context.back()).appearance);
    app->source_primitive = 1000000;
    bad.object(context.back()).appearance = std::move(app);
    reject(std::move(bad));
  }
  {
    auto extra = scene;
    const auto group = extra.add_group("A different actual native collection");
    const auto id = extra.add("Same-asset foreign instance", Volume{});
    auto &foreign = extra.object(id);
    foreign = scene.object(context.back());
    foreign.id = id;
    foreign.group = group;
    check(
        siblings(extra, target) == context,
        "Same asset/node primitive in a different actual group is not joined");
  }
  for (auto id : context)
    std::cout << "CONTEXT " << id
              << " primitive=" << scene.object(id).appearance->source_primitive
              << std::endl;
  // Independently chosen public nine-output baseline. It is never the input
  // to the separately computed one-output field below.
  full = scene.snapshot();
  SkinAuthoringOptions baseline_options;
  baseline_options.mode = SkinAuthoringMode::NativeVisible;
  baseline_options.reference = target;
  baseline_options.native_visible.contact_handling =
      NativeContactHandling::FiniteLineIntegral;
  check(author_native_visible_skin(full, context, baseline_options),
        "Actual public original node-wide output supplies independent exact "
        "baseline");
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
  prior.native_visible.support_context =
      NativeSupportContext::CompleteReferenceNode;
  NativeVisibleSkinReport report;
  const auto start = std::chrono::steady_clock::now();
  check(author_native_visible_skin(scene, std::array<uint64_t, 1>{target},
                                   prior, {}, &report),
        "Actual public preselected context emits ONLY the requested target");
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
  check(report.changed_parts == 1 &&
            report.rows >
                scene.object(target).painted->mesh().positions.size() &&
            report.work <= 500000000 &&
            report.planned_workspace <= 256u * 1024u * 1024u,
        "Actual readonly context charged under original500M/256MiB; exactly "
        "one changed recipient");
  for (int kind = 0; kind < 3; ++kind) {
    auto probe = protected_before.snapshot();
    const auto exact = probe.encode();
    auto limited = prior;
    if (kind == 0)
      limited.native_visible.maximum_work = 1;
    if (kind == 1)
      limited.native_visible.maximum_workspace = 4096;
    if (kind == 2)
      limited.native_visible.support_context = NativeSupportContext(2);
    bool rejected = false;
    try {
      (void)author_native_visible_skin(probe, std::span(&target, 1), limited);
    } catch (const std::exception &) {
      rejected = true;
    }
    check(rejected && probe.encode() == exact,
          "Unknown context or insufficient charged staging budget refuses "
          "atomically");
  }
  for (int fault = 0; fault < 9; ++fault) {
    auto probe = protected_before.snapshot();
    bool fired = false, rejected = false;
    std::optional<Scene> expected_external;
    bool returned = false;
    NativeVisibleSkinReport sentinel{123};
    try {
      (void)author_native_visible_skin(
          probe, std::span(&target, 1), prior,
          [&] {
            if (fired)
              return false;
            fired = true;
            auto &donor = probe.object(context.back());
            switch (fault) {
            case 0:
              donor.visible = !donor.visible;
              break;
            case 1:
              donor.material.color[0] += .01f;
              break;
            case 2:
              donor.painted = donor.painted->snapshot();
              break;
            case 3:
              donor.volume =
                  std::make_shared<Volume>(donor.volume->geometry_snapshot());
              break;
            case 4:
              donor.appearance =
                  std::make_shared<const CreatorAppearance>(*donor.appearance);
              break;
            case 5:
              probe.group(donor.group).transform.position.x += .001f;
              break;
            case 6:
              donor.group = 0;
              break;
            case 7:
              probe.select(probe.active().id == context.back()
                               ? context.front()
                               : context.back());
              break;
            case 8: {
              auto app = std::make_shared<CreatorAppearance>(*donor.appearance);
              app->source_primitive = 1000000;
              donor.appearance = std::move(app);
              break;
            }
            }
            expected_external = probe;
            returned = true;
            return false;
          },
          &sentinel);
    } catch (const std::exception &) {
      rejected = true;
    }
    bool exact = false;
    if (expected_external) {
      try {
        detail::require_isolated_scene(*expected_external, probe, 0);
        exact = expected_external->active().id == probe.active().id;
      } catch (const std::exception &) {
      }
    }
    check(fired && returned && rejected && sentinel.work == 123 && exact,
          "Real callback readonly input mutation refuses without adopting any "
          "weights/report");
  }
  {
    auto probe = protected_before.snapshot();
    const auto exact = probe.encode();
    size_t calls = 0;
    bool rejected = false;
    try {
      (void)author_native_visible_skin(probe, std::span(&target, 1), prior,
                                       [&] { return ++calls == 64; });
    } catch (const std::exception &) {
      rejected = true;
    }
    check(rejected && calls == 64 && probe.encode() == exact,
          "Actual context preparation cancellation retains exact whole Scene");
  }
  {
    auto probe = protected_before.snapshot();
    probe.object(context.back()).visible = false;
    probe.object(context.back()).locked = true;
    const auto before_readonly = probe;
    check(author_native_visible_skin(probe, std::span(&target, 1), prior),
          "Hidden locked complete sibling remains readonly support");
    detail::require_isolated_scene(before_readonly, probe, target);
    check(
        probe.object(target).deformation->encode(
            probe.object(target).painted->mesh()) ==
            scene.object(target).deformation->encode(
                scene.object(target).painted->mesh()),
        "Readonly hidden/locked status does not change computed target field");
  }
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
                        {"work", report.work},
                        {"workspace", report.planned_workspace},
                        {"seconds", elapsed},
                        {"field_rows", report.rows},
                        {"changed", report.changed_parts},
                        {"field_equal_full_baseline", true},
                        {"protected_context_exact", true},
                        {"save_restore_exact", true},
                        {"scope",
                         "actual explicit public context; not automatic "
                         "context discovery or public/default admission"}}
             .dump(2);
  std::cout << "PASS PARTIAL_CONTEXT work=" << report.work
            << " bytes=" << report.planned_workspace << " seconds=" << elapsed
            << std::endl;
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << std::endl;
  return 1;
}
