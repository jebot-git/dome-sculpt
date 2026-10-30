// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "xatlas.h"
#include <atomic>
#include <condition_variable>
#include <iostream>
#include <mutex>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
std::atomic<xatlas::ProgressFunc> forward{nullptr};
std::atomic<unsigned> atlas_number{0};
thread_local int category=-1;
std::mutex gate_mutex;
std::condition_variable gate_changed;
bool block_chart=false, entered=false, released=false;
unsigned checks=0;
void check(bool value,const char *message) {
  if(!value)throw std::runtime_error(message);
  ++checks;std::cout<<"PASS "<<message<<'\n';
}
bool trampoline(xatlas::ProgressCategory stage,int percent,void *data) {
  if(stage==xatlas::ProgressCategory::ComputeCharts) {
    std::unique_lock lock(gate_mutex);
    if(block_chart) {
      block_chart=false;entered=true;gate_changed.notify_all();
      if(!gate_changed.wait_for(lock,std::chrono::seconds(20),[]{return released;}))
        std::terminate();
    }
  }
  auto previous=category;category=int(stage);
  auto result=forward.load()(stage,percent,data);
  category=previous;return result;
}
template<class Operation> void cancel_at(Operation operation,unsigned target,const char *message) {
  atlas_number=0;std::atomic_bool visited=false;
  bool refused=false;
  try {
    operation([&] {
      if(category==int(xatlas::ProgressCategory::ComputeCharts)&&atlas_number.load()==target) {
        visited=true;return true;
      }
      return false;
    });
  }catch(const std::runtime_error &){refused=true;}
  check(visited&&refused,message);
}
Scene sculpt() {
  Scene scene;scene.active().volume=std::make_shared<Volume>(.04f);
  Brush brush;brush.radius=.18f;
  scene.active().volume->begin_stroke();scene.active().volume->stamp(brush);
  scene.active().volume->end_stroke();return scene;
}
Scene painted() {
  Mesh mesh;mesh.positions={{-.2f,-.2f,-.2f},{.2f,-.2f,-.2f},{.2f,.2f,-.2f},{-.2f,.2f,-.2f},
                           {-.2f,-.2f,.2f},{.2f,-.2f,.2f},{.2f,.2f,.2f},{-.2f,.2f,.2f}};
  for(auto p:mesh.positions)mesh.normals.push_back(normalized(p));
  mesh.indices={0,2,1,0,3,2,4,5,6,4,6,7,0,1,5,0,5,4,1,2,6,1,6,5,2,3,7,2,7,6,3,0,4,3,4,7};
  unwrap(mesh,128);
  Scene scene;scene.active().mesh_active=true;
  scene.active().painted=std::make_shared<PaintedMesh>(std::move(mesh),128);
  return scene;
}
}
void real_progress(xatlas::Atlas *,xatlas::ProgressFunc,void *)
  asm("__real__ZN6xatlas19SetProgressCallbackEPNS_5AtlasEPFbNS_16ProgressCategoryEiPvES3_");
void wrap_progress(xatlas::Atlas *,xatlas::ProgressFunc,void *)
  asm("__wrap__ZN6xatlas19SetProgressCallbackEPNS_5AtlasEPFbNS_16ProgressCategoryEiPvES3_");
