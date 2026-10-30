// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/creation.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
void check(bool ok, const char *label) {
  if (!ok) throw std::runtime_error(label);
  std::cout << "PASS " << label << '\n';
}
template<class F> bool rejects(F f) { try { f(); return false; } catch (...) { return true; } }
}
struct Controller {
  Document &doc; SpatialFrame frame;
  Controller(Document &d):doc(d){
    frame.focused=frame.head_valid=true; frame.head.position={0,1.6f,0};
    frame.hands[1].tracked.valid=true; frame.hands[1].tracked.grip.position={0,1.48f,-.5f}; send();
  }
  void send(float x=0,float y=0,bool click=false){
    ++frame.sequence; frame.time+=.05; frame.hands[1].x=x;frame.hands[1].y=y;frame.hands[1].joystick=click;
    doc.submit(frame); if(!doc.wait_input())throw std::runtime_error("Controller timed out");
  }
  void sector(int i){float a=i*3.14159265358979323846f/6;send(std::cos(a),std::sin(a));send(std::cos(a),std::sin(a),true);send();}
  void avatars(){send();send(0,0,true);send();sector(8);sector(10);check(doc.frame().state->menu_page==9,"Joystick reaches avatar authoring page");}
};
int main() try {
  Scene scene;
  auto recipe = std::make_shared<HumanoidControls>(humanoid_baseline_defaults("makehuman-hm08"));
  CanvasTransform root{}; root.position = {1,0,0}; root.scale = 2;
  auto a = scene.add_group("Avatar", root, 0, recipe);
  CanvasTransform local{}; local.position = {0,1,0};
  auto b = scene.add_group("Nested", local, a);
  scene.active().group = b;
  scene.active().transform.position = {0,0,1};
  scene.validate();
  auto world = scene.world_transform(scene.active().id);
  check(length(Vec3{world.position.x,world.position.y,world.position.z} - Vec3{1,2,2}) < 1e-6f && world.scale == 2,
        "Nested group transforms compose once in world coordinates");
  recipe->height = 1.9f;
  check(scene.group(a).humanoid->height == 1.7f, "Creator parameters detach caller aliases");
  auto snapshot = scene.snapshot();
  scene.group(a).transform.position.x = 2;
  check(snapshot.group(a).transform.position.x == 1, "Group snapshots remain exact during edits");
  auto loaded = Scene::decode(scene.encode());
  check(loaded.groups().size() == 2 && loaded.active().group == b &&
        loaded.group(a).humanoid->height == 1.7f && loaded.encode() == scene.encode(),
        "Native v5 roundtrip preserves group IDs, parents, transforms and creator controls");
  auto bad = loaded.encode(); bad["groups"][0]["parent"] = b;
  check(rejects([&] { Scene::decode(bad); }), "Cyclic group hierarchies are rejected");
  bad = loaded.encode(); bad["objects"][0]["group"] = -1;
  check(rejects([&] { Scene::decode(bad); }), "Negative group IDs are rejected before conversion");
  bad = loaded.encode(); bad["objects"][0]["group"] = 999;
  check(rejects([&] { Scene::decode(bad); }), "Unknown group parents/objects cannot enter scenes");
  auto flat_world = loaded.world_transform(loaded.active().id);
  loaded.erase_group(a, false);
  check(loaded.groups().empty() && loaded.active().group == 0 &&
        loaded.active().transform.position.x == flat_world.position.x &&
        loaded.active().transform.scale == flat_world.scale,
        "Ungrouping retains exact world placement");
  scene.erase_group(a);
  check(scene.groups().empty() && scene.objects().size() == 1 && scene.active().group == 0,
        "Deleting the last group retains a valid empty sculpt document");
  check(std::string(spatial_labels(9)[2])=="Center part" && spatial_action(9,2)==SpatialCommand::Center,"XR avatar page replaces the removed mesh choice with a useful center action");
  Document document;
  const auto legacy_before=document.copy_scene().encode();document.paint_command(SpatialCommand::CreateMakeHuman);
  check(document.copy_scene().encode()==legacy_before && !document.frame().state->asset_busy,"Removed MakeHuman creation command cannot be invoked through legacy controller bindings");
  document.seed();
  document.edit_scene("Create humanoid", [](Scene &scene, const auto &cancelled) {
    CanvasTransform transform{}; transform.position.y = -.85f;
    append_humanoid(scene, humanoid_baseline_defaults("vroid-beta-female"), "Test avatar", transform, 64, cancelled);
  });
  check(document.wait_asset() && document.frame().state->error.empty(), "Creator appends paintable rigged parts on the guarded worker");
  auto made = document.copy_scene();
  check(made.groups().size() == 1 && made.objects().size() == 20 && made.active().deformation && made.active().painted,
        "Creator parts share one retained recipe and select the rigged body");
  auto roundtrip = Scene::decode(made.encode());
  check(roundtrip.groups().size() == 1 && std::any_of(roundtrip.objects().begin(),roundtrip.objects().end(),[](const auto &o){return o.deformation && o.deformation->morphs && !o.deformation->morphs->targets.empty();}),
        "Created topology, weights, face morphs and parameters survive native roundtrip");
  check(document.undo() && document.copy_scene().objects().size() == 1 && document.copy_scene().groups().empty() &&
        document.redo() && document.copy_scene().objects().size() == 20,
        "Entire avatar creation shares one exact global undo/redo transaction");
  check(document.wait_mesh(), "Grouped creator publishes immutable rendered geometry");
  for (const char *baseline : {"vroid-beta-female", "vroid-beta-male"}) {
    Document actual;
    actual.edit_scene("Create CC0 VRoid", [baseline](Scene &scene, const auto &cancelled) {
      auto controls = humanoid_baseline_defaults(baseline);
      append_humanoid(scene, controls, "CC0 baseline", {}, 64, cancelled);
    });
    check(actual.wait_asset(std::chrono::seconds(120)) && actual.frame().state->error.empty(),
          "Actual CC0 VRoid creates all paintable linked-UV parts through guarded worker");
    auto retained = actual.copy_scene();
    check(retained.objects().size() == 20 && retained.active().appearance &&
          retained.active().painted->source_maps(), "Original CC0 appearance remains on every grouped source model");
    auto encoded = retained.encode();
    check(encoded["source_models"].size() == 1 && encoded["source_images"].size() == 25,
          "Actual avatar stores one exact source and25 shared images");
    auto restarted = Scene::decode(encoded);
    check(restarted.objects().size() == 20 && restarted.active().appearance->original_bytes &&
          restarted.group(restarted.active().group).humanoid->baseline == baseline,
          "Actual CC0 grouped scene restarts with source and parameters intact");
    check(actual.undo() && actual.copy_scene().objects().size() == 1,
          "Actual CC0 creation undoes as one exact transaction");
  }

  Document tracked; tracked.seed(); Controller controller(tracked);
  controller.avatars(); controller.sector(0);
  check(tracked.wait_asset(std::chrono::seconds(120)) && tracked.frame().state->error.empty() && tracked.copy_scene().objects().size()==20,
        "Joystick creates actual CC0 female on the guarded worker");
  controller.avatars(); controller.sector(6);
  check(tracked.wait_asset() && tracked.copy_scene().objects().size()==1,
        "Joystick deletes an avatar as one grouped operation");
  check(tracked.undo() && tracked.copy_scene().objects().size()==20,
        "Grouped avatar deletion shares exact global undo");
  controller.avatars(); controller.sector(7);
  check(tracked.wait_asset() && tracked.copy_scene().groups().empty() && tracked.copy_scene().objects().size()==20,
        "Joystick ungroups an avatar without deleting its parts");
  Document male_controller;male_controller.seed();Controller male_input(male_controller);male_input.avatars();male_input.sector(1);
  check(male_controller.wait_asset(std::chrono::seconds(120)) && male_controller.frame().state->error.empty() && male_controller.copy_scene().objects().size()==20 && male_controller.copy_scene().group(male_controller.copy_scene().active().group).humanoid->baseline=="vroid-beta-male","Joystick creates the actual CC0 male through the second offered baseline command");
  return 0;
} catch (const std::exception &e) { std::cerr << e.what() << '\n'; return 1; }
