// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "polygon-fixture.hpp"
#include "dome/polygon-scene.hpp"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <iostream>
#include <unistd.h>
#include <pthread.h>
#include <cstring>
#include <cstdlib>
#include <new>
#include <condition_variable>
using namespace dome;
namespace {
std::atomic_bool gate_enabled=false,gate_entered=false,fail_after_gate=false;
std::mutex gate_mutex;std::condition_variable gate_changed;bool gate_released=false;
void allocation_gate(size_t size){
 if(!gate_enabled||size!=sizeof(PolygonWorkspace))return;
 char name[16]{};pthread_getname_np(pthread_self(),name,sizeof(name));
 if(std::strcmp(name,"dome-polygon")||!gate_enabled.exchange(false))return;
 {std::unique_lock lock(gate_mutex);gate_entered=true;gate_changed.notify_all();gate_changed.wait(lock,[]{return gate_released;});}
 if(fail_after_gate.exchange(false))throw std::bad_alloc();
}
struct WorkerGate {
 explicit WorkerGate(bool fail=false){std::lock_guard lock(gate_mutex);gate_released=false;gate_entered=false;fail_after_gate=fail;gate_enabled=true;}
 void wait(){std::unique_lock lock(gate_mutex);if(!gate_changed.wait_for(lock,std::chrono::seconds(10),[]{return gate_entered.load();}))throw std::runtime_error("Polygon worker did not reach detached workspace allocation");}
 void release(){std::lock_guard lock(gate_mutex);gate_enabled=false;gate_released=true;gate_changed.notify_all();}
 ~WorkerGate(){release();}
};
}
[[gnu::noinline]] void *operator new(size_t n){allocation_gate(n);if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](size_t n){return ::operator new(n);}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p,size_t)noexcept{std::free(p);}
namespace dome {struct DocumentNativeTestAccess {
 static bool grab_xr(Document &d,uint64_t id){std::lock_guard lock(d.model_mutex_);return d.begin_object_grab_locked(id);}
 static auto ledger(Document &d){std::lock_guard lock(d.model_mutex_);return std::tuple{d.undo_.size(),d.redo_.size(),d.content_,d.next_content_,d.state_.revision};}
};}
namespace {
unsigned checks=0;
void check(bool value,const char *message){if(!value)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<std::endl;}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}
Scene fixture(){return polygon_test::fixture();}

