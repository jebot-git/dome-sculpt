// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/source-placement.hpp"
#include "import-authoring-fixtures.hpp"
#include "import-controller-driver.hpp"
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
size_t checks=0;std::atomic_bool gated=false,entered=false,released=false;std::mutex gate_mutex;std::condition_variable changed;
void check(bool ok,const char*m){if(!ok)throw std::runtime_error(m);++checks;}
template<class F>void await(F f,const char*m){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(8);while(!f()){if(std::chrono::steady_clock::now()>end)throw std::runtime_error(m);std::this_thread::sleep_for(std::chrono::milliseconds(2));}}
void ready(Document&d){check(d.wait_asset()&&d.wait_native_stroke()&&d.wait_mesh(),"Actual worker/source/mesh endpoint settles");if(!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
}
extern "C" ssize_t __real_read(int,void*,size_t);
extern "C" ssize_t __wrap_read(int fd,void *buffer,size_t size){
 if(gated.load()){char link[64],path[4096];snprintf(link,sizeof(link),"/proc/self/fd/%d",fd);auto n=readlink(link,path,sizeof(path)-1);if(n>0&&std::string_view(path,size_t(n)).ends_with("/model.glb")){entered=true;changed.notify_all();std::unique_lock lock(gate_mutex);changed.wait_for(lock,std::chrono::seconds(5),[]{return released.load();});}}
 return __real_read(fd,buffer,size);
}
int main()try{
 const auto home=std::filesystem::temp_directory_path()/("dome-affine-admission-"+std::to_string(getpid()));std::filesystem::create_directories(home/"assets");setenv("HOME",home.c_str(),1);setenv("XDG_STATE_HOME",home.c_str(),1);setenv("XDG_CONFIG_HOME",home.c_str(),1);
 struct Cleanup{std::filesystem::path p;~Cleanup(){gated=false;released=true;changed.notify_all();std::error_code ec;std::filesystem::remove_all(p,ec);}}cleanup{home};
 import_fixture::Fixture f;f.j["nodes"][0]["scale"]={-1.7f,.8f,1.1f};f.j["nodes"][1]["rotation"]={0,0,std::sin(.31f),std::cos(.31f)};const bool default_material=std::getenv("DOME_IMPORT_DEFAULT_SOURCE")!=nullptr;if(default_material){f.j["meshes"][0]["primitives"][0].erase("material");f.j.erase("materials");f.j.erase("extensionsUsed");}const auto path=home/"assets/model.glb";f.save(path);ImportSettings settings;settings.paint_resolution=64;
 Document d;d.replace(Scene{});ready(d);auto baseline=d.copy_scene().encode();auto expected=d.begin_import();
 struct GateRelease{~GateRelease(){gated=false;released=true;changed.notify_all();}}release;
 auto pending_path=home/"pending.dsculpt";d.copy_scene().save(pending_path);
 gated=true;d.import_asset(path,settings);await([]{return entered.load();},"Real source read cancellation barrier not reached");auto pending_save=d.save_async(pending_path);
 check(pending_save.wait_for(std::chrono::seconds(0))!=std::future_status::ready&&Scene::load(pending_path).encode()==baseline,"Pending import Save remains ordered behind the actual read and preserves the existing source file");
 d.cancel_asset_work();gated=false;released=true;changed.notify_all();bool cancelled_save=false;try{pending_save.get();}catch(const std::exception&){cancelled_save=true;}
 check(cancelled_save&&d.wait_asset()&&Scene::load(pending_path).encode()==baseline,"Cancelled import fails its dependent Save without replacing the old project with a stale endpoint");auto cancel_save=home/"cancel.dsculpt";d.save_async(cancel_save).get();ready(d);
 check(d.copy_scene().encode()==baseline&&!d.frame().state->can_undo&&Scene::load(cancel_save).encode()==baseline,"Cancel during captured affine source read leaves exact source/history and ordered Save");
 entered=false;released=false;gated=true;d.import_asset(path,settings);await([]{return entered.load();},"Actual positive import read barrier not reached");auto accepted_path=home/"accepted-import.dsculpt";auto accepted_save=d.save_async(accepted_path);
 check(accepted_save.wait_for(std::chrono::seconds(0))!=std::future_status::ready&&!std::filesystem::exists(accepted_path),"Accepted import owns its following Save before any imported object is published");gated=false;released=true;changed.notify_all();accepted_save.get();ready(d);auto imported=d.copy_scene();auto accepted=imported.encode();
 check(Scene::load(accepted_path).encode()==accepted,"Immediate dependent Save contains the complete accepted affine source graph");
 check(imported.objects().size()==3&&accepted["version"]==(default_material?17:13),"Actual native asset worker admits complete witnessed affine source graph under its exact feature version");
 const auto id=imported.active().id;const auto &o=imported.active();check(imported.has_affine_placement(id)&&d.object_editable(id)&&o.appearance&&*o.appearance->original_bytes==f.bytes(),"Accepted active object retains source bytes/placement and live edit permission");
 if(default_material)check(o.appearance->material==0&&o.appearance->source_material==-1&&o.appearance->material_json.empty(),"Actual native admission retains witnessed absence without an authored material ID");
 auto placed=imported.world_matrix(id);auto geometry=o.deformation?o.deformation->preview(o.painted->mesh()):o.painted->mesh();const auto &idx=geometry.indices;auto local=geometry.positions[idx[0]]*.15f+geometry.positions[idx[1]]*.10f+geometry.positions[idx[2]]*.75f;auto p=placed.point({local.x,local.y,local.z});Vec3 point{float(p[0]),float(p[1]),float(p[2])};auto normal=d.gradient(point);auto hit=d.raycast(point+normal,normal*-1.f);check(hit&&length(*hit-point)<4e-6f,"Public native picking follows current morph and original reflected/sheared chain");
 PaintSettings paint;paint.color={0,1,0,1};d.paint_settings(paint);NativeStrokeSample sample;sample.origin=point+normal;sample.direction=normal*-1.f;sample.brush.radius=.03f;
 check(d.begin_native_stroke()&&d.sample_native_stroke(sample),"Accepted affine source paints through ordered evaluated native worker");d.end_native_stroke();auto saved=home/"painted.dsculpt";d.save_async(saved).get();ready(d);auto painted=d.copy_scene().encode();
 check(painted!=accepted&&d.copy_scene().active().painted->pixel(1,54,48).data[9]>0,"Current-world painting modifies the independently chosen original UV texel");
 check(Scene::load(saved).encode()==painted,"Immediately ordered Save restores exact original source and accepted paint endpoint");
 check(d.begin_native_stroke()&&d.sample_native_stroke(sample),"Second affine paint draft begins");d.end_native_stroke(false);ready(d);check(d.copy_scene().encode()==painted,"Cancel retains exact earlier source and paint");
 check(d.undo()&&d.copy_scene().encode()==accepted&&d.undo()&&d.copy_scene().encode()==baseline,"Painting and import each have one exact Undo");check(d.redo()&&d.copy_scene().encode()==accepted&&d.redo()&&d.copy_scene().encode()==painted,"Redo restores both complete source endpoints");ready(d);
 bool stale=false;try{d.import_asset(path,settings,expected);}catch(const std::logic_error&){stale=true;}check(stale&&d.copy_scene().encode()==painted,"Stale chooser source cannot append against a different accepted endpoint");
 d.configure_object_grab(false,true);d.hold_object(id);bool protected_import=false;try{d.import_asset(path,settings);}catch(const std::logic_error&){protected_import=true;}check(protected_import&&d.copy_scene().encode()==painted,"Accepted held-object isolation permission prevents import creation atomically");d.release_held_object(false);
 d.replace(Scene{});ready(d);d.configure_import(settings);d.open_import_browser(home/"assets");await([&]{auto v=d.frame().state->importing;return v&&!v->busy;},"Source browser did not settle");ImportDriver hand(d);hand.open();hand.sector(8);hand.sector(9);hand.sector(10);await([&]{auto v=d.frame().state->importing;return v&&!v->busy;},"Controller source listing did not settle");check(d.frame().state->menu_page==21&&d.frame().state->importing->entries.size()==1,"Real controller menu browses the affine source with existing shared options");hand.sector(6);ready(d);auto controller=d.copy_scene().encode();check(controller==accepted,"Controller Apply admits the same complete affine graph as native import");auto controller_save=home/"controller.dsculpt";d.save_async(controller_save).get();check(Scene::load(controller_save).encode()==controller&&d.undo()&&d.copy_scene().encode()==baseline,"Controller Save/reopen and one Undo preserve exact affine source authority");
 std::cout<<checks<<" actual affine native/controller admission/pick/paint/cancel/history/save checks passed\n";return 0;
}catch(const std::exception&e){gated=false;released=true;changed.notify_all();std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
