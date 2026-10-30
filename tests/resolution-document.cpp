// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "ray-corridor.hpp"
#include <nlohmann/json.hpp>
#include <condition_variable>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
unsigned checks=0;
void check(bool value,const char *message){++checks;if(!value)throw std::runtime_error(message);std::cout<<"PASS "<<message<<'\n';}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}
AssetSource lease(const Document &d){auto s=d.frame().state;return {s->generation,s->revision,s->active_object};}
void mesh(Document &d){check(d.wait_mesh(),"Immutable current surface ready");}
Volume sphere(){Volume v(.04f);v.begin_stroke();Brush b;b.radius=.237f;b.from=b.to={.013f,.017f,-.011f};b.blend=0;v.stamp(b);v.end_stroke();return v.geometry_snapshot();}
nlohmann::json ordered(const Volume &v){auto j=v.encode();std::sort(j["samples"].begin(),j["samples"].end());for(auto &l:j["sculpt_layers"])std::sort(l["delta"].begin(),l["delta"].end());if(j.contains("sculpt_mask"))std::sort(j["sculpt_mask"]["values"].begin(),j["sculpt_mask"]["values"].end());return j;}
std::atomic_bool gate{false};std::mutex mutex;std::condition_variable changed;bool entered=false,released=false;
void arm(){std::lock_guard lock(mutex);entered=released=false;gate=true;}
void await(){std::unique_lock lock(mutex);if(!changed.wait_for(lock,std::chrono::seconds(10),[]{return entered;}))throw std::runtime_error("Ordered save gate did not enter");}
void release(){std::lock_guard lock(mutex);released=true;changed.notify_all();}
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd){if(gate.exchange(false)){std::unique_lock lock(mutex);entered=true;changed.notify_all();if(!changed.wait_for(lock,std::chrono::seconds(20),[]{return released;}))return -1;}return __real_fsync(fd);}
int main(int argc,char **argv){try{
 if(argc==3 && std::string(argv[1])=="--ui-fixture"){Scene s;s.active().volume=std::make_shared<Volume>(sphere());s.save(argv[2]);return 0;}
 if(argc==3 && std::string(argv[1])=="--ray-fixture"){Scene s;auto v=test::dense_ray_corridor();bool exhausted=false;try{v.raycast({0,0,2.2f},{0,0,-1},10);}catch(const std::length_error&){exhausted=true;}if(!exhausted)throw std::runtime_error("Native ray fixture must exhaust real candidate cells");s.active().volume=std::make_shared<Volume>(std::move(v));s.save(argv[2]);return 0;}
 if(argc==3 && std::string(argv[1])=="--inspect"){std::cout<<Scene::load(argv[2]).encode().dump()<<'\n';return 0;}
 Document d;d.replace(sphere());mesh(d);auto baseline=ordered(d.copy_volume());auto state=d.frame().state;
 check(state->can_change_resolution && std::abs(state->sculpt_voxel_mm-40)<1e-5,"Source exposes current world-mm spacing and ready state");
 SculptResolutionOptions options;options.voxel_size=.04f;d.change_sculpt_resolution(options,lease(d));
 check(!d.frame().state->asset_busy && !d.frame().state->can_undo && d.frame().state->revision==state->revision,"Same-voxel confirmation never queues work or changes revision/history");
 auto stale=lease(d);auto settings=d.frame().state->sculpt_settings;settings.symmetry_axes=5;d.sculpt_settings(settings);
 check(refuses([&]{d.change_sculpt_resolution(options,stale);}),"Stale source lease refused even on same-voxel no-op");d.undo();mesh(d);
 options.voxel_size=.02f;d.change_sculpt_resolution(options,lease(d));check(d.wait_asset(),"Resolution background worker completes");mesh(d);
 check(d.frame().state->error.empty() && d.copy_volume().voxel_size()==.02f && d.copy_volume().band()==.16f,"Validated worker changes spacing and retains original physical band");
 auto result=ordered(d.copy_volume());check(d.frame().state->can_undo,"Resolution publishes one history command");
 check(d.undo(),"Resolution undo available");mesh(d);check(ordered(d.copy_volume())==baseline && !d.frame().state->can_undo,"Undo restores exact original layers/field/settings in one step");
 check(d.redo(),"Resolution redo available");mesh(d);check(ordered(d.copy_volume())==result,"Redo restores exact detached replacement");
 auto path=std::filesystem::temp_directory_path()/("dome-resolution-doc-"+std::to_string(getpid())+".dsculpt");d.save(path);
 Document reopened;reopened.open(path);mesh(reopened);check(ordered(reopened.copy_volume())==result && !reopened.frame().state->can_undo,"Native save/restart restores Volume4 field and physical band without transient history");
 d.undo();mesh(d);auto object=d.frame().state->active_object;auto info=d.frame().state->objects.front();
 d.sculpt_layer_properties(object,d.copy_volume().active_layer(),"Locked 日本語",true,true,.75f);mesh(d);auto before=ordered(d.copy_volume());
 options.voxel_size=.02f;d.change_sculpt_resolution(options);check(d.wait_asset(),"Resolution permits locked layer representation change");mesh(d);
 check(d.copy_volume().layers()[0].locked && d.copy_volume().layers()[0].name=="Locked 日本語" && d.copy_volume().layers()[0].opacity==.75f,"Layer identity/lock/name/strength survive native operation");d.undo();mesh(d);check(ordered(d.copy_volume())==before,"Layer representation undo exact");
 options.maximum_work=128;d.change_sculpt_resolution(options);check(d.wait_asset(),"Quality/budget refusal drains worker");mesh(d);check(!d.frame().state->error.empty() && ordered(d.copy_volume())==before,"Refusal keeps original exact and reports normal status");
 d.object_properties(object,info.name,true,true,info.transform,info.material);check(!d.frame().state->can_change_resolution && refuses([&]{d.change_sculpt_resolution(options);}),"Object lock refuses resolution");
 Scene grouped;grouped.active().volume=std::make_shared<Volume>(sphere());
 auto parent=grouped.add_group("Parent",{{},{},1.7f});auto child=grouped.add_group("Child",{{},{},.5f},parent);grouped.active().group=child;grouped.active().transform.scale=.75f;
 Document scaled;scaled.replace(grouped);mesh(scaled);check(std::abs(scaled.frame().state->sculpt_voxel_mm-25.5)<1e-5,"World millimetres include complete nested group/object scale");
 options={};options.voxel_size=.02f;scaled.change_sculpt_resolution(options);check(scaled.wait_asset(),"Nested scaled resolution drains");mesh(scaled);check(std::abs(scaled.frame().state->sculpt_voxel_mm-12.75)<1e-5 && scaled.copy_scene().groups().size()==2 && scaled.copy_scene().active().transform.scale==.75f,"Resolution preserves nested transforms and reports updated physical spacing");
 Document cutting;cutting.replace(sphere());mesh(cutting);const auto cut_original=ordered(cutting.copy_volume());
 cutting.begin_cut();check(cutting.wait_cut(),"Plane-cut preview prepares before resolution exclusion check");
 check(cutting.frame().state->cut && !cutting.frame().state->can_change_resolution && refuses([&]{cutting.change_sculpt_resolution(options);}),"Active cut workspace excludes native resolution without resetting preview");
 check(refuses([&]{cutting.draft_scene();}),"Native resolution draft cannot bypass active cut source lease");
 cutting.end_cut(false);mesh(cutting);check(cutting.frame().state->can_change_resolution && ordered(cutting.copy_volume())==cut_original,"Cancelling cut restores exact sculpt and resolution availability");
 Document pending;pending.replace(sphere());mesh(pending);options={};options.voxel_size=.02f;
 struct Release{~Release(){release();}} guard;
 arm();auto saved=pending.save_async(path);await();pending.change_sculpt_resolution(options,lease(pending));check(pending.asset_status().cancellable,"Queued operation exposes cancellation");pending.cancel_asset_work();release();saved.get();check(pending.wait_asset(),"Cancelled operation drains");
 check(ordered(pending.copy_volume())==baseline && !pending.frame().state->can_undo,"Cancellation publishes neither field nor history");
 arm();saved=pending.save_async(path);await();pending.change_sculpt_resolution(options,lease(pending));pending.replace(Volume(.07f));release();saved.get();check(pending.wait_asset(),"Stale-generation operation drains");check(pending.copy_volume().voxel_size()==.07f && pending.copy_volume().voxels().empty() && !pending.frame().state->can_undo,"Late worker cannot overwrite replacement document");
 std::filesystem::remove(path);std::cout<<checks<<" native resolution checks passed\n";
 }catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}}
