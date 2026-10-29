// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/rig-edit.hpp"
#include "dome/document.hpp"
#include "dome/creation.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
void check(bool yes, const char *label) {
  if (!yes) throw std::runtime_error(label);
  std::cout << "PASS " << label << '\n';
}
template<class F> bool rejects(F f) { try { f(); return false; } catch (...) { return true; } }
Scene fixture() {
  Mesh mesh;
  mesh.positions = {{0, 0, 0}, {1, 0, 0}, {0, 1, 0}};
  mesh.normals.assign(3, {0, 0, 1}); mesh.indices = {0, 1, 2};
  mesh.uv = {{{0, 0}}, {{1, 0}}, {{0, 1}}};
  Scene s;
  auto group = s.add_group("Shared rig");
  auto &o = s.active(); o.group = group;
  o.painted = std::make_shared<PaintedMesh>(mesh, 64); o.mesh_active = true;
  DeformationState d(mesh);
  d.skeleton = Skeleton{{{"root", -1, {}, 1}, {"tip", 0, {{0, 1, 0}}, .2f}}};
  d.skin = SkinWeights{}; d.skin->vertices.assign(3, {{0, 1}});
  d.morphs = MorphSet(mesh); auto edited = mesh.positions; edited[0].z = .5f;
  d.morphs->set_target(mesh, "Forward", edited);
  s.set_deformation(o.id, d);
  s.duplicate(o.id); s.select(1);
  s.validate(); return s;
}
struct Controller {
  Document &doc; SpatialFrame f;
  Controller(Document &d) : doc(d) {
    f.focused = f.head_valid = true; f.head.position = {0, 1.6f, 0};
    f.hands[1].tracked.valid = true;
    f.hands[1].tracked.grip.position = {0, 1.48f, -.5f}; send();
  }
  void send(float x = 0, float y = 0, bool click = false) {
    ++f.sequence; f.time += .05;
    f.hands[1].x = x; f.hands[1].y = y; f.hands[1].joystick = click;
    doc.submit(f); if (!doc.wait_input()) throw std::runtime_error("Input timeout");
  }
  void sector(int i) {
    float a = i * 3.14159265358979323846f / 6;
    send(std::cos(a), std::sin(a)); send(std::cos(a), std::sin(a), true); send();
  }
  void rig() {
    send(); send(0, 0, true); send(); sector(8); sector(10); sector(5);
    check(doc.frame().state->menu_page == 11, "Joystick reaches native rig and shapekey page");
  }
};
} // namespace
int main(int argc, char **argv) try {
  if (argc == 3 && std::string_view(argv[1]) == "--ui-fixture") {
    fixture().save(argv[2]); return 0;
  }
  auto source = fixture(); auto s = source.snapshot();
  auto before = source.encode();
  auto paint = s.active().painted;
  RigEdit edit; edit.joint = JointAdjustment{0, {0, 0, 90}, {}};
  edit_rig(s, 1, edit);
  auto p = s.active().deformation->preview(s.active().painted->mesh());
  check(length(p.positions[1] - Vec3{0, 1, 0}) < 1e-5f,
        "Independent weighted vertex follows expected local ninety-degree rotation");
  check(s.objects()[1].deformation->pose[0].rotation.z > .7f,
        "Linked pose follows all exact native rig parts in the explicit group");
  check(source.encode() == before && s.active().painted == paint,
        "Pose operation leaves source snapshot, rest geometry and paint untouched");
  RigEdit shape; shape.morph = MorphWeightEdit{0, .4f}; edit_rig(s, 1, shape);
  check(s.active().deformation->morph_weights[0] == .4f &&
            s.objects()[1].deformation->morph_weights.empty(),
        "Shapekey values affect only explicitly selected native target identities");
  RigEdit reset; reset.reset_pose = true; edit_rig(s, 1, reset);
  check(s.active().deformation->pose.empty() && s.objects()[1].deformation->pose.empty() &&
        s.active().deformation->morph_weights[0] == .4f,
        "Grouped reset returns to rest without clearing shapekey weights");
  auto locked = source.snapshot(); locked.object(2).locked = true;
  auto exact = locked.encode();
  check(rejects([&] { edit_rig(locked, 1, edit); }) && locked.encode() == exact,
        "Locked linked part rejects atomically without partial source mutation");
  auto cancelled = source.snapshot();
  size_t checks = 0;
  check(rejects([&] { edit_rig(cancelled, 1, edit, [&] { return ++checks > 2; }); }) && cancelled.encode() == before,
        "Cancelled detached rig operation leaves complete source unchanged");
  edit.joint->joint = 9;
  check(rejects([&] { edit_rig(s, 1, edit); }), "Out-of-range joint identity rejects");
  edit.joint->joint = 0; edit.joint->degrees.x = NAN;
  check(rejects([&] { edit_rig(s, 1, edit); }), "Nonfinite joint changes reject");
  shape.morph->weight = 1.01f;
  check(rejects([&] { edit_rig(s, 1, shape); }), "Out-of-range shapekey weights reject");
  auto reopened = Scene::decode(s.encode());
  check(reopened.encode() == s.encode(), "Native restart retains accepted pose and shapekey edits");
  Document doc; doc.replace(source.snapshot()); auto draft = doc.draft_scene();
  edit = {}; edit.joint = JointAdjustment{0, {0, 0, 45}, {}};
  doc.edit_scene("Pose", [edit](Scene &scene, const auto &cancel) { edit_rig(scene, 1, edit, cancel); }, draft.source);
  check(doc.wait_asset() && doc.frame().state->error.empty() && doc.wait_mesh(),
        "Pose worker publishes immutable deformed rendering snapshot");
  auto accepted = doc.copy_scene().encode();
  check(doc.undo() && doc.copy_scene().encode() == before && doc.redo() && doc.copy_scene().encode() == accepted,
        "Linked rig editing uses one exact global undo and redo transaction");
  check(rejects([&] { doc.edit_scene("Stale pose", [](Scene &, const auto &) {}, draft.source); }),
        "Stale modal authoring source cannot overwrite a newer scene");
  Document xr; xr.replace(source.snapshot()); Controller controller(xr);
  controller.rig(); controller.sector(0);
  check(xr.frame().state->controls.parameter == VrParameter::RigRotation &&
        xr.frame().state->controls.components == 3,
        "Joystick opens three-axis bounded joint control");
  controller.send(1, 0); controller.send(1, 0);
  check(xr.copy_scene().encode() == before && xr.frame().state->controls.value[0] > 0,
        "Continuous joystick rotation stages values without blocking geometry work");
  controller.send(0, -1); controller.send(); controller.send(1, 0);
  check(xr.frame().state->controls.selected == 1 && xr.frame().state->controls.value[1] > 0,
        "Vertical joystick changes XYZ rows while horizontal motion adjusts the selected axis");
  controller.send(0, 0, true); controller.send();
  check(xr.wait_asset() && xr.frame().state->error.empty() && !xr.copy_scene().active().deformation->pose.empty(),
        "Joystick confirms pose on guarded worker");
  check(xr.undo() && xr.copy_scene().encode() == before,
        "Controller pose operation has one exact undo");
  xr.cancel_spatial_menu(); controller.rig(); controller.sector(3);
  controller.send(1, 0); controller.send(1, 0);
  xr.cancel_spatial_menu();
  check(xr.copy_scene().encode() == before && !xr.frame().state->asset_busy,
        "Cancelled headset shapekey slider makes no persistent edit");
  controller.send(); controller.rig(); controller.sector(3);
  controller.send(1, 0); controller.send(1, 0); controller.send(0, 0, true); controller.send();
  check(xr.wait_asset() && xr.frame().state->error.empty() &&
        xr.copy_scene().active().deformation->morph_weights[0] > 0,
        "Controller shapekey weight persists through accepted worker transaction");
  for (const char *baseline : {"vroid-beta-female", "vroid-beta-male"}) {
    Scene avatar;
    append_humanoid(avatar, humanoid_baseline_defaults(baseline), "CC0", {}, 64);
    auto selected = avatar.active().id;
    auto appearance = avatar.active().appearance;
    auto rest = avatar.active().painted;
    auto head = avatar.active().deformation->humanoid->joints.at("head");
    RigEdit pose; pose.joint = JointAdjustment{head, {0, 10, 0}, {}};
    edit_rig(avatar, selected, pose);
    size_t posed = 0;
    for (const auto &part : avatar.objects())
      if (part.deformation) {
        check(part.deformation->pose.size() == part.deformation->skeleton->bones.size() &&
              part.deformation->pose[head].rotation.y == avatar.active().deformation->pose[head].rotation.y,
              "Actual CC0 linked part follows the same explicit native rig pose");
        ++posed;
      }
    check(posed == 19 && avatar.active().painted == rest && avatar.active().appearance == appearance,
          "Actual nineteen-part rig edit retains source maps, vertex, joint and morph identities");
    auto face = std::find_if(avatar.objects().begin(), avatar.objects().end(), [](const auto &o) {
      return o.deformation && o.deformation->morphs && !o.deformation->morphs->targets.empty();
    });
    check(face != avatar.objects().end(), "Actual avatar retains an explicitly editable face target set");
    selected = face->id; avatar.select(selected); appearance = avatar.active().appearance;
    RigEdit actual_shape; actual_shape.morph = MorphWeightEdit{0, .35f};
    edit_rig(avatar, selected, actual_shape);
    check(avatar.active().deformation->morph_weights[0] == .35f && avatar.active().appearance == appearance,
          "Actual source shapekey edits change only weights and preserve source identity archive");
  }
  return 0;
} catch (const std::exception &e) { std::cerr << "FAIL " << e.what() << '\n'; return 1; }