void wrap_progress(xatlas::Atlas *atlas,xatlas::ProgressFunc function,void *data) {
  if(!function){real_progress(atlas,function,data);return;}
  auto old=forward.exchange(function);
  if(old&&old!=function)std::terminate();
  ++atlas_number;real_progress(atlas,trampoline,data);
}
int main() try {
  auto source=sculpt();const auto before=source.encode();
  const auto owner=source.active().volume->geometry_owner();
  cancel_at([&](auto cancelled){(void)source.surfaces(true,cancelled);},1,
            "Scene export surfaces cancel inside genuine atlas chart work");
  check(source.encode()==before&&source.active().volume->geometry_owner()==owner,
        "Cancelled Scene atlas leaves exact sculpt source and owner");
  check(!source.surfaces(true).empty(),"Scene atlas retries successfully after cancellation");

  auto original=painted();const auto original_bytes=original.encode();
  const auto paint=original.active().painted;
  RetopoMesh topology;std::vector<TopologyID> corners;
  for(auto p:std::array<Vec3,4>{{{-.2f,-.2f,.2f},{.2f,-.2f,.2f},{.2f,.2f,.2f},{-.2f,.2f,.2f}}})
    corners.push_back(topology.add_vertex({p}));
  topology.add_face(corners);const auto topology_bytes=topology.encode();
  cancel_at([&](auto cancelled){publish_retopology(original,topology,original.active().id,{},cancelled);},1,
            "Retopology publication cancels inside genuine atlas chart work");
  check(original.encode()==original_bytes&&original.active().painted==paint&&topology.encode()==topology_bytes,
        "Cancelled retopology preserves exact Scene, painted owner and topology");
  auto retry=original.snapshot();
  check(publish_retopology(retry,topology,retry.active().id,{})!=original.active().id,
        "Retopology retries to an actual new editable patch");

  BakeSource high;high.mesh=paint->mesh();
  CutSettings settings;settings.offset_mm=30;
  auto preview=preview_cut(high.mesh,settings);
  check(preview.changed&&!preview.cap_mesh.indices.empty(),"Cut fixture has real shell and cap geometry");
  for(unsigned atlas: {1u,2u}) {
    cancel_at([&](auto cancelled){publish_cut(original,preview,high,64,cancelled);},atlas,
              atlas==1?"Cut shell cancels inside genuine atlas chart work":"Cut cap cancels inside genuine atlas chart work");
    check(original.encode()==original_bytes&&original.active().painted==paint,
          "Cancelled cut leaves exact original Scene and painted owner");
  }
  retry=original.snapshot();auto result=publish_cut(retry,preview,high,64);
  check(result.changed&&result.shell&&result.cap,"Cut retries with both editable outputs after atlas cancellation");

  Document document;document.replace(source.snapshot());
  const auto initial=document.frame().state;
  {
    std::lock_guard lock(gate_mutex);block_chart=true;entered=false;released=false;
  }
  document.prepare_mesh(128);
  bool arrived;
  {
    std::unique_lock lock(gate_mutex);
    arrived=gate_changed.wait_for(lock,std::chrono::seconds(15),[]{return entered;});
  }
  const bool cancellable=document.frame().state->can_cancel_asset&&document.asset_status().cancellable;
  document.cancel_asset_work();
  {
    std::lock_guard lock(gate_mutex);released=true;gate_changed.notify_all();
  }
  check(arrived&&cancellable,"Native painting-mesh preparation exposes Cancel during actual atlas work");
  check(document.wait_asset(std::chrono::seconds(20)),"Cancelled atlas releases the actual ordered file worker");
  auto cancelled=document.frame().state;
  check(document.copy_scene().encode()==before&&cancelled->dirty==initial->dirty&&
        cancelled->can_undo==initial->can_undo&&!cancelled->asset_busy&&!cancelled->can_cancel_asset&&
        !document.asset_status().cancellable&&cancelled->error.empty(),
        "Cancelled preparation preserves source/history and clears worker cancellation state");
  document.prepare_mesh(128);
  check(document.wait_asset(std::chrono::seconds(20))&&document.copy_scene().active().mesh_active&&
        document.frame().state->error.empty(),"Painting preparation retries through the real worker");
  document.undo();
  check(document.copy_scene().encode()==before,"Successful retry remains exactly one Undo operation");
  document.redo();
  check(document.copy_scene().active().mesh_active&&!document.asset_status().cancellable,
        "Redo restores the prepared mesh without retaining a cancelled job");
  std::cout<<checks<<" authoring atlas cancellation checks passed\n";
  return 0;
}catch(const std::exception &error) {
  {std::lock_guard lock(gate_mutex);released=true;gate_changed.notify_all();}
  std::cerr<<"FAIL "<<error.what()<<'\n';return 1;
}
