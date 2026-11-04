// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/creation.hpp"
#include "dome/document.hpp"
#include "dome/rig-fit-tool.hpp"
#include "dome/rig-rest.hpp"
#include "dome/skin-authoring.hpp"
#include "dome/source-placement.hpp"
#include "import-controller-driver.hpp"
#include "polygon-conversion.hpp"
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
Scene fixture(bool morph) {
  Mesh m;
  m.positions = {{-.1f, 0, 0}, {.1f, 0, 0}, {0, .5f, 0}};
  m.normals.assign(3, {0, 0, 1});
  m.indices = {0, 1, 2};
  m.uv = {{{0, 0}}, {{1, 0}}, {{.5f, 1}}};
  Scene s;
  s.active().painted = std::make_shared<PaintedMesh>(m, 64);
  s.active().mesh_active = true;
  s.active().name = "Actual native rig reference";
  DeformationState rig(m);
  rig.skeleton =
      Skeleton{{{"root", -1, {}, .3f}, {"tip", 0, {{0, .3f, 0}}, .2f}}};
  s.set_deformation(1, rig);
  const auto id = s.add("Original unskinned target", Volume{});
  auto &o = s.object(id);
  o.mesh_active = true;
  o.painted = std::make_shared<PaintedMesh>(m, 64);
  o.material.name = "Original painted material";
  o.material.metallic = .3f;
  o.material.roughness = .7f;
  const auto hidden = o.volume->add_layer("Retained hidden authoring layer");
  o.volume->layer_properties(hidden, "Retained hidden authoring layer", false,
                             false, .7f);
  o.volume->fill_mask(.2f);
  PaintSettings settings;
  settings.color = {.2f, .7f, .1f, .6f};
  Brush b;
  b.from = b.to = {0, .2f, 0};
  b.normal = {0, 0, 1};
  b.radius = .4f;
  o.painted->begin_stroke();
  o.painted->stamp(b, settings);
  if (!o.painted->end_stroke())
    throw std::runtime_error("Actual original paint sample missing");
  o.shape_source = capture_polygon_origin(s, id);
  if (morph) {
    DeformationState d(m);
    d.morphs = MorphSet(m);
    auto changed = m.positions;
    changed[0].z = .02f;
    d.morphs->set_target(m, "Original authored shape", changed);
    d.morph_weights = {.35f};
    s.set_deformation(id, d);
  }
  s.select(id);
  return s;
}
Scene bind(Scene source) {
  auto original = capture_polygon_origin(source, 2);
  SkinAuthoringOptions options;
  options.reference = 1;
  options.seed_distance = 1;
  if (!author_skin(source, 2, options))
    throw std::runtime_error(
        "Real existing native automatic bind did not change target");
  source.object(2).rig_source = std::move(original);
  source.validate();
  return source;
}
Scene changed_current(Scene bound) {
  auto &o = bound.object(2);
  o.painted = o.painted->snapshot();
  PaintSettings settings;
  settings.color = {.8f, .1f, .7f, 1};
  Brush b;
  b.from = b.to = {0, .2f, 0};
  b.normal = {0, 0, 1};
  b.radius = .4f;
  o.painted->begin_stroke();
  o.painted->stamp(b, settings);
  o.painted->end_stroke();
  o.material.color = {.7f, .1f, .1f, .5f};
  o.material.metallic = .9f;
  o.volume = std::make_shared<Volume>(o.volume->geometry_snapshot());
  o.volume->fill_mask(.8f);
  o.shape_source.reset();
  auto d = *o.deformation;
  if (d.morphs)
    d.morph_weights[0] = .75f;
  d.pose.clear();
  for (const auto &bone : d.skeleton->bones)
    d.pose.push_back(bone.rest);
  d.pose[1].rotation = {.92387953f, 0, 0, .38268343f};
  bound.set_deformation(2, d);
  return bound;
}
} // namespace
int main(int argc, char **argv) try {
  check(argc == 2, "Choose original-source restoration output directory");
  const std::filesystem::path out = argv[1];
  std::filesystem::create_directories(out);
  const auto state_root = out / "state", config_root = out / "config";
  std::filesystem::create_directories(state_root);
  std::filesystem::create_directories(config_root);
  setenv("XDG_STATE_HOME", state_root.c_str(), 1);
  setenv("XDG_CONFIG_HOME", config_root.c_str(), 1);
  for (bool morph : {false, true}) {
    auto original = fixture(morph);
    const auto expected = original.encode();
    auto current = changed_current(bind(original));
    const auto before = current.encode();
    check(current.object(2).rig_source &&
              (!current.object(2).rig_source->deformation ||
               !current.object(2).rig_source->deformation->skeleton),
          "Authentic complete first capsule records absent original skeleton");
    auto linked = current;
    bool refused = false;
    try {
      restore_rig_source(linked, 2, true);
    } catch (const std::invalid_argument &) {
      refused = true;
    }
    check(
        refused && linked.encode() == before,
        "Missing original skeleton never infers linked restoration membership");
    auto cancelled = current;
    int calls = 0;
    bool stopped = false;
    try {
      restore_rig_source(cancelled, 2, false, [&] { return ++calls >= 2; });
    } catch (const std::runtime_error &) {
      stopped = true;
    }
    check(stopped && calls >= 2 && cancelled.encode() == before,
          "Cancellation after detached restoration preserves complete current "
          "source");
    auto restored = current;
    restore_rig_source(restored, 2, false);
    check(restored.encode() == expected,
          "Full unskinned/deformation/morph/paint/material/shape/sculpt source "
          "restores exactly");
    check(!restored.object(2).deformation ||
              !restored.object(2).deformation->skeleton,
          "Restore removes only the subsequently created rig and skin");
    const auto path = out / (morph ? "morph.dsculpt" : "bare.dsculpt");
    current.save(path);
    auto reopened = Scene::load(path);
    restore_rig_source(reopened, 2, false);
    check(reopened.encode() == expected,
          "Complete originally unskinned capsule restores after actual native "
          "restart");
    auto placed = current;
    placed.object(2).transform.position = {1, 2, 3};
    auto placement_expected = original;
    placement_expected.object(2).transform = placed.object(2).transform;
    restore_rig_source(placed, 2, false);
    check(placed.encode() == placement_expected.encode(),
          "Full original data Restore retains explicit current placement and "
          "all other objects");
    for (bool hidden : {false, true}) {
      auto protected_scene = current;
      if (hidden)
        protected_scene.object(2).visible = false;
      else
        protected_scene.object(2).locked = true;
      const auto protected_before = protected_scene.encode();
      bool blocked = false;
      try {
        restore_rig_source(protected_scene, 2, false);
      } catch (const std::invalid_argument &) {
        blocked = true;
      }
      check(blocked && protected_scene.encode() == protected_before,
            "Hidden/locked current targets cannot bypass source restoration "
            "permissions");
    }
    Document doc;
    doc.replace(current);
    check(doc.wait_mesh(std::chrono::seconds(120)),
          "Actual native current mesh ready");
    doc.configure_object_grab(true, true);
    doc.hold_object(2);
    check(doc.wait_mesh(std::chrono::seconds(120)),
          "Actual pinned-source isolation ready");
    doc.begin_rig_fit();
    auto view = *doc.frame().state->rig_fit;
    view.restore_source = true;
    view.linked_rest = false;
    doc.configure_rig_fit(view);
    doc.accept_rig_fit();
    auto save = doc.save_async(path);
    save.get();
    check(doc.wait_asset() && doc.wait_mesh(std::chrono::seconds(120)) &&
              doc.frame().state->error.empty(),
          "Actual existing native fitting Apply queues complete unskinned "
          "Restore plus Save");
    check(doc.copy_scene().encode() == expected &&
              Scene::load(path).encode() == expected &&
              doc.frame().state->held_object == 2,
          "Pinned Restore preserves read-only reference and saves the exact "
          "original complete Scene");
    check(doc.undo() && doc.copy_scene().encode() == before && doc.redo() &&
              doc.copy_scene().encode() == expected,
          "Actual full-source restoration is exactly one native Undo/Redo");
    check(doc.undo() && doc.wait_mesh(std::chrono::seconds(120)),
          "Actual controller source restored to current rigged endpoint");
    doc.begin_rig_fit();
    view = *doc.frame().state->rig_fit;
    view.restore_source = true;
    view.linked_rest = false;
    doc.configure_rig_fit(view);
    check(
        rig_fit_tool_label(doc.frame().state->rig_fit.get(), 10) ==
            "Restore original source",
        "Controller action accurately names full original-source restoration");
    ImportDriver hand(doc);
    hand.open();
    for (int sector : {8, 10, 5, 7})
      hand.sector(sector);
    check(doc.frame().state->menu_page == 27,
          "Actual joystick route reaches existing fitting page27");
    hand.sector(10);
    doc.save_async(path).get();
    check(doc.wait_asset() && doc.wait_mesh(std::chrono::seconds(120)) &&
              doc.copy_scene().encode() == expected &&
              Scene::load(path).encode() == expected,
          "Actual controller Apply restores full unskinned source and accepted "
          "ordered Save");
  }
  std::cout << "PASS original_unskinned_restore_checks=" << checks << '\n';
  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
