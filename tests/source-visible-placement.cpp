// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/creation.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/skin-authoring.hpp"
#include "dome/source-placement.hpp"
#include "source-visible-pose.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
size_t checks = 0;
void check(bool value, const char *why) {
  ++checks;
  if (!value)
    throw std::runtime_error(why);
}
template <class F> bool refuses(F f) {
  try {
    f();
    return false;
  } catch (const std::exception &) {
    return true;
  }
}
} // namespace
int main() try {
  Scene input;
  append_humanoid(input, HumanoidControls{}, "REST weight authority", {}, 64);
  std::vector<uint64_t> ids;
  for (const auto &o : input.objects())
    if (o.painted)
      ids.push_back(o.id);
  check(ids.size() == 19, "Genuine complete19part cohort");
  SkinAuthoringOptions opts;
  opts.mode = SkinAuthoringMode::SourceVisible;
  opts.reference = ids.front();
  auto baseline = input.snapshot();
  check(author_source_visible_skin(baseline, ids, opts), "Baseline REST bind");
  for (int variant = 0; variant < 2; ++variant) {
    auto scene = input.snapshot();
    for (auto id : ids) {
      auto d = *scene.object(id).deformation;
      source_visible_test::pose(d, 3);
      if (d.morphs && !d.morphs->targets.empty()) {
        d.morph_weights.assign(d.morphs->targets.size(), 0);
        d.morph_weights[0] = .35f;
      }
      scene.set_deformation(id, std::move(d));
    }
    const auto group = scene.object(ids.front()).group;
    if (variant)
      scene.group(group).transform = {
          {1.25f, -.75f, .5f}, {0, 0, .38268343f, .92387953f}, 2.5f};
    auto before = scene.snapshot();
    check(author_source_visible_skin(scene, ids, opts),
          "Bind retained displayed pose/morph with optional common similarity");
    for (auto id : ids) {
      const auto &o = scene.object(id), &b = before.object(id);
      check(o.deformation->skin->vertices ==
                baseline.object(id).deformation->skin->vertices,
            "REST field/output exactly matches baseline under pose/common "
            "placement");
      auto restored = *o.deformation;
      restored.skin = b.deformation->skin;
      check(restored.encode(b.painted->mesh()) ==
                b.deformation->encode(b.painted->mesh()),
            "Pose/morph/rest/frame identity retained");
      check(scene.world_matrix(id).matrix() == before.world_matrix(id).matrix(),
            "Placement unchanged by rebind");
    }
  }
  for (int variant = 0; variant < 3; ++variant) {
    auto scene = input.snapshot();
    if (variant == 0)
      scene.object(ids.back()).transform.position.x = .01f;
    if (variant == 1) {
      auto a = AffineTransform{}.matrix();
      a[0] = 2;
      scene.group(scene.object(ids.front()).group).affine_edit =
          std::make_shared<const NativeAffineEdit>(AffineTransform(a));
    }
    if (variant == 2) {
      auto a = std::make_shared<CreatorAppearance>(
          *scene.object(ids.front()).appearance);
      a->asset.reset();
      scene.object(ids.front()).appearance = a;
    }
    const auto before = variant == 2 ? nlohmann::json{} : scene.encode();
    const auto owners = scene.objects();
    SourceVisibleSkinReport report{11, 12, 13, 14, 15, 16};
    check(refuses([&] {
            author_source_visible_skin(scene, ids, opts, {}, &report);
          }),
          "Refuse relative/affine/foreign source ambiguity");
    bool exact = variant == 2 || scene.encode() == before;
    for (const auto &o : owners) {
      const auto &current = scene.object(o.id);
      exact &= current.deformation == o.deformation &&
               current.appearance == o.appearance &&
               current.painted == o.painted && current.volume == o.volume &&
               current.rig_source == o.rig_source;
    }
    check(exact && report.work == 11 && report.controls == 16,
          "Refusal leaves exact Scene/report");
  }
  std::cout << "PASS " << checks
            << " REST/pose/morph/common-similarity/native source checks\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
