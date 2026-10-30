// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "ray-index.hpp"
#include "ray-corridor.hpp"
#include <atomic>
#include <chrono>
#include <future>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
using namespace dome::sculpt_detail;
namespace dome {struct DocumentNativeTestAccess {
  static std::condition_variable *condition(Document &d){return &d.native_changed_;}
};}
namespace {
enum class Phase { Cold, Nodes, Cells };
std::atomic<std::condition_variable *> native_condition{nullptr};
std::atomic_bool armed{false};std::mutex gate;std::condition_variable changed;
bool entered=false,released=false;Phase phase=Phase::Cold;RayIndexStats start;
unsigned checks=0;
void check(bool b,const char *text){if(!b)throw std::runtime_error(text);++checks;std::cout<<"PASS "<<text<<std::endl;}
void release(){std::lock_guard lock(gate);released=true;changed.notify_all();}
struct Release {~Release(){release();}};
void arm(Phase p){std::lock_guard lock(gate);entered=released=false;phase=p;start=ray_index_stats();armed=true;}
void await(){std::unique_lock lock(gate);if(!changed.wait_for(lock,std::chrono::seconds(30),[]{return entered;}))throw std::runtime_error("Native ray never reached actual indexed work barrier");}
nlohmann::json ordered(const Volume &v){auto j=v.encode();std::sort(j["samples"].begin(),j["samples"].end());for(auto &l:j["sculpt_layers"])std::sort(l["delta"].begin(),l["delta"].end());return j;}
bool reached(const RayIndexStats &s){return phase==Phase::Cold?s.pages_scanned>start.pages_scanned:phase==Phase::Nodes?s.node_visits>=start.node_visits+256:s.cells>=start.cells+256;}
}
extern "C" void notify_real(std::condition_variable*) asm("__real__ZNSt18condition_variable10notify_allEv");
extern "C" void notify_wrap(std::condition_variable*) asm("__wrap__ZNSt18condition_variable10notify_allEv");
extern "C" void notify_wrap(std::condition_variable *cv){notify_real(cv);if(cv==native_condition.load())changed.notify_all();}
extern "C" std::optional<Vec3> ray_real(const Volume*,Vec3,Vec3,float,const std::function<bool()>&) asm("__real__ZNK4dome6Volume7raycastENS_4Vec3ES1_fRKSt8functionIFbvEE");
extern "C" std::optional<Vec3> ray_wrap(const Volume*,Vec3,Vec3,float,const std::function<bool()>&) asm("__wrap__ZNK4dome6Volume7raycastENS_4Vec3ES1_fRKSt8functionIFbvEE");
extern "C" std::optional<Vec3> ray_wrap(const Volume *v,Vec3 o,Vec3 d,float m,const std::function<bool()> &cancelled){
  if(!armed.load())return ray_real(v,o,d,m,cancelled);
  if(!cancelled)throw std::runtime_error("Native field picker omitted its real session cancellation predicate");
  auto observe=[&]{
    if(armed.load()&&reached(ray_index_stats())&&armed.exchange(false)){
      std::unique_lock lock(gate);entered=true;changed.notify_all();
      if(!changed.wait_for(lock,std::chrono::seconds(30),[&]{return released||cancelled();}))throw std::runtime_error("Native ray cancellation predicate did not release actual indexed work");
    }
    return cancelled();
  };
  return ray_real(v,o,d,m,observe);
}
namespace {
void scenario(const Volume &field,Phase p,int operation){
  clear_ray_index_cache();auto doc=std::make_unique<Document>();Release guard;
  doc->replace(field.geometry_snapshot());if(!doc->wait_mesh(std::chrono::seconds(60)))throw std::runtime_error("Native paged render baseline did not become ready: "+doc->frame().state->error);
  check(true,"Native paged field and immutable render baseline are ready");
  native_condition=DocumentNativeTestAccess::condition(*doc);
  auto held=doc->copy_volume();auto original=ordered(held);auto owner=held.geometry_owner();auto mesh=doc->frame().surface;auto revision=doc->frame().state->revision;
  if(p!=Phase::Cold){
    // Exercise and retain the original three-argument API on the SAME owner.
    using Legacy=std::optional<Vec3>(Volume::*)(Vec3,Vec3,float)const;
    Legacy legacy=static_cast<Legacy>(&Volume::raycast);(held.*legacy)({0,0,0},{0,0,1},.0001f);
    check(ray_index_stats().cached_entries==1,"Legacy ray API primes one immutable paged owner without changing the field");
  }
  arm(p);check(doc->begin_native_stroke(),"Native ray gesture acquires its source lease");
  NativeStrokeSample sample;sample.origin={0,0,.5f};sample.direction={0,0,-1};sample.brush.tool=Tool::Carve;sample.brush.radius=.005f;
  check(doc->sample_native_stroke(sample),"Actual native sample enters paged picking");await();
  auto before=ray_index_stats();
  check(reached(before),"Barrier observes actual cold pages, hierarchy nodes or candidate cells rather than an artificial delay");
  if(p==Phase::Cold)check(before.cached_entries==0&&before.cold_builds==start.cold_builds,"Incomplete cold index is not published");
  else check(before.cold_builds==start.cold_builds,"Warm candidate traversal reuses the exact minimal owner");
  auto pending=std::async(std::launch::async,[&]{
    if(operation==0)doc->end_native_stroke(false);
    else if(operation==1)doc->replace(Volume(.07f));
    else doc.reset();
  });
  check(pending.wait_for(std::chrono::seconds(10))==std::future_status::ready,"Cancel, stale replacement or shutdown completes while indexed work is gated only by its real cancellation predicate");pending.get();
  if(doc){
    check(doc->wait_native_stroke(),"Cancelled/stale indexed native job drains without manual barrier release");
    if(operation==0)check(ordered(doc->copy_volume())==original&&doc->frame().surface==mesh&&doc->frame().state->revision==revision&&!doc->frame().state->can_undo,"Cancellation retains exact authority, history and immutable render baseline");
    else check(doc->copy_volume().voxel_size()==.07f&&doc->copy_volume().voxels().empty()&&!doc->frame().state->can_undo,"Stale indexed work cannot overwrite a replacement generation");
  }
  check(ordered(held)==original&&held.geometry_owner()==owner,"Held paged samples and strong owner remain immutable across cancellation/shutdown");
  auto after=ray_index_stats();
  if(p==Phase::Cold)check(after.cached_entries==0,"Cancelled cold construction leaves no partial cached owner");
  else {auto builds=after.cold_builds;held.raycast({0,0,0},{0,0,1},.0001f);check(ray_index_stats().cold_builds==builds,"Cancelled candidate traversal does not poison the reusable immutable index");}
  native_condition=nullptr;clear_ray_index_cache();
}
}
int main()try{
  auto field=dome::test::dense_ray_corridor();
  for(auto p:{Phase::Cold,Phase::Nodes,Phase::Cells})for(int operation=0;operation<3;++operation)scenario(field,p,operation);
  std::cout<<checks<<" native paged-ray cancel/stale/shutdown checks passed\n";return 0;
}catch(const std::exception &e){armed=false;release();std::cerr<<e.what()<<'\n';return 1;}
