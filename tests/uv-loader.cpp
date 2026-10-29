// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include <atomic>
#include <condition_variable>
#include <cstdlib>
#include <iostream>
#include <mutex>
#include <new>
#include <numbers>
#include <nlohmann/json.hpp>
#include <thread>
#include <unistd.h>
using namespace dome;
namespace {
std::atomic_bool allocation_gate{false}, entered{false};
std::atomic_size_t geometry_allocation_size{0}, model_geometry_allocations{0};
std::atomic_bool fail_geometry_allocation{false};
std::thread::id model_thread;
std::mutex gate_mutex;
std::condition_variable gate_changed;
bool gate_released=false;
bool observe_allocation(size_t size) {
  if (!allocation_gate.load() || size!=geometry_allocation_size.load()) return false;
  if (std::this_thread::get_id()==model_thread) { ++model_geometry_allocations;return false; }
  if (entered.exchange(true)) return false;
  std::unique_lock lock(gate_mutex);gate_changed.notify_all();
  gate_changed.wait(lock,[]{return gate_released;});
  return fail_geometry_allocation.load();
}
struct AllocationBarrier {
  explicit AllocationBarrier(size_t bytes,bool fail=false) {
    std::lock_guard lock(gate_mutex);gate_released=false;entered=false;
    model_thread=std::this_thread::get_id();geometry_allocation_size=bytes;
    model_geometry_allocations=0;fail_geometry_allocation=fail;allocation_gate=true;
  }
  void wait() {
    std::unique_lock lock(gate_mutex);
    if (!gate_changed.wait_for(lock,std::chrono::seconds(30),[]{return entered.load();}))
      throw std::runtime_error("UV worker did not reach its first geometry allocation");
  }
  void release() {
    std::lock_guard lock(gate_mutex);allocation_gate=false;gate_released=true;gate_changed.notify_all();
  }
  ~AllocationBarrier(){release();}
};
}
[[gnu::noinline]] void *operator new(size_t size) {
  if (observe_allocation(size)) throw std::bad_alloc();
  if (void *p=std::malloc(size?size:1)) return p;
  throw std::bad_alloc();
}
[[gnu::noinline]] void *operator new[](size_t size){return ::operator new(size);}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p,size_t)noexcept{std::free(p);}
void require(bool value,const char *message){if(!value)throw std::runtime_error(message);std::cout<<"PASS "<<message<<'\n';}
Mesh grid(uint32_t side){
  Mesh mesh;for(uint32_t y=0;y<=side;++y)for(uint32_t x=0;x<=side;++x){
    mesh.positions.push_back({float(x)/side,float(y)/side,0});mesh.normals.push_back({0,0,1});mesh.uv.push_back({.1f+.8f*x/side,.1f+.8f*y/side});
  }
  for(uint32_t y=0;y<side;++y)for(uint32_t x=0;x<side;++x){uint32_t a=y*(side+1)+x;mesh.indices.insert(mesh.indices.end(),{a,a+1,a+side+2,a,a+side+2,a+side+1});}
  return mesh;
}
Scene scene(uint32_t side){Scene result;auto &object=result.active();object.mesh_active=true;object.name="Native UV initialization fixture";object.painted=std::make_shared<PaintedMesh>(grid(side),128);return result;}
struct Driver {
  Document &document;SpatialFrame frame;
  explicit Driver(Document &doc):document(doc){frame.sequence=1;frame.time=1;frame.focused=frame.head_valid=true;frame.head.position={0,1.6f,0};frame.hands[1].tracked.valid=true;frame.hands[1].tracked.grip.position={0,1.48f,-.5f};send();}
  void send(float x=0,float y=0,bool click=false){++frame.sequence;frame.time+=.05;auto &hand=frame.hands[1];hand.x=x;hand.y=y;hand.joystick=click;document.submit(frame);if(!document.wait_input())throw std::runtime_error("Controller blocked on UV initialization");}
  void open(){send(0,0,true);send();}
  void sector(int value){float a=value*std::numbers::pi_v<float>/6;send(std::cos(a),std::sin(a));send(std::cos(a),std::sin(a),true);send();}
};
int main()try{
  auto file=std::filesystem::temp_directory_path()/("dome-uv-loader-"+std::to_string(getpid())+".dsculpt");
  auto source=scene(160);source.save(file);auto expected=source.encode();
  Document document;document.open(file);require(document.wait_mesh(std::chrono::seconds(60)),"Real native loader publishes the large immutable source surface");
  const size_t vertices=source.active().painted->mesh().positions.size();
  const size_t allocation=vertices*sizeof(Vec3);
  {
    AllocationBarrier barrier(allocation);document.begin_uv();barrier.wait();auto view=document.frame().state->uv;
    require(model_geometry_allocations==0,"Opening native UV allocates no geometry on the model/input thread");
    require(view&&view->solving&&!view->valid&&!view->topology&&!view->source_vertices,
            "Document exposes loading stub while actual UV worker geometry copy is deterministically blocked");
    bool blocked=false;try{document.end_uv(true);}catch(const std::logic_error&){blocked=true;}
    require(blocked&&!document.undo()&&!document.redo(),"Loading Confirm and history commands are disabled without arbitrary timing sleeps");
    barrier.release();require(document.wait_uv(std::chrono::seconds(60)),"Existing UV worker completes and validates asynchronous initialization");
    auto ready=document.frame().state->uv;
    require(ready&&ready->valid&&ready->topology&&ready->source_vertices->size()==vertices&&!ready->solving,
            "Only complete topology and exact source map become editable");
    require(!document.frame().state->dirty&&document.copy_scene().encode()==expected,
            "Initialization alone preserves the exact clean native scene");document.end_uv(false);
  }
  {
    AllocationBarrier barrier(allocation,true);document.begin_uv();barrier.wait();
    barrier.release();require(document.wait_uv(std::chrono::seconds(60)),"Worker allocation failure leaves the public loading state terminal");
    auto failed=document.frame().state->uv;
    require(failed&&!failed->valid&&!failed->solving&&!failed->validating&&!failed->topology&&
            failed->error.find("Not enough memory")!=std::string::npos,
            "Failed initialization publishes an actionable error without partial editable topology");
    bool blocked=false;try{document.end_uv(true);}catch(const std::exception&){blocked=true;}
    require(blocked&&document.copy_scene().encode()==expected&&!document.frame().state->dirty,
            "Failed workspace cannot be approved or mutate the original native scene");
    document.end_uv(false);document.begin_uv();require(document.wait_uv(std::chrono::seconds(60)),"A new UV session succeeds after the failed worker is cancelled");
    require(document.frame().state->uv->valid&&document.frame().state->uv->source_vertices->size()==vertices,
            "Failure recovery reconstructs complete source identity before enabling edits");document.end_uv(false);
  }
  {
    AllocationBarrier barrier(allocation);document.begin_uv();barrier.wait();document.end_uv(false);
    require(!document.frame().state->uv,"Explicit loading Cancel closes immediately while worker remains blocked");
    document.begin_uv();barrier.release();require(document.wait_uv(std::chrono::seconds(60)),"New UV session initializes after cancellation of the previous request");
    require(document.frame().state->uv->source_vertices->size()==vertices&&!document.frame().state->dirty,
            "Old request cannot overwrite the new workspace or dirty native data");document.end_uv(false);
  }
  {
    AllocationBarrier barrier(allocation);document.begin_uv();barrier.wait();auto replacement=scene(1);document.replace(replacement);
    barrier.release();require(document.wait_mesh(std::chrono::seconds(60)),"Document generation replacement publishes its new source");
    document.begin_uv();require(document.wait_uv(),"New generation initializes its own independent UV source");
    require(document.frame().state->uv->mesh->positions.size()==4,
            "Stale large-source loader cannot overwrite another generation's small mesh");document.end_uv(false);
  }
  document.open(file);require(document.wait_mesh(std::chrono::seconds(60)),"Native source reopens before controller loading test");
  Driver hand(document);
  {
    AllocationBarrier barrier(allocation);hand.open();hand.sector(9);hand.sector(7);barrier.wait();
    require(document.frame().state->uv&&document.frame().state->uv->solving,
            "Controller input finishes dispatch while topology loading runs on another worker");
    hand.frame.focused=false;hand.send();
    require(!document.frame().state->uv,"XR focus loss cancels an unconfirmed loading stub promptly");
    barrier.release();
  }
  require(document.copy_scene().encode()==expected&&!document.frame().state->dirty,
          "Focus-cancelled source initialization preserves exact native project state");
  std::filesystem::remove(file);return 0;
}catch(const std::exception &failure){allocation_gate=false;{std::lock_guard lock(gate_mutex);gate_released=true;gate_changed.notify_all();}std::cerr<<"FAIL "<<failure.what()<<'\n';return 1;}
