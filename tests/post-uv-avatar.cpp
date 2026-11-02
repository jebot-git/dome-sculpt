// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/creation.hpp"
#include "dome/document.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/shapekey-authoring.hpp"
#include <iostream>
#include <cstdlib>

using namespace dome;
namespace {
unsigned checks=0;
void check(bool result,const char *message){if(!result)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<std::endl;}
struct Hand{
  Document &document;SpatialFrame frame;
  explicit Hand(Document &d,Vec3 origin):document(d){frame.focused=frame.head_valid=true;frame.head.position={0,1.6f,0};auto &hand=frame.hands[1].tracked;hand.valid=hand.aimValid=true;send();hand.grip.position=hand.aim.position=ob::transform(document.frame().state->canvas,{origin.x,origin.y,origin.z});hand.grip.orientation=hand.aim.orientation=document.frame().state->canvas.orientation;send();}
  void send(float x=0,float y=0,bool click=false,float pressure=0){++frame.sequence;frame.time+=.05;auto &hand=frame.hands[1];hand.x=x;hand.y=y;hand.joystick=click;hand.tracked.trigger=pressure;document.submit(frame);if(!document.wait_input())throw std::runtime_error("Actual avatar controller timeout");}
  void sector(int index){const float a=index*3.14159265358979323846f/6;send(std::cos(a),std::sin(a));send(std::cos(a),std::sin(a),true);send();}
  void shapes(){send();send(0,0,true);send();sector(8);sector(9);sector(7);}
};
}
void ready(Document &document) {
  if (!document.wait_asset(std::chrono::seconds(120)) ||
      !document.wait_mesh(std::chrono::seconds(120)) ||
      !document.frame().state->error.empty())
    throw std::runtime_error("Actual post-UV avatar worker: " + document.frame().state->error);
}
int main(int argc, char **argv) try {
  if (argc != 2) return 2;
  std::filesystem::create_directories(argv[1]);
  const uint32_t resolution = std::getenv("DOME_POST_UV_AVATAR_RES")
      ? uint32_t(std::stoul(std::getenv("DOME_POST_UV_AVATAR_RES"))) : 256;
  for (const char *baseline : {"vroid-beta-female", "vroid-beta-male"}) {
    Scene scene;
    const auto group = append_humanoid(scene, humanoid_baseline_defaults(baseline), "Post-UV CC0 avatar", {}, resolution);
    const auto face = std::find_if(scene.objects().begin(), scene.objects().end(), [&](const auto &object) {
      return object.group == group && object.name.find("Face_00_SKIN") != std::string::npos &&
          object.deformation && object.deformation->morphs;
    });
    if (face == scene.objects().end()) throw std::runtime_error("Missing actual VRoid face");
    scene.select(face->id);
    auto &object = scene.active();
    std::cout << "AVATAR " << baseline << " faces=" << object.painted->mesh().indices.size() / 3
              << " targets=" << object.deformation->morphs->targets.size()
              << " policy=" << int(object.painted->surface_diagnostics().policy) << std::endl;
    auto prepared = prepare_source_normals({object.painted, object.appearance,
        object.material_bindings, object.deformation, object.material});
    object.painted = prepared.paint;
    object.material_bindings = prepared.material_bindings;
    scene.validate();
    Document document;
    document.replace(scene);
    ready(document);
    if(std::string_view(baseline)=="vroid-beta-female"){
      const auto exact=document.copy_scene().encode();
      document.begin_uv();check(document.wait_uv(std::chrono::seconds(120)),"Actual fractional UV draft initializes");
      document.edit_uv(UvAction::Scale,.95f);check(document.wait_uv(std::chrono::seconds(120)),"Actual fractional UV geometry draft validates");
      document.end_uv(true);check(document.wait_asset(std::chrono::seconds(120))&&document.wait_mesh(std::chrono::seconds(120)),"Actual fractional normalUV failure finishes");
      check(document.frame().state->error.find("quality bound")!=std::string::npos&&document.copy_scene().encode()==exact,"Actual subpixel quality failure retains all original avatar/source bytes");
      document.end_uv(false);check(!document.undo()&&document.copy_scene().encode()==exact,"Failed fractional UV Apply and Cancel add no global history endpoint");
    }
    for (int pass = 0; pass < 2; ++pass) {
      std::cout << "UV " << baseline << " pass=" << pass << std::endl;
      document.begin_uv();
      if (!document.wait_uv(std::chrono::seconds(120))) throw std::runtime_error("Actual avatar UV initialization timeout");
      std::cout << "UV_INITIAL valid=" << document.frame().state->uv->valid
                << " error=" << document.frame().state->uv->error << std::endl;
      document.edit_uv(UvAction::Move,0,(pass? -1.f:1.f)/float(resolution));
      document.end_uv(true);
      ready(document);
    }
    const auto before = document.copy_scene();
    const auto original = before.encode();
    const auto slot = before.active().deformation->morphs->targets.size();
    ShapeKeyEdit edit;
    edit.name = "Actual avatar key after UV";
    document.author_shape(edit);
    ready(document);
    edit.action = ShapeKeyAction::Replace;
    edit.target = slot;
    edit.positions = before.active().painted->mesh().positions;
    size_t vertex = before.active().painted->mesh().indices[2];
    for (size_t v = 0; v < edit.positions.size(); ++v)
      if (edit.positions[v].z > edit.positions[vertex].z) vertex = v;
    edit.positions[vertex].y += .005f;
    document.author_shape(edit);
    ready(document);
    document.configure_shape_sculpt(true);
    ready(document);
    NativeStrokeSample sample;sample.origin=edit.positions[vertex]+Vec3{0,0,.3f};sample.direction={0,0,-1};
    sample.brush.radius=.025f;sample.brush.tool=Tool::Clay;sample.brush.pressure=.2f;
    const auto stroke_before=document.copy_scene().encode();
    if(!document.begin_native_stroke()||!document.sample_native_stroke(sample))throw std::runtime_error("Actual avatar native key gesture refused");
    document.end_native_stroke();
    if(!document.wait_native_stroke(std::chrono::seconds(120)))throw std::runtime_error("Actual avatar native stroke timeout");
    ready(document);
    if(document.copy_scene().encode()==stroke_before)throw std::runtime_error("Actual avatar native stroke did not edit target");
    const auto stroked=document.copy_scene().encode();
    if(!document.undo()||document.copy_scene().encode()!=stroke_before||!document.redo()||document.copy_scene().encode()!=stroked)throw std::runtime_error("Actual avatar native stroke Undo/Redo differs");
    ready(document);
    if(!document.begin_native_stroke()||!document.sample_native_stroke(sample))throw std::runtime_error("Actual avatar cancel gesture refused");
    document.end_native_stroke(false);
    if(!document.wait_native_stroke(std::chrono::seconds(120)))throw std::runtime_error("Actual avatar native cancel timeout");
    ready(document);
    if(document.copy_scene().encode()!=stroked)throw std::runtime_error("Actual avatar canceled key stroke changed source");
    document.configure_shape_sculpt(false);
    ready(document);
    const auto controller_before=document.copy_scene().encode();
    Hand hand(document,sample.origin);hand.shapes();hand.sector(11);ready(document);
    check(document.frame().state->shape_sculpt&&!document.frame().state->menu,"Actual joystick navigation opens avatar target sculpt after repeated normalUV edits");
    hand.send();hand.send(0,0,false,.4f);
    check(document.frame().state->native_pending,"Actual controller trigger admits an avatar target gesture");
    hand.send();check(document.wait_native_stroke(std::chrono::seconds(120)),"Actual avatar controller target gesture completes");ready(document);
    const auto controller_after=document.copy_scene().encode();
    check(controller_after!=controller_before,"Actual controller ray edits the current avatar target");
    hand.send(0,0,false,.4f);hand.frame.focused=false;hand.send(0,0,false,.4f);
    check(document.wait_native_stroke(std::chrono::seconds(120)),"Actual avatar focus cancellation completes");ready(document);
    check(document.copy_scene().encode()==controller_after,"Controller focus loss retains the exact prior accepted avatar endpoint");
    document.configure_shape_sculpt(false);ready(document);
    auto path = std::filesystem::path(argv[1]) / (std::string(baseline) + ".dsculpt");
    document.save(path);
    Document reopened;
    reopened.open(path);
    ready(reopened);
    if (reopened.copy_scene().encode() != document.copy_scene().encode()) throw std::runtime_error("Actual avatar restart differs");
    edit = {};
    edit.action = ShapeKeyAction::Restore;
    reopened.author_shape(edit);
    ready(reopened);
    if (reopened.copy_scene().encode() != original) throw std::runtime_error("Actual avatar post-UV Restore differs");
    std::cout << "PASS actual " << baseline << " post-UV Create/Replace/restart/Restore" << std::endl;
  }
  std::cout<<"PASS "<<checks<<" actual post-UV avatar checks\n";
  return 0;
} catch (const std::exception &error) {
  std::cerr << "FAIL " << error.what() << '\n';
  return 1;
}