void ready(Document &d){check(d.wait_polygon(),"Ordered polygon worker reaches ready state");auto e=d.frame().state->error;if(!e.empty())throw std::runtime_error(e);}
void picked(Document &d){d.polygon_pointer({0,0,3},{0,0,-1},PolygonAction::Pick);ready(d);check(d.frame().state->polygon->selection->ids.size()==1,"Real object-local ray selects one visible N-gon");}
void extrude(Document &d){picked(d);d.edit_polygon(PolygonAction::Extrude);ready(d);}
}
int main(int argc,char **argv)try{
 if(argc==3&&std::string_view(argv[1])=="--sculpt-fixture"){Document d;d.seed();d.save(argv[2]);return 0;}
 if(argc==3&&std::string_view(argv[1])=="--inspect"){std::cout<<Scene::load(argv[2]).encode().dump()<<'\n';return 0;}
 if(argc==3&&(std::string_view(argv[1])=="--ui-fixture"||std::string_view(argv[1])=="--xr-fixture")){auto scene=fixture();if(std::string_view(argv[1])=="--xr-fixture")scene.active().transform.scale=.35f;scene.save(argv[2]);return 0;}
 auto directory=std::filesystem::temp_directory_path()/("dome-polygon-doc-"+std::to_string(getpid()));std::filesystem::create_directories(directory);
 Document d;d.replace(fixture());check(d.wait_mesh(),"Committed source surface is ready");auto original=d.copy_scene().encode();auto initial=d.frame().state;
 d.begin_polygon();ready(d);check(d.frame().state->polygon->topology->faces().size()==6,"Native workspace retains six real quads instead of its twelve display triangles");
 check(!d.begin_native_stroke(),"Paint/sculpt gestures cannot mutate through an active polygon workspace");
 picked(d);check(d.copy_scene().encode()==original&&!d.frame().state->dirty,"Component selection leaves committed source and history exact");
 d.save(directory/"unconfirmed.dms");check(Scene::load(directory/"unconfirmed.dms").encode()==original,"Saving an unconfirmed polygon workspace saves the exact committed source");
 d.edit_polygon(PolygonAction::Extrude);ready(d);check(d.frame().state->polygon->topology->faces().size()==10&&d.frame().state->polygon->changed,"Actual worker extrusion prepares four real side faces");
 check(d.undo(),"Global Undo routes into polygon local history");ready(d);check(!d.frame().state->polygon->changed&&d.copy_scene().encode()==original,"Local Undo restores original N-gons without global publication");
 check(d.redo(),"Global Redo routes into polygon local history");ready(d);
 d.end_polygon(false);check(!d.frame().state->polygon&&d.copy_scene().encode()==original&&!d.frame().state->can_undo,"Cancel discards all draft topology and global history remains exact");
 d.begin_polygon();ready(d);d.end_polygon(true);ready(d);check(d.frame().state->revision==initial->revision&&!d.frame().state->can_undo,"Unchanged Apply closes workspace without advancing revision or history");
 d.begin_polygon();ready(d);extrude(d);d.end_polygon(true);auto saved=d.save_async(directory/"accepted.dms");saved.get();ready(d);check(d.wait_mesh(),"Accepted immutable surface is ready");auto accepted=d.copy_scene().encode();
 check(!d.frame().state->polygon&&d.frame().state->can_undo&&d.copy_scene().active().polygon->topology.faces().size()==10,"Accepted topology publishes once through native transaction");
 check(Scene::load(directory/"accepted.dms").encode()==accepted,"Immediate accepted Apply then Save captures the exact completed Scene12 endpoint");
 check(refuses([&]{d.prepare_mesh(64);})&&d.copy_scene().encode()==accepted,"Generic sculpt-mesh rebuilding cannot silently replace an active polygon derivative");
 check(d.undo(),"Accepted topology has a global Undo");check(d.copy_scene().encode()==original&&!d.frame().state->can_undo,"Exactly one global Undo restores full original source and paint");
 check(d.redo()&&d.copy_scene().encode()==accepted,"Global Redo restores exact accepted stable-ID authority and derivative");
 d.begin_polygon();ready(d);d.edit_polygon(PolygonAction::SelectAll);ready(d);d.edit_polygon(PolygonAction::DeleteFaces);ready(d);d.end_polygon(true);ready(d);check(d.copy_scene().active().polygon->topology.faces().empty()&&!d.copy_scene().active().mesh_active&&!d.copy_scene().active().painted,"Delete-all publishes active empty authority without stale paint or Volume fallback");
 check(!d.begin_native_stroke()&&!d.frame().state->can_configure_sculpt&&!d.raycast({0,0,3},{0,0,-1}),"Empty polygon authority cannot edit or ray-pick its real retained sculpt/masks behind the empty surface");
 d.save(directory/"empty.dms");auto empty=Scene::load(directory/"empty.dms");check(empty.surfaces().empty()&&empty.active().polygon->origin->paint,"Empty Scene12 restart retains the latest complete source while rendering no stale surface");
 d.restore_polygon_source();check(d.wait_asset(),"Retained source restore worker finishes");check(d.frame().state->error.empty()&&d.copy_scene().encode()==accepted,"Explicit source Restore recovers prior accepted N-gons and complete paint");
 d.replace(fixture());check(d.wait_mesh(),"Held source surface is ready");d.configure_object_grab(true,true);d.hold_object(1);d.begin_polygon();ready(d);
 check(DocumentNativeTestAccess::grab_xr(d,1),"The same object can be physically held while its polygon workspace is open");auto pose=d.copy_scene().active().transform;pose.position.x=.2f;d.move_held_object(pose);
 d.polygon_pointer({.2f,0,3},{0,0,-1},PolygonAction::Pick);ready(d);check(d.frame().state->polygon->selection->ids.size()==1,"Polygon ray uses effective held pose instead of stale packed world transform");
 d.edit_polygon(PolygonAction::Extrude);ready(d);
 {WorkerGate gate;d.end_polygon(true);gate.wait();check(refuses([&]{auto moved=pose;moved.position.x=.3f;d.move_held_object(moved);}),"Accepted geometry freezes captured held pose until publication");
  SpatialFrame loss;loss.sequence=1;loss.time=1;d.submit(loss);check(d.wait_input(),"Real focus-loss frame drains while accepted worker is blocked");
  check(d.frame().state->object_grabbing&&d.frame().state->polygon->committing,"Tracking loss retains accepted captured-pose authority rather than discarding its edit");gate.release();}
 ready(d);check(d.copy_scene().active().transform.position.x==.2f&&!d.frame().state->object_grabbing,"One transaction publishes changed topology together with captured held pose");
 check(d.undo()&&d.copy_scene().active().transform.position.x==0&&d.copy_scene().active().polygon->topology.faces().size()==6,"One Undo restores both original pose and topology after simultaneous held editing");
 // An explicit unhold invalidates accepted geometry and its dependent Save,
 // without accepting the frozen motion or consuming global Undo.
 d.replace(fixture());check(d.wait_mesh(),"Explicit unhold source surface ready");d.configure_object_grab(true,true);d.hold_object(1);d.begin_polygon();ready(d);extrude(d);
 check(DocumentNativeTestAccess::grab_xr(d,1),"Second held draft starts with exact source lease");d.move_held_object(pose);auto before_unhold=d.copy_scene().encode();
 {WorkerGate gate;d.end_polygon(true);gate.wait();auto dependent=d.save_async(directory/"cancelled.dms");d.release_held_object(true);
  check(!d.frame().state->held_object&&!d.frame().state->polygon&&d.copy_scene().encode()==before_unhold&&!d.frame().state->can_undo,"Explicit unhold cancels accepted topology and captured pose without a transform history record");
  gate.release();check(refuses([&]{dependent.get();})&&!std::filesystem::exists(directory/"cancelled.dms"),"Dependent Save refuses a cancelled accepted token and never writes an old endpoint");}
 // A failure after pose capture leaves all committed source authority exact.
 d.replace(fixture());check(d.wait_mesh(),"Failure source surface ready");d.configure_object_grab(true,true);d.hold_object(1);d.begin_polygon();ready(d);extrude(d);
 check(DocumentNativeTestAccess::grab_xr(d,1),"Failure draft starts before pose capture");d.move_held_object(pose);auto before_failure=d.copy_scene().encode();auto before_ledger=DocumentNativeTestAccess::ledger(d);
 {WorkerGate gate(true);d.end_polygon(true);gate.wait();auto dependent=d.save_async(directory/"failed.dms");gate.release();check(d.wait_polygon(),"Failed detached worker returns a usable draft/status");check(refuses([&]{dependent.get();})&&!std::filesystem::exists(directory/"failed.dms"),"Allocation failure after pose capture refuses its dependent Save");}
 check(d.copy_scene().encode()==before_failure&&DocumentNativeTestAccess::ledger(d)==before_ledger&&!d.frame().state->object_grabbing&&d.frame().state->polygon&&d.frame().state->polygon->changed&&d.frame().state->polygon->can_undo&&d.frame().state->can_undo,"Owned allocation refusal retains exact committed source/history, restores original pose, and retains cancellable local geometry");
 d.end_polygon(false);check(d.copy_scene().encode()==before_failure&&!d.frame().state->can_undo&&DocumentNativeTestAccess::ledger(d)==before_ledger,"Failed draft remains exactly cancellable with no global history/content identity consumed");
 // The actual default application path starts with a sculpt, not an already
 // serialized polygon fixture. Atlas/raster preparation must remain detached.
 Scene sculpt_source;sculpt_source.active().volume=std::make_shared<Volume>(fixture().active().volume->geometry_snapshot());d.replace(sculpt_source);check(d.wait_mesh(),"Real retained sculpt surface ready for native polygon entry");auto sculpt_original=d.copy_scene().encode();
 d.begin_polygon();ready(d);auto initial_faces=d.frame().state->polygon->topology->faces().size();
 check(initial_faces>20&&d.copy_scene().encode()==sculpt_original,"Native sculpt entry prepares genuine stable-ID editable topology without changing committed Volume");
 d.polygon_pointer({.023f,.017f,3},{0,0,-1},PolygonAction::Pick);ready(d);check(d.frame().state->polygon->selection->ids.size()==1,"Actual sculpt-derived face is ray-selectable");d.edit_polygon(PolygonAction::Extrude);ready(d);d.end_polygon(true);ready(d);
 check(d.copy_scene().active().mesh_active&&d.copy_scene().active().polygon&&d.copy_scene().active().polygon->topology.faces().size()==initial_faces+3,"Sculpt-derived triangle extrusion publishes real side faces, editable authority and a bounded painting atlas");
 d.save(directory/"from-sculpt.dms");auto sculpt_converted=d.copy_scene().encode();check(Scene::load(directory/"from-sculpt.dms").encode()==sculpt_converted,"Actual sculpt-to-polygon native path restarts with exact topology, atlas and full source layers/masks");
 check(d.undo()&&d.copy_scene().encode()==sculpt_original&&!d.frame().state->can_undo,"One Undo restores the complete original sculpt after atlas-backed topology conversion");check(d.redo(),"Sculpt conversion can be restored by Redo");d.restore_polygon_source();check(d.wait_asset()&&d.frame().state->error.empty()&&d.copy_scene().encode()==sculpt_original,"Retained source Restore returns exactly to original sculpt authoring and minimal native schema");
 d.seed();check(d.wait_mesh(),"Application default 30mm sculpt seed is ready");auto default_original=d.copy_scene().encode();d.begin_polygon();ready(d);auto default_faces=d.frame().state->polygon->topology->faces().size();
 d.polygon_pointer({.013f,.017f,3},{0,0,-1},PolygonAction::Pick);ready(d);check(d.frame().state->polygon->selection->ids.size()==1,"Default application sculpt enters real component selection");d.edit_polygon(PolygonAction::Extrude);ready(d);d.end_polygon(true);ready(d);
 check(d.copy_scene().active().polygon->topology.faces().size()==default_faces+3,"Default sculpt-to-polygon path accepts a genuine topology edit through bounded atlas preparation");d.save(directory/"default-converted.dms");check(Scene::load(directory/"default-converted.dms").encode()==d.copy_scene().encode(),"Default application sculpt edit saves and reopens exact native polygon state");d.restore_polygon_source();check(d.wait_asset()&&d.frame().state->error.empty()&&d.copy_scene().encode()==default_original,"Default sculpt's full original Volume remains exactly restorable after restartable topology conversion");
 // Explicit conversion edits the evaluated rigid/morphed visible shape,
 // and retains the complete original rest/skin/morph/pose source for Restore.
 auto rigged=fixture();const auto &rest=rigged.active().painted->mesh();DeformationState deform(rest);
 deform.skeleton=Skeleton{{{"Root",-1,{},.4f}}};SkinWeights skin;for(size_t i=0;i<rest.positions.size();++i)skin.vertices.push_back({{0,1}});deform.skin=std::move(skin);
 deform.pose={deform.skeleton->bones[0].rest};deform.pose[0].translation.x=.1f;
 deform.morphs=MorphSet(rest);auto expanded=rest.positions;for(auto &v:expanded)v=v*1.1f;deform.morphs->set_target(rest,"Uniform fit",expanded);deform.morph_weights={1};rigged.set_deformation(1,deform);
 d.replace(rigged);check(d.wait_mesh(),"Rigged/morphed source surface ready");auto rigged_original=d.copy_scene().encode();
 check(refuses([&]{d.begin_polygon(false);})&&d.copy_scene().encode()==rigged_original,"Original rig/morph source requires explicit retained-source conversion before editable topology");
 d.begin_polygon(true);ready(d);auto visible=d.frame().state->polygon->topology;
 float minimum=1e9f,maximum=-1e9f;for(auto &v:visible->vertices()){minimum=std::min(minimum,v.position.x);maximum=std::max(maximum,v.position.x);}
 check(std::abs(minimum+.45f)<1e-6f&&std::abs(maximum-.65f)<1e-6f,"Conversion uses independently expected active morph scale and skin pose rather than hidden rest positions");
 d.polygon_pointer({.1f,0,3},{0,0,-1},PolygonAction::Pick);ready(d);d.edit_polygon(PolygonAction::Extrude);ready(d);d.end_polygon(true);ready(d);
 auto converted=d.copy_scene();check(!converted.active().deformation&&converted.active().polygon->origin->deformation&&converted.active().polygon->origin->deformation->morph_weights==std::vector<float>{1},"Accepted explicit conversion retains full original skin/morph/pose authority while editing canonical visible geometry");
 d.save(directory/"converted.dms");auto resumed=Scene::load(directory/"converted.dms");check(resumed.encode()==converted.encode(),"Scene12 restart keeps canonical converted topology plus complete original deformation capsule");
 check(d.undo()&&d.copy_scene().encode()==rigged_original&&!d.frame().state->can_undo,"Explicit rig/morph conversion plus topology is one exact global Undo");
 check(d.redo(),"Converted visible geometry can be restored by Redo");d.restore_polygon_source();check(d.wait_asset()&&d.frame().state->error.empty()&&d.copy_scene().encode()==rigged_original,"Source Restore recovers exact original rest, skin, morphs, pose and all paint/sculpt metadata");
 std::filesystem::remove_all(directory);std::cout<<"PASS "<<checks<<" polygon Document checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
