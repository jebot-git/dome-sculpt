// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <nlohmann/json.hpp>
#include <condition_variable>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
size_t checks=0;
void check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);++checks;std::cout<<"PASS "<<message<<'\n';}
template<class F> bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}
nlohmann::json archive(const Volume &v){auto j=v.encode();std::sort(j["samples"].begin(),j["samples"].end());for(auto &l:j["sculpt_layers"])std::sort(l["delta"].begin(),l["delta"].end());if(j.contains("sculpt_mask"))std::sort(j["sculpt_mask"]["values"].begin(),j["sculpt_mask"]["values"].end());return j;}
Volume sphere(){Volume v(.04f);v.begin_stroke();Brush b;b.radius=.237f;b.from=b.to={.013f,.017f,-.011f};b.blend=0;v.stamp(b);v.end_stroke();return v.geometry_snapshot();}
AssetSource lease(const Document &d){auto s=d.frame().state;return {s->generation,s->revision,s->active_object};}
void ready(Document &d){check(d.wait_asset()&&d.wait_mesh(),"Background operation and current immutable surface complete");}
std::atomic_bool gate{false};std::mutex mutex;std::condition_variable changed;bool entered=false,released=false;
void arm(){std::lock_guard lock(mutex);entered=released=false;gate=true;}
void await(){std::unique_lock lock(mutex);if(!changed.wait_for(lock,std::chrono::seconds(10),[]{return entered;}))throw std::runtime_error("Save gate timed out");}
void release(){std::lock_guard lock(mutex);released=true;changed.notify_all();}
struct Driver {
  Document &doc;SpatialFrame frame;
  explicit Driver(Document &d):doc(d){frame.sequence=1;frame.time=1;frame.focused=frame.head_valid=true;frame.head.position={0,1.6f,0};frame.hands[1].tracked.valid=true;frame.hands[1].tracked.grip.position={0,1.48f,-.5f};send();}
  void send(float x=0,float y=0,bool click=false,float pressure=0){++frame.sequence;frame.time+=.05;auto &h=frame.hands[1];h.x=x;h.y=y;h.joystick=click;h.tracked.trigger=pressure;doc.submit(frame);if(!doc.wait_input())throw std::runtime_error("Input worker timed out");}
  void open(){send();send(0,0,true);send();}
  void sector(int i){float a=i*3.14159265358979323846f/6;send(std::cos(a),std::sin(a));send(std::cos(a),std::sin(a),true);send();}
  void point(float x,float y,float pressure=.8f){auto s=doc.frame().state;auto &h=frame.hands[1];h.tracked.aimValid=true;h.tracked.aim.position=ob::transform(s->panel,{(x/640-.5f)*.6f,(.5f-y/640)*.6f,.25f});h.tracked.aim.orientation=s->panel.orientation;send(0,0,false,pressure);send();}
  void confirm(bool accept){point(accept?470:170,609);}
};
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd){if(gate.exchange(false)){std::unique_lock lock(mutex);entered=true;changed.notify_all();if(!changed.wait_for(lock,std::chrono::seconds(20),[]{return released;}))return -1;}return __real_fsync(fd);}
int main(int argc,char **argv)try {
  if(argc==3&&std::string(argv[1])=="--ui-fixture"){Scene s;s.active().volume=std::make_shared<Volume>(sphere());s.save(argv[2]);return 0;}
  if(argc==3&&std::string(argv[1])=="--inspect"){std::cout<<Scene::load(argv[2]).encode().dump()<<'\n';return 0;}
  Document d;d.replace(sphere());ready(d);auto before=archive(d.copy_volume());auto revision=d.frame().state->revision;
  ProceduralSculptOptions options;options.rows=options.columns=3;options.density=1;options.depth=.12f;
  d.apply_procedural_sculpt(options,lease(d));ready(d);check(d.frame().state->error.empty()&&d.copy_volume().layers().size()==2,"Native Document publishes background-generated detail in a separate layer");
  auto result=archive(d.copy_volume());check(result!=before&&d.frame().state->can_undo,"Accepted procedural geometry is visible and undoable");
  check(d.undo()&&!d.frame().state->can_undo&&archive(d.copy_volume())==before,"One Undo restores the exact original source");ready(d);
  check(d.redo()&&archive(d.copy_volume())==result,"One Redo restores exact detail and layer metadata");ready(d);
  auto path=std::filesystem::temp_directory_path()/("dome-procedural-"+std::to_string(getpid())+".dsculpt");d.save(path);
  Document restored;restored.open(path);ready(restored);check(archive(restored.copy_volume())==result&&!restored.frame().state->can_undo,"Save/restart retains generated sculpt geometry and editable layers");
  auto stale=lease(d);d.undo();ready(d);check(refuses([&]{d.apply_procedural_sculpt(options,stale);}),"Stale editor source cannot edit a newer document");
  auto low=options;low.maximum_work=4;d.apply_procedural_sculpt(low);ready(d);
  check(!d.frame().state->error.empty()&&archive(d.copy_volume())==before&&!d.frame().state->can_undo,"Budget failure reports normal status without geometry or history publication");
  options.density=0;d.apply_procedural_sculpt(options);ready(d);check(archive(d.copy_volume())==before&&!d.frame().state->can_undo,"No-op detail creates no history command");options.density=1;
  Document pending;pending.replace(sphere());ready(pending);struct Release{~Release(){release();}} release_guard;
  arm();auto saved=pending.save_async(path);await();pending.apply_procedural_sculpt(options,lease(pending));pending.cancel_asset_work();release();saved.get();ready(pending);
  check(archive(pending.copy_volume())==before&&!pending.frame().state->can_undo,"Cancelled queued procedural operation retains exact source and history");
  arm();saved=pending.save_async(path);await();pending.apply_procedural_sculpt(options,lease(pending));pending.replace(Volume(.07f));release();saved.get();ready(pending);
  check(pending.copy_volume().voxel_size()==.07f&&pending.copy_volume().voxels().empty()&&!pending.frame().state->can_undo,"Stale procedural worker cannot overwrite replacement document");
  Document xr;xr.replace(sphere());ready(xr);Driver hand(xr);hand.open();hand.sector(8);hand.sector(11);hand.sector(9);
  check(xr.frame().state->menu_page==18&&xr.frame().state->menu_enabled[6],"Joystick Objects / Sculpt layers / Procedural reaches enabled detail controls");
  hand.sector(0);check(xr.frame().state->controls.parameter==VrParameter::ProceduralPattern,"Joystick opens large procedural pattern panel");hand.point(520,300);hand.confirm(false);
  check(xr.frame().state->procedural_options.pattern==ProceduralSculptOptions::Pattern::Panels&&archive(xr.copy_volume())==before&&!xr.frame().state->can_undo,"XR Cancel discards staged procedural settings and leaves model/history exact");
  hand.sector(2);check(xr.frame().state->controls.components==2,"Grid panel exposes independent rows and columns");hand.point(100,240);hand.point(100,320);hand.confirm(true);
  check(xr.frame().state->procedural_options.rows==1&&xr.frame().state->procedural_options.columns==1&&!xr.frame().state->can_undo,"Confirmed XR settings update tool configuration without model history");
  auto seed=xr.frame().state->procedural_options.seed;hand.sector(7);check(xr.frame().state->procedural_options.seed==seed+1,"Next seed is usable directly from controller page");
  hand.sector(6);std::cout<<"XR navigation page="<<xr.frame().state->menu_page<<" index="<<xr.frame().state->menu_index<<" busy="<<xr.frame().state->asset_busy<<" enabled="<<xr.frame().state->menu_enabled[6]<<"\n";ready(xr);std::cout<<"XR apply status: error="<<xr.frame().state->error<<" message="<<xr.frame().state->message<<" layers="<<xr.copy_volume().layers().size()<<" seed="<<xr.frame().state->procedural_options.seed<<" density="<<xr.frame().state->procedural_options.density<<"\n";check(xr.frame().state->error.empty()&&xr.copy_volume().layers().size()==2,"Controller Apply performs actual procedural field operation");
  check(xr.undo()&&archive(xr.copy_volume())==before&&!xr.frame().state->can_undo,"Controller detail is exactly one global Undo");ready(xr);hand.open();hand.sector(8);hand.sector(11);hand.sector(9);
  hand.sector(3);hand.point(100,240);hand.frame.focused=false;hand.send();
  check(xr.frame().state->controls.parameter==VrParameter::None&&archive(xr.copy_volume())==before,"Focus loss discards unconfirmed procedural parameter changes");
  check(spatial_action(0,4)==SpatialCommand::Undo&&spatial_action(0,5)==SpatialCommand::Redo&&spatial_parameter(7,7)==VrParameter::SculptResolution,"Root Undo/Redo and resolution navigation remain intact");
  check(d.frame().state->revision>revision,"Only actual accepted edits advance document authority");
  std::filesystem::remove(path);std::cout<<checks<<" procedural native/controller checks passed\n";return 0;
}catch(const std::exception &e){release();std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
