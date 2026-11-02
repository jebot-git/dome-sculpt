// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/import.hpp"
#include <unistd.h>
#include <nlohmann/json.hpp>
#include <iostream>
using namespace dome;
namespace {
size_t checks=0;
void require(bool value,const char *message){++checks;if(!value)throw std::runtime_error(message);}
template<class F> bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}
TopologyID edge(const RetopoMesh &mesh,TopologyID a,TopologyID b){for(auto &[id,e]:mesh.edges())if((e.a==a&&e.b==b)||(e.a==b&&e.b==a))return id;throw std::runtime_error("Test edge absent");}
struct Pair {RetopoMesh mesh;std::array<TopologyID,4> left,right;};
Pair patches(){
 Pair p;for(auto [target,x]:{std::pair{&p.left,-.3f},std::pair{&p.right,.1f}}){
  *target={p.mesh.add_vertex({{x,-.1f,0},{.1f,.2f},true}),p.mesh.add_vertex({{x+.2f,-.1f,0}}),p.mesh.add_vertex({{x+.2f,.1f,0}}),p.mesh.add_vertex({{x,.1f,0}})};
  p.mesh.add_face(*target,target==&p.left?5:9);
 }
 return p;
}
Scene reference(const RetopoMesh &topology){
 Mesh m;m.positions={{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};
 Scene s;s.active().painted=std::make_shared<PaintedMesh>(m,64);s.active().mesh_active=true;publish_retopology(s,topology,s.active().id,{});return s;
}
void point(RetopoWorkspace &w,Vec3 p){w.edit(RetopoAction::BeginGesture);w.edit(RetopoAction::Sample,p);w.edit(RetopoAction::EndGesture);}
void settle(Document &d){require(d.wait_retopology(),"Retopology worker drains");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
void point(Document &d,Vec3 p){require(d.begin_desktop(),"Native retopo gesture begins");Brush b;b.from=b.to=p;b.radius=.01f;d.stamp_desktop(b);d.end_desktop();settle(d);}
struct Controller {
 Document &doc;SpatialFrame f;
 explicit Controller(Document &d):doc(d){f.focused=f.head_valid=true;f.head.position={0,1.6f,0};f.hands[1].tracked.valid=true;send();}
 void send(float pressure=0,float x=0,float y=0,bool click=false){++f.sequence;f.time+=.02;auto &h=f.hands[1];h.tracked.trigger=pressure;h.x=x;h.y=y;h.joystick=click;doc.submit(f);require(doc.wait_input(),"Controller input drains");}
 void sector(int index){float a=index*6.283185307f/12;send(0,std::cos(a),std::sin(a));send(0,std::cos(a),std::sin(a),true);send();}
 void position(Vec3 p){f.hands[1].tracked.grip.position=ob::transform(doc.frame().state->canvas,{p.x,p.y,p.z});}
 void point(Vec3 p){position(p);send(.7f);send();settle(doc);}
 void menu(){send(0,0,0,true);send();require(doc.frame().state->menu&&doc.frame().state->menu_page==16,"Controller reopens patch page");}
};
}
int main(int argc,char **argv)try{
 if(argc==3&&std::string(argv[1])=="--ui-fixture"){reference(patches().mesh).save(argv[2]);return 0;}
 auto p=patches();const auto before=p.mesh.encode();auto boundary=edge(p.mesh,p.left[1],p.left[2]);p.mesh.set_seam(boundary,true);const auto seamed=p.mesh.encode();
 auto face=p.mesh.bridge_edges({p.left[2],p.left[1],p.right[0],p.right[3]});
 require(p.mesh.faces().size()==3&&p.mesh.faces().at(face).material==5,"Bridge creates outward quad inheriting first material");
 require(p.mesh.vertices().size()==8&&p.mesh.edges().at(boundary).seam&&p.mesh.vertices().at(p.left[0]).pinned,"Bridge preserves old vertices/pins/seams");
 require(RetopoMesh::decode(p.mesh.encode()).encode()==p.mesh.encode(),"Bridge stable topology survives native codec exactly");
 const auto bridged=p.mesh.encode();
 require(refuses([&]{p.mesh.bridge_edges({p.left[2],p.left[1],p.right[0],p.right[3]});})&&p.mesh.encode()==bridged,"Interior/duplicate bridge refusal preserves complete state and cursor");
 require(refuses([&]{p.mesh.bridge_edges({p.left[0],p.left[1],p.left[1],p.left[2]});})&&p.mesh.encode()==bridged,"Shared endpoints cannot masquerade as two disjoint edges");
 auto q=patches();q.mesh.set_seam(edge(q.mesh,q.left[0],q.left[1]),true);const auto original=q.mesh.encode();
 const auto e=q.mesh.edges().at(edge(q.mesh,q.left[0],q.left[1]));auto a=q.mesh.vertices().at(e.a).position,b=q.mesh.vertices().at(e.b).position;
 auto result=q.mesh.extrude_edge(edge(q.mesh,e.a,e.b),{a+Vec3{0,-.1f,0},b+Vec3{0,-.1f,0}});
 require(q.mesh.faces().size()==3&&q.mesh.vertices().size()==10&&q.mesh.faces().at(result.face).material==5,"Extrusion adds exactly two vertices and one material-preserving quad");
 require(q.mesh.edges().at(result.edge).seam&&!q.mesh.vertices().at(result.outer[0]).pinned&&!q.mesh.vertices().at(result.outer[1]).pinned,"Outer seam follows boundary while new points are unpinned");
 require(q.mesh.vertices().at(q.left[0]).uv==std::array<float,2>{.1f,.2f}&&q.mesh.vertices().at(q.left[0]).pinned,"Pinned original endpoint and UV remain exact");
 const auto extruded=q.mesh.encode();require(refuses([&]{q.mesh.extrude_edge(edge(q.mesh,e.a,e.b),{a,b});})&&q.mesh.encode()==extruded,"Interior edge cannot be extruded again");
 for(unsigned at:{1u,2u,4u,8u,16u}){
  auto copy=patches();auto snapshot=copy.mesh.encode();unsigned calls=0;
  const bool rejected=refuses([&]{copy.mesh.extrude_edge(edge(copy.mesh,copy.left[0],copy.left[1]),{Vec3{-.3f,-.2f,0},Vec3{-.1f,-.2f,0}},[&]{return ++calls>=at;});});
  require(!rejected||copy.mesh.encode()==snapshot,"Cancelled extrusion never consumes IDs or partial attributes");
 }
 auto source=reference(patches().mesh);const auto source_bytes=source.encode();RetopoWorkspace w(source);
 point(w,{-.1f,-.1f,0});point(w,{-.1f,.1f,0});point(w,{.1f,.1f,0});point(w,{.1f,-.1f,0});
 auto selection=w.view();w.edit(RetopoAction::BridgeEdges);auto bridge=w.view();
 require(bridge.topology->faces().size()==3&&bridge.selected.empty()&&source.encode()==source_bytes,"Workspace chooses valid outward bridge from either endpoint order");
 w.edit(RetopoAction::Undo);require(w.view().topology==selection.topology&&w.view().selected==selection.selected,"One bridge Undo restores selection and exact snapshot");
 w.edit(RetopoAction::Redo);require(w.view().topology==bridge.topology,"Bridge Redo reuses exact immutable result");
 w.edit(RetopoAction::Clear);point(w,{-.3f,-.1f,0});point(w,{-.1f,-.1f,0});w.edit(RetopoAction::ExtrudeMode);const auto base=w.view();
 w.edit(RetopoAction::BeginGesture);w.edit(RetopoAction::Sample,{-.2f,-.1f,0});w.edit(RetopoAction::EndGesture);
 require(w.view().topology==base.topology,"Extrude click without drag is exact no-op");
 w.edit(RetopoAction::BeginGesture);require(refuses([&]{w.edit(RetopoAction::Sample,{.6f,.6f,0});})&&w.view().topology==base.topology,"Extrusion must start on the actual selected edge");w.edit(RetopoAction::CancelGesture);
 w.edit(RetopoAction::BeginGesture);w.edit(RetopoAction::Sample,{-.2f,-.1f,0});w.edit(RetopoAction::Sample,{-.2f,-.2f,0});auto preview=w.view();
 require(preview.topology->vertices().size()==10&&preview.selected.size()==2,"Dragged extrusion publishes only a source-projected immutable preview");
 w.edit(RetopoAction::Sample,{-.2f,-.25f,0});require(w.view().selected==preview.selected&&w.view().topology->vertices().size()==10,"Continuous drag retains new stable IDs without repeated strips");
 w.edit(RetopoAction::Sample,{-.2f,-.1f,0});require(w.view().topology==base.topology&&w.view().selected==base.selected,"Returning to anchor restores exact original snapshot and selection");
 w.edit(RetopoAction::Sample,{-.2f,-.2f,0});w.edit(RetopoAction::CancelGesture);require(w.view().topology==base.topology&&w.view().selected==base.selected,"Cancelled extrusion retains cursor/source/history endpoint");
 w.edit(RetopoAction::BeginGesture);w.edit(RetopoAction::Sample,{-.2f,-.1f,0});w.edit(RetopoAction::Sample,{-.2f,-.2f,0});w.edit(RetopoAction::EndGesture);auto accepted=w.view();
 w.edit(RetopoAction::Undo);require(w.view().topology==base.topology,"Entire extrusion drag is one local Undo");w.edit(RetopoAction::Redo);require(w.view().topology==accepted.topology,"Extrusion Redo restores exact IDs and attributes");
 Document doc;doc.replace(source.snapshot());doc.begin_retopology();settle(doc);
 point(doc,{-.3f,-.1f,0});point(doc,{-.1f,-.1f,0});doc.edit_retopology(RetopoAction::ExtrudeMode);settle(doc);
 require(doc.begin_desktop(),"Desktop extrusion begins");Brush stroke;stroke.from=stroke.to={-.2f,-.1f,0};doc.stamp_desktop(stroke);stroke.from=stroke.to={-.2f,-.2f,0};doc.stamp_desktop(stroke);doc.end_desktop();settle(doc);
 require(doc.frame().state->retopology->topology->vertices().size()==10&&doc.copy_scene().encode()==source_bytes,"Actual worker gesture edits detached patch only");
 require(doc.undo(),"Universal Undo admits extrusion");settle(doc);require(doc.frame().state->retopology->topology->vertices().size()==8,"Universal Undo removes exactly one extrusion");
 require(doc.redo(),"Universal Redo admits extrusion");settle(doc);require(doc.frame().state->retopology->topology->vertices().size()==10,"Universal Redo restores extrusion");

 const auto native=doc.frame().state->retopology->topology->encode();doc.end_retopology(true);
 require(doc.wait_asset()&&!doc.frame().state->retopology&&doc.frame().state->error.empty(),"Extrusion Apply completes ordered atlas/publication");
 auto applied=doc.copy_scene().encode();require(doc.copy_scene().active().retopology->topology.encode()==native,"Apply retains editable extrusion IDs and attributes");
 auto directory=std::filesystem::temp_directory_path()/("dome-retopo-edges-"+std::to_string(getpid()));std::filesystem::create_directories(directory);
 doc.save(directory/"strip.dsculpt");require(Scene::load(directory/"strip.dsculpt").encode()==applied,"Real native save/reopen preserves entire extruded scene");
 doc.export_asset(directory/"strip.glb").get();auto emitted=import_mesh(directory/"strip.glb");
 require(emitted.primitives.size()==1&&emitted.primitives.front().mesh.indices.size()==18,"Actual exported GLB carries all three quads as six triangles");
 require(doc.undo()&&doc.copy_scene().encode()==source_bytes,"One global Undo restores exact pre-Apply source and target");
 require(doc.redo()&&doc.copy_scene().encode()==applied,"Global Redo restores editable strip and atlas exactly");
 doc.begin_retopology();settle(doc);require(doc.frame().state->retopology->topology->encode()==native,"Reopened workspace retains stable extrusion topology");doc.end_retopology(false);
 std::filesystem::remove_all(directory);
 Document xr;xr.replace(source.snapshot());Controller hand(xr);xr.begin_retopology();settle(xr);hand.send();hand.send(0,0,0,true);hand.send();hand.sector(8);hand.sector(9);hand.sector(11);require(xr.frame().state->menu_page==16,"Controller navigates to Retopology page");hand.sector(0);
 for(auto p:std::array<Vec3,4>{{{-.1f,-.1f,0},{-.1f,.1f,0},{.1f,.1f,0},{.1f,-.1f,0}}})hand.point(p);
 hand.menu();require(xr.frame().state->menu_enabled[3],"Four selected endpoints enable contextual bridge on controller page");hand.sector(3);settle(xr);
 require(xr.frame().state->retopology->topology->faces().size()==3,"Actual joystick Connect/Bridge action builds outward bridge");
 if(xr.frame().state->menu)hand.sector(0);
 hand.point({-.3f,-.1f,0});hand.point({-.1f,-.1f,0});
 hand.menu();hand.sector(1);settle(xr);require(xr.frame().state->retopology->moving,"First controller mode change selects Move");
 hand.menu();hand.sector(1);settle(xr);require(xr.frame().state->retopology->extruding&&!xr.frame().state->retopology->moving,"Second controller mode change selects Extrude on boundary pair");
 hand.position({-.2f,-.1f,0});hand.send(.7f);settle(xr);hand.position({-.2f,-.2f,0});hand.send(.7f);hand.send();settle(xr);
 require(xr.frame().state->retopology->topology->faces().size()==4&&xr.frame().state->retopology->topology->vertices().size()==10,"Actual tracked controller drag adds one projected strip");
 require(xr.copy_scene().encode()==source_bytes,"Controller bridge/extrusion drafts preserve committed reference scene");
 hand.menu();hand.sector(10);xr.finish_spatial();require(xr.wait_asset()&&!xr.frame().state->retopology&&xr.frame().state->error.empty(),"Accepted controller strip Apply survives input shutdown");
 require(xr.copy_scene().active().retopology->topology.faces().size()==4,"Controller result retains bridge plus extrusion as editable quads");
 std::cout<<"PASS "<<checks<<" bridge/extrusion checks\n";return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
