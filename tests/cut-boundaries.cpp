// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
struct Gate {
  std::mutex mutex;
  std::condition_variable changed;
  bool armed=false,entered=false;
  void arm(){std::lock_guard lock(mutex);armed=true;entered=false;}
  void stop(){std::unique_lock lock(mutex);if(!armed)return;entered=true;changed.notify_all();changed.wait(lock,[&]{return !armed;});}
  void wait(){std::unique_lock lock(mutex);if(!changed.wait_for(lock,std::chrono::seconds(10),[&]{return entered;}))throw std::runtime_error("Ordered save did not reach its fsync barrier");}
  void release(){std::lock_guard lock(mutex);armed=false;changed.notify_all();}
} save_gate;
struct Release {~Release(){save_gate.release();}};
void check(bool value,const char *message){if(!value)throw std::runtime_error(message);std::cout<<"PASS "<<message<<'\n';}
Scene source(){
  Mesh mesh;
  mesh.positions={{-.2f,-.2f,-.2f},{.2f,-.2f,-.2f},{.2f,.2f,-.2f},{-.2f,.2f,-.2f},
                  {-.2f,-.2f,.2f},{.2f,-.2f,.2f},{.2f,.2f,.2f},{-.2f,.2f,.2f}};
  for(auto point:mesh.positions)mesh.normals.push_back(normalized(point));
  mesh.indices={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7};
  unwrap(mesh,128);
  Scene scene;scene.active().mesh_active=true;scene.active().painted=std::make_shared<PaintedMesh>(std::move(mesh),128);
  return scene;
}
void controller_cancel(Document &doc){
  SpatialFrame frame;frame.focused=frame.head_valid=true;frame.head.position={0,1.6f,0};frame.hands[1].tracked.valid=true;
  auto send=[&](float x,float y,bool press){++frame.sequence;frame.time+=.02;frame.hands[1].x=x;frame.hands[1].y=y;
    frame.hands[1].joystick=press;doc.submit(frame);check(doc.wait_input(),"Controller sample drains through the native input worker");};
  send(0,0,false);send(0,0,true);send(0,0,false);
  check(doc.frame().state->menu_page==17 && doc.frame().state->menu_enabled[11],
        "Accepted cut keeps its own XR Cancel command available without navigating to another page");
  constexpr float angle=11*6.283185307f/12;
  send(std::cos(angle),std::sin(angle),false);send(std::cos(angle),std::sin(angle),true);send(0,0,false);
}
}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd){save_gate.stop();return __real_fsync(fd);}
int main()try {
  auto original=source();auto encoded=original.encode();
  for(bool controller:{false,true}) {
    Document doc;Release cleanup;doc.replace(original.snapshot());
    auto path=std::filesystem::temp_directory_path()/("dome-cut-boundary-"+std::to_string(getpid())+(controller?"-controller":"-desktop")+".dsculpt");
    save_gate.arm();auto saving=doc.save_async(path);save_gate.wait();
    doc.begin_cut();check(doc.wait_cut()&&doc.frame().state->error.empty(),"Cut preview prepares while an earlier ordered save is paused");
    auto preview=doc.frame().state->cut->preview;
    doc.end_cut(true);
    check(doc.frame().state->asset_busy&&doc.frame().state->cut->committing&&doc.asset_status().cancellable,
          "Accepted cut owns a cancellable asset lease behind the earlier save");
    if(controller)controller_cancel(doc);else doc.end_cut(false);
    check(doc.asset_status().cancelling&&doc.frame().state->cut->committing,
          "Cancel requests the asset owner's cancellation and retains its draft until acknowledgement");
    save_gate.release();saving.get();check(doc.wait_asset(),"Cancelled cut releases the ordered asset and final-save waiters");
    check(doc.copy_scene().encode()==encoded&&doc.frame().state->cut&&!doc.frame().state->cut->committing&&
          doc.frame().state->cut->preview==preview,
          "Cancelled publication retains the exact source scene and original editable preview without partial outputs");
    check(Scene::load(path).encode()==encoded,"The preceding save retains its own original scene snapshot");
    doc.end_cut(true);check(doc.wait_asset()&&!doc.frame().state->cut&&doc.frame().state->error.empty(),
          "The same preview can be explicitly retried after cancellation without restarting the workspace");
    check(doc.copy_scene().objects().size()==3&&doc.undo()&&doc.copy_scene().encode()==encoded,
          "Retried cut publishes shell and cap as exactly one global Undo transaction");
    std::filesystem::remove(path);
  }
  return 0;
}catch(const std::exception &error){save_gate.release();std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
