// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/creation.hpp"
#include "dome/document.hpp"
#include "dome/polygon-scene.hpp"
#include <bit>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
size_t checks = 0;
void check(bool v, const char *why) {
  ++checks;
  if (!v)
    throw std::runtime_error(why);
  std::cout << "PASS " << why << std::endl;
}
template <class F> bool refuses(F f) {
  try {
    f();
    return false;
  } catch (const std::exception &) {
    return true;
  }
}
float weight(const VertexWeights &row, uint32_t joint) {
  for (auto x : row)
    if (x.joint == joint)
      return x.weight;
  return 0;
}
} // namespace
int main(int argc, char **argv) try {
  check(argc == 2, "Choose actual locks/pinned output directory");
  std::filesystem::path out = argv[1];
  std::filesystem::create_directories(out);
  Scene original;
  append_humanoid(original, HumanoidControls{}, "Hard locks source", {}, 64);
  auto found = std::find_if(
      original.objects().begin(), original.objects().end(), [](const auto &o) {
        if (o.name.find("Body_00_SKIN") == std::string::npos ||
            !o.deformation || !o.deformation->skin || !o.deformation->humanoid)
          return false;
        const auto joint = o.deformation->humanoid->joints.at("hips");
        bool present = false, absent = false;
        for (const auto &row : o.deformation->skin->vertices) {
          present |= weight(row, joint) > 0;
          absent |= weight(row, joint) == 0;
        }
        // Test fixture selection only: require both exact hard-lock cases.
        // Artist values are not fed into the field except the explicitly
        // checked lock.
        return present && absent;
      });
  check(found != original.objects().end(),
        "Genuine Body fixture contains present and absent hips locks");
  const auto id = found->id;
  original.select(id);
  SkinAuthoringOptions options;
  options.mode = SkinAuthoringMode::SourceVisible;
  options.reference = id;
  const auto &before = *found->deformation;
  const auto hips = before.humanoid->joints.at("hips");
  options.locks.assign(before.skeleton->bones.size(), 0);
  options.locks[hips] = 1;
  auto partial = original.snapshot();
  const auto partial_input = partial.objects();
  SourceVisibleSkinReport report;
  check(author_source_visible_skin(partial, std::span(&id, 1), options, {},
                                   &report),
        "Actual Body partial-lock source-assisted field changes unlocked rows");
  bool exact = true, positive = false, absent = false;
  size_t present_count = 0, absent_count = 0, different_count = 0;
  for (size_t v = 0; v < before.skin->vertices.size(); ++v) {
    const float a = weight(before.skin->vertices[v], hips),
                b = weight(partial.object(id).deformation->skin->vertices[v],
                           hips);
    exact &= std::bit_cast<uint32_t>(a) == std::bit_cast<uint32_t>(b);
    positive |= a > 0;
    absent |= a == 0;
    present_count += a > 0;
    absent_count += a == 0;
    if (std::bit_cast<uint32_t>(a) != std::bit_cast<uint32_t>(b)) {
      ++different_count;
      if (different_count < 4)
        std::cerr << "LOCK_ROW v=" << v << " old=" << a << " new=" << b
                  << " joint=" << hips << std::endl;
    }
  }
  std::cerr << "LOCK_DIAGNOSTIC exact=" << exact << " present=" << present_count
            << " absent=" << absent_count << " different=" << different_count
            << std::endl;
  check(exact && positive && absent,
        "Present locked float values and absent locked influences are exact");
  check(partial.active().rig_source &&
            partial.active().rig_source->deformation ==
                original.active().deformation,
        "Partial bind captures complete original weights");
  for (const auto &o : partial_input)
    if (o.id != id)
      check(partial.object(o.id).deformation == o.deformation &&
                partial.object(o.id).painted == o.painted,
            "Partial target leaves every support deformation/paint owner "
            "unchanged");
  for (int kind = 0; kind < 2; ++kind) {
    auto refused = original.snapshot();
    auto limited = options;
    if (kind == 0)
      limited.source_visible.maximum_work = 1024;
    else
      limited.source_visible.maximum_workspace = 4096;
    const auto input = refused.encode();
    SourceVisibleSkinReport untouched{7, 8, 9, 10, 11, 12};
    check(refuses([&] {
            author_source_visible_skin(refused, std::span(&id, 1), limited, {},
                                       &untouched);
          }) &&
              refused.encode() == input && untouched.work == 7 &&
              untouched.controls == 12,
          "Low source resources refuse atomically without report/output");
  }
  Document doc;
  doc.replace(original);
  check(doc.wait_mesh(), "Pinned changed-target initial mesh ready");
  doc.configure_object_grab(true, true);
  doc.hold_object(id);
  check(doc.wait_mesh(), "Pinned target authority ready before real bind");
  const auto input = doc.copy_scene().encode();
  const auto authority = doc.frame().state->edit_authority;
  const auto path = out / "pinned.dsculpt";
  doc.copy_scene().save(path);
  doc.bind_skin(options);
  auto save = doc.save_async(path);
  save.get();
  check(doc.wait_asset() && doc.wait_mesh() && doc.frame().state->error.empty(),
        "Actual pinned changed bind and ordered Save complete");
  auto accepted = doc.copy_scene();
  check(accepted.encode() != input &&
            Scene::load(path).encode() == accepted.encode() &&
            doc.frame().state->held_object == id &&
            doc.frame().state->edit_authority == authority &&
            doc.object_editable(id),
        "Changed shell remains held/editable with exact accepted Save");
  for (const auto &o : original.objects())
    if (o.id != id)
      check(accepted.object(o.id).deformation == o.deformation,
            "Pinned real bind leaves every ghost skin owner unchanged");
  check(accepted.active().deformation->skin->vertices ==
            partial.active().deformation->skin->vertices,
        "Native pinned publication emits the exact independently called "
        "partial-lock field");
  check(doc.undo() && doc.copy_scene().encode() == input && !doc.undo(),
        "Changed pinned bind adds exactly one global Undo");
  check(doc.redo() && doc.copy_scene().encode() == accepted.encode(),
        "Redo restores real pinned result and original capsule");
  doc.configure_object_grab(true, false);
  check(doc.begin_object_grab(id),
        "Actual object motion begins after successful bind");
  check(
      !doc.frame().state->can_bind_skin &&
          refuses([&] { doc.bind_skin(options); }) &&
          !doc.frame().state->asset_busy,
      "Motion readiness/API agree with existing generic admission restriction");
  doc.end_object_grab(false);
  std::cout << "SUMMARY checks=" << checks << " work=" << report.work
            << " workspace=" << report.planned_workspace
            << " partial_locks=1 changed_held=1\n";
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
