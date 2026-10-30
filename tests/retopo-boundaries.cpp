// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <nlohmann/json.hpp>
#include <condition_variable>
#include <iostream>
#include <unistd.h>
using namespace dome;
namespace {
struct Gate {
  std::mutex mutex;std::condition_variable changed;bool armed=false,entered=false;
  void arm(){std::lock_guard lock(mutex);armed=true;entered=false;}
  void stop(){std::unique_lock lock(mutex);if(!armed)return;entered=true;changed.notify_all();changed.wait(lock,[&]{return !armed;});}
  void wait(){std::unique_lock lock(mutex);if(!changed.wait_for(lock,std::chrono::seconds(10),[&]{return entered;}))throw std::runtime_error("Worker barrier not reached");}
  void release(){std::lock_guard lock(mutex);armed=false;changed.notify_all();}
} topology_gate,file_gate;
void check(bool ok,const char *message){if(!ok)throw std::runtime_error(message);std::cout<<"PASS "<<message<<'\n';}
void ready(Document &d){check(d.wait_retopology()&&d.frame().state->error.empty(),"Native retopology work completes");}
void point(Document &d,Vec3 p){check(d.begin_desktop(),"A desktop draft gesture acquires ownership");Brush b;b.from=b.to=p;d.stamp_desktop(b);d.end_desktop();ready(d);}
Scene plane(){Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};Scene s;s.active().painted=std::make_shared<PaintedMesh>(m,64);s.active().mesh_active=true;return s;}
void patch(Document &d){d.begin_retopology();ready(d);point(d,{-.2f,-.2f,0});point(d,{.2f,-.2f,0});point(d,{0,.2f,0});d.edit_retopology(RetopoAction::Face);ready(d);}
void wait_asset_started(Document &d){auto end=std::chrono::steady_clock::now()+std::chrono::seconds(10);while(!d.frame().state->asset_busy&&std::chrono::steady_clock::now()<end)std::this_thread::sleep_for(std::chrono::milliseconds(2));check(d.frame().state->asset_busy&&d.asset_status().cancellable,"Retopology reaches its cancellable ordered asset stage");}
struct Release {~Release(){topology_gate.release();file_gate.release();}};
}
extern "C" void __real__ZN4dome15RetopoWorkspace4editENS_12RetopoActionENS_4Vec3EfRKSt8functionIFbvEE(RetopoWorkspace *,RetopoAction,Vec3,float,const std::function<bool()> &);
extern "C" void __wrap__ZN4dome15RetopoWorkspace4editENS_12RetopoActionENS_4Vec3EfRKSt8functionIFbvEE(RetopoWorkspace *workspace,RetopoAction action,Vec3 point,float distance,const std::function<bool()> &cancelled){if(action==RetopoAction::EndGesture)topology_gate.stop();__real__ZN4dome15RetopoWorkspace4editENS_12RetopoActionENS_4Vec3EfRKSt8functionIFbvEE(workspace,action,point,distance,cancelled);}
extern "C" int __real_fsync(int);
extern "C" int __wrap_fsync(int fd){file_gate.stop();return __real_fsync(fd);}
int main()try {
  auto source=plane();auto original=source.encode();
  {
    Document d;Release cleanup;d.replace(source.snapshot());patch(d);
    topology_gate.arm();d.end_retopology(true);topology_gate.wait();
    check(d.frame().state->retopology->committing&&!d.frame().state->asset_busy,
          "Accepted Apply pauses in the real topology drain before any asset lease exists");
    d.end_retopology(false);
    check(!d.frame().state->retopology&&d.copy_scene().encode()==original,
          "Explicit Cancel closes a pre-asset accepted draft without changing the committed scene");
    topology_gate.release();check(d.wait_asset(),"Cancelled topology drain does not strand final-save waiting");
  }
  {
    Document d;Release cleanup;d.replace(source.snapshot());
    auto path=std::filesystem::temp_directory_path()/("dome-retopo-boundary-"+std::to_string(getpid())+".dsculpt");
    file_gate.arm();auto saving=d.save_async(path);file_gate.wait();patch(d);d.end_retopology(true);wait_asset_started(d);
    d.end_retopology(false);
    check(d.asset_status().cancelling,"Cancel during asset publication requests cancellation instead of discarding in-flight state");
    file_gate.release();saving.get();check(d.wait_asset(),"Cancelled ordered asset publication releases all waiters");
    check(d.frame().state->retopology&&!d.frame().state->retopology->committing&&d.copy_scene().encode()==original,
          "Asset-stage cancellation retains the editable draft and exact original scene for correction");
    d.end_retopology(false);check(!d.frame().state->retopology,"A subsequent explicit Cancel closes the retained draft");
    std::filesystem::remove(path);
  }
  return 0;
}catch(const std::exception&e){topology_gate.release();file_gate.release();std::cerr<<e.what()<<'\n';return 1;}
