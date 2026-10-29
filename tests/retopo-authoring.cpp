// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using namespace dome;
namespace {
unsigned checks = 0;
void check(bool value, const char *message) {
  ++checks;
  if (!value) throw std::runtime_error(message);
  std::cout << "PASS " << message << '\n';
}
template<class F> bool rejects(F f) { try { f(); return false; } catch (const std::exception &) { return true; } }
Scene plane() {
  Mesh m;
  m.positions = {{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};
  m.normals.assign(4, {0,0,1}); m.uv = {{0,0},{1,0},{1,1},{0,1}}; m.indices = {0,1,2,0,2,3};
  Scene scene; scene.active().name = "Reference";
  scene.active().painted = std::make_shared<PaintedMesh>(std::move(m),64);
  scene.active().mesh_active = true; return scene;
}
void point(RetopoWorkspace &w, Vec3 p) {
  w.edit(RetopoAction::BeginGesture); w.edit(RetopoAction::Sample,p); w.edit(RetopoAction::EndGesture);
}
void point(Document &d, Vec3 p) {
  if (!d.begin_desktop()) throw std::runtime_error("Desktop gesture refused");
  Brush b; b.from=b.to=p; b.radius=.01f; d.stamp_desktop(b); d.end_desktop();
  if (!d.wait_retopology() || !d.frame().state->error.empty()) throw std::runtime_error(d.frame().state->error);
}
struct Controller {
  Document &doc; SpatialFrame f;
  explicit Controller(Document &d):doc(d) {
    f.focused=f.head_valid=true; f.head.position={0,1.6f,0}; f.hands[1].tracked.valid=true; send();
  }
  void send(float pressure=0, float x=0, float y=0, bool click=false) {
    ++f.sequence; f.time+=.02; auto &h=f.hands[1]; h.tracked.trigger=pressure;h.x=x;h.y=y;h.joystick=click;
    doc.submit(f); if(!doc.wait_input())throw std::runtime_error("Input timed out");
  }
  void sector(int index) {
    float angle=index*6.283185307f/12; send(0,std::cos(angle),std::sin(angle));
    send(0,std::cos(angle),std::sin(angle),true); send();
  }
};
}
int main(int argc, char **argv) try {
  if (argc == 3 && std::string(argv[1]) == "--ui-fixture") { plane().save(argv[2]); return 0; }
  auto scene=plane(); auto original=scene.encode();
  RetopoWorkspace w(scene);
  point(w,{-.2f,-.2f,.02f}); auto first=w.view();
  check(first.topology->vertices().size()==1 && std::abs(first.topology->vertices().begin()->second.position.z)<1e-7f,
        "Placement projects onto the actual immutable source surface");
  point(w,{-.2f,.2f,.02f});point(w,{.2f,.2f,.02f});point(w,{.2f,-.2f,.02f});
  check(first.topology->vertices().size()==1 && w.view().selected.size()==4 && scene.encode()==original,
        "Ordered quad selection preserves earlier snapshots and the complete source scene");
  w.edit(RetopoAction::Face); auto quad=w.view().topology; auto encoded=quad->encode();
  check(quad->faces().size()==1 && quad->triangulate().mesh.indices.size()==6 && w.view().selected.empty(),
        "Clockwise input becomes a consistently outward quad with two export triangles");
  w.edit(RetopoAction::Undo);check(w.view().topology->faces().empty()&&w.view().selected.size()==4,"Draft Undo restores the unfinished ordered patch exactly");
  w.edit(RetopoAction::Redo);check(w.view().topology==quad,"Draft Redo restores the exact immutable topology pointer");
  w.edit(RetopoAction::Mode);w.edit(RetopoAction::BeginGesture);w.edit(RetopoAction::Sample,{-.2f,-.2f,0});
  w.edit(RetopoAction::Sample,{-.25f,-.2f,.01f});
  check(w.view().topology->encode()!=encoded,"Move gesture projects a real stable point while retaining faces");
  w.edit(RetopoAction::CancelGesture);

  auto grouped = scene.snapshot();
  auto group = grouped.add_group("Rotated source", {{1,2,3},{0,0,.70710678f,.70710678f},2});
  grouped.active().group = group;
  RetopoWorkspace scaled(grouped);
  scaled.edit(RetopoAction::BeginGesture);
  check(rejects([&]{scaled.edit(RetopoAction::Sample,{0,0,.03f},50);}),
        "A 50 mm bound rejects 60 mm world distance under a rotated scaled group");
  scaled.edit(RetopoAction::CancelGesture);
  point(scaled,{0,0,.02f});
  check(std::abs(scaled.view().topology->vertices().begin()->second.position.z)<1e-6f,
        "Rotated grouped source projection preserves local topology and world-millimetre bounds");
  check(w.view().topology==quad,"Gesture cancellation restores the exact topology snapshot");
  w.edit(RetopoAction::Mode);point(w,{-.2f,-.2f,0});w.edit(RetopoAction::Pin);
  check(w.view().topology->vertices().begin()->second.pinned,"Pin command persists on selected stable vertices");
  w.edit(RetopoAction::Mode);w.edit(RetopoAction::BeginGesture);
  check(rejects([&]{w.edit(RetopoAction::Sample,{-.2f,-.2f,0});}),"Pinned points refuse a movement gesture");
  w.edit(RetopoAction::CancelGesture);w.edit(RetopoAction::Mode);w.edit(RetopoAction::Clear);
  point(w,{-.2f,-.2f,0});point(w,{-.2f,.2f,0});w.edit(RetopoAction::Seam);
  check(std::count_if(w.view().topology->edges().begin(),w.view().topology->edges().end(),[](auto &e){return e.second.seam;})==1,
        "Selected endpoint pair marks the actual face boundary seam");
  auto seam=w.view().topology;
  check(rejects([&]{w.edit(RetopoAction::Relax,{},50,[]{return true;});})&&w.view().topology==seam,
        "Cancelled heavy operation cannot publish a partial topology");
  w.edit(RetopoAction::Delete);
  check(w.view().topology->faces().empty()&&w.view().topology->vertices().size()==2,"Delete atomically removes selected points with incident topology");
  w.edit(RetopoAction::Undo);check(w.view().topology==seam,"Deletion Undo retains IDs, allocation cursor, pins and seams");
  w.edit(RetopoAction::BeginGesture);auto miss=w.view().topology;
  check(rejects([&]{w.edit(RetopoAction::Sample,{0,0,.2f});})&&w.view().topology==miss,"Out-of-range projection refuses the point without changing topology");
  w.edit(RetopoAction::CancelGesture);

  Document doc;doc.replace(scene.snapshot());doc.begin_retopology();
  check(doc.wait_retopology()&&doc.frame().state->retopology->source,"Reference BVH initializes off the interaction thread");
  point(doc,{-.2f,-.2f,.02f});point(doc,{.2f,-.2f,.02f});point(doc,{.2f,.2f,.02f});point(doc,{-.2f,.2f,.02f});
  check(doc.copy_scene().encode()==original&&doc.frame().state->dirty,"Desktop drafts are visibly dirty without mutating source scene or global history");
  check(doc.undo()&&doc.wait_retopology()&&doc.frame().state->retopology->topology->vertices().size()==3,"Universal Undo routes to the retopology draft");
  check(doc.redo()&&doc.wait_retopology()&&doc.frame().state->retopology->selected.size()==4,"Universal Redo restores the ordered draft selection");
  check(rejects([&]{doc.select_object(1);}),"Unconfirmed retopology cannot be silently discarded by a source-changing command");
  doc.edit_retopology(RetopoAction::Face);check(doc.wait_retopology()&&doc.frame().state->error.empty(),"Worker validates desktop quad creation");
  auto native=doc.frame().state->retopology->topology->encode();
  doc.end_retopology(true);
  check(doc.wait_retopology()&&doc.wait_asset()&&!doc.frame().state->retopology&&doc.frame().state->error.empty(),"Apply completes guarded atlas/publication and closes the accepted draft");
  auto accepted=doc.copy_scene();auto target=accepted.active().id;
  check(accepted.objects().size()==2&&!accepted.object(1).visible&&accepted.object(1).painted->encode()==scene.object(1).painted->encode()&&
        accepted.active().retopology->topology.encode()==native&&accepted.active().mesh_active,
        "Apply retains the exact original and creates a separate atlas-ready stable-topology mesh");
  auto json=accepted.encode();auto decoded=Scene::decode(json);
  check(json.at("version")==7&&decoded.encode()==json,"Scene version seven round-trips stable topology, source and derived display mesh");
  check(doc.undo()&&doc.copy_scene().encode()==original,"Exactly one global Undo removes the complete accepted retopology operation");
  check(doc.redo()&&doc.copy_scene().encode()==json,"Global Redo restores the exact source/target scene");
  doc.begin_retopology();check(doc.wait_retopology()&&doc.frame().state->retopology->topology->encode()==native,"Reopen recovers persistent editable quads rather than triangulated output");
  doc.end_retopology(false);check(doc.copy_scene().encode()==json,"Cancel reopened patch leaves authored target and source exact");
  auto bad=json;bad["objects"][1]["retopology"]["source"]=target;
  check(rejects([&]{Scene::decode(bad);}),"Native decoder refuses self-referential retopology sources");
  auto copy=accepted.duplicate(target);check(accepted.object(copy).retopology->topology.encode()==native,"Object duplication retains immutable editable topology");
  auto detached=decoded.snapshot();detached.erase(1);Document orphan;orphan.replace(detached);
  check(rejects([&]{orphan.begin_retopology();})&&!orphan.frame().state->retopology,
        "Deleted source produces a clean reopen refusal without a stranded draft");

  Document cancelled;cancelled.replace(scene.snapshot());cancelled.begin_retopology();check(cancelled.wait_retopology(),"Cancellation fixture initializes");
  check(cancelled.begin_desktop(),"Held placement acquires desktop ownership");Brush b;b.from=b.to={0,0,.01f};cancelled.stamp_desktop(b);
  check(cancelled.wait_retopology()&&cancelled.frame().state->retopology->topology->vertices().size()==1,"Held gesture publishes actual topology before release");
  cancelled.cancel_desktop();check(cancelled.wait_retopology()&&cancelled.frame().state->retopology->topology->vertices().empty(),"Focus-loss cancellation restores an unconfirmed gesture exactly");
  point(cancelled,{0,0,0});cancelled.replace(scene.snapshot());
  check(cancelled.wait_retopology()&&!cancelled.frame().state->retopology&&cancelled.copy_scene().encode()==original,"Document replacement invalidates late workspace publications");

  Document xr;xr.replace(scene.snapshot());Controller hand(xr);
  hand.send(0,0,0,true);hand.send();hand.sector(8);hand.sector(9);hand.sector(11);
  check(xr.frame().state->menu_page==14,"Joystick navigation opens the retopology workflow from mesh tools");
  hand.sector(0);check(xr.wait_retopology()&&xr.frame().state->retopology,"Controller menu creates the same native worker workspace");
  for(auto p:std::array<Vec3,3>{{{-.2f,-.2f,.01f},{.2f,-.2f,.01f},{0,.2f,.01f}}}) {
    hand.f.hands[1].tracked.grip.position=ob::transform(xr.frame().state->canvas,{p.x,p.y,p.z});
    hand.send(.7f);hand.send();check(xr.wait_retopology()&&xr.frame().state->error.empty(),"Controller pressure places a source-constrained point");
  }
  hand.send(0,0,0,true);hand.send();hand.sector(2);check(xr.wait_retopology()&&xr.frame().state->retopology->topology->faces().size()==1,
        "Joystick Make face builds the actual selected triangle");
  hand.send(0,0,0,true);hand.send();hand.sector(9);
  check(xr.frame().state->controls.parameter==VrParameter::RetopoDistance,"XR projection distance uses the common controller slider panel");
  hand.send(0,1);hand.send(0,0,0,true);hand.send();
  check(xr.frame().state->retopology&&xr.frame().state->retopology->snap_mm>=50,"Confirming the XR distance control preserves the draft");
  // Dismiss the root menu if the control returns to its parent, then reopen.
  if (!xr.frame().state->menu) { hand.send(0,0,0,true);hand.send(); }
  hand.sector(10);check(xr.wait_retopology()&&xr.wait_asset()&&xr.copy_scene().objects().size()==2&&xr.frame().state->error.empty(),
        "Controller-only workflow applies an atlas-ready native mesh with undo history");
  std::cout<<checks<<" checks passed\n";
} catch(const std::exception &e) {std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
