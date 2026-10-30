// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/uv.hpp"
#include "dome/humanoid-creator.hpp"
#include <atomic>
#include <cstdlib>
#include <future>
#include <iostream>
#include <new>
#include <thread>
using namespace dome;
namespace {
std::atomic_bool count_allocations{false};
std::atomic_size_t allocated_bytes{0};
}
[[gnu::noinline]] void *operator new(std::size_t size) {
  if (void *pointer = std::malloc(size ? size : 1)) {
    if (count_allocations.load(std::memory_order_relaxed)) allocated_bytes.fetch_add(size,std::memory_order_relaxed);
    return pointer;
  }
  throw std::bad_alloc();
}
[[gnu::noinline]] void *operator new[](std::size_t size) { return ::operator new(size); }
[[gnu::noinline]] void operator delete(void *pointer) noexcept { std::free(pointer); }
[[gnu::noinline]] void operator delete[](void *pointer) noexcept { std::free(pointer); }
[[gnu::noinline]] void operator delete(void *pointer,std::size_t) noexcept { std::free(pointer); }
[[gnu::noinline]] void operator delete[](void *pointer,std::size_t) noexcept { std::free(pointer); }
void require(bool condition,const char *message) {
  if (!condition) throw std::runtime_error(message);
  std::cout<<"PASS "<<message<<'\n';
}
void rejects(auto operation,std::string_view fragment,const char *message) {
  bool rejected=false;
  try { operation(); } catch(const std::exception &failure) {
    rejected=std::string_view(failure.what()).find(fragment)!=std::string_view::npos;
  }
  require(rejected,message);
}
Mesh grid(uint32_t side) {
  Mesh mesh;
  for(uint32_t y=0;y<=side;++y)for(uint32_t x=0;x<=side;++x){
    mesh.positions.push_back({float(x)/side,float(y)/side,0});
    mesh.normals.push_back({0,0,1});mesh.uv.push_back({.1f+.8f*x/side,.1f+.8f*y/side});
  }
  for(uint32_t y=0;y<side;++y)for(uint32_t x=0;x<side;++x){
    uint32_t a=y*(side+1)+x;mesh.indices.insert(mesh.indices.end(),{a,a+1,a+side+2,a,a+side+2,a+side+1});
  }
  return mesh;
}
int main()try{
  auto source=std::make_shared<const Mesh>(grid(160));
  allocated_bytes=0;count_allocations=true;
  UvWorkspace stub(source,128);
  count_allocations=false;
  std::cout<<"STUB_ALLOCATED_BYTES "<<allocated_bytes.load()<<'\n';
  require(allocated_bytes<1024,"Large UV loading stub allocates no geometry/topology/identity arrays");
  require(stub.view().mesh==source&&!stub.view().topology&&!stub.view().source_vertices&&
          !stub.view().valid&&stub.view().validating&&stub.view().solving&&!stub.view().can_undo,
          "Loading stub retains immutable source and cannot be accepted or edited");
  require(!stub.validated(source,{}),"Empty validation cannot approve uninitialized topology");
  rejects([&]{stub.edit(UvAction::Move,.01f,0);},"initializing","Direct edits reject a loading stub safely");
  auto before=stub.view().mesh;
  rejects([&]{stub.initialize([]{return true;});},"cancelled","Immediate loader cancellation leaves source exact");
  require(stub.view().mesh==before&&!stub.view().topology&&!stub.view().source_vertices,
          "Cancelled loader publishes no partial topology or source map");
  UvInitializationLimits small;small.maximum_bytes=1024;
  rejects([&]{stub.initialize({},small);},"memory budget","Initialization checks memory bounds before geometry copy");
  small={};small.maximum_work=1;
  rejects([&]{stub.initialize({},small);},"work budget","Initialization checks total work bounds before geometry copy");
  small={};small.maximum_vertices=1;
  rejects([&]{stub.initialize({},small);},"geometry budget","Initialization rejects oversized source geometry explicitly");
  size_t steps=0;
  rejects([&]{stub.initialize([&]{return ++steps>256;});},"cancelled","Topology traversal checks cancellation inside map/edge construction");
  require(!stub.view().topology&&stub.view().mesh==source,"Mid-topology cancellation leaves loading stub unchanged");
  auto main_thread=std::this_thread::get_id();std::atomic_bool ran_off_thread=false;
  auto future=std::async(std::launch::async,[copy=stub,&ran_off_thread,main_thread]()mutable{
    copy.initialize([&]{ran_off_thread=std::this_thread::get_id()!=main_thread;return false;});
    auto mesh=copy.view().mesh;copy.validated(mesh,validate_uv(*mesh));return copy;
  });
  require(future.wait_for(std::chrono::seconds(60))==std::future_status::ready,"Background topology initialization completes within bounded fixture time");
  auto ready=future.get();
  require(ran_off_thread&&ready.view().topology&&ready.view().valid&&!ready.view().solving,
          "Detached worker builds and validates the complete UV workspace");
  require(!stub.view().topology&&stub.view().mesh==source,
          "Initializing a copied workspace never mutates shared loading/history state");
  require(ready.view().source_vertices->size()==source->positions.size()&&
          ready.view().topology->islands.size()==1&&!uv_changed(*source,*ready.view().mesh),
          "Initialization preserves complete source identity and semantic UV state");
  bool exact=true;for(uint32_t i=0;i<ready.view().source_vertices->size();++i)exact&=ready.view().source_vertices->at(i)==i;
  require(exact,"Every lazily initialized vertex retains its exact original index");
  ready.edit(UvAction::Move,.01f,0);
  require(ready.view().can_undo&&!stub.view().can_undo,
          "Ready workspace history remains detached from loading snapshot");
  auto invalid=std::make_shared<Mesh>(grid(1));invalid->uv.clear();
  UvWorkspace malformed(invalid,128);
  rejects([&]{malformed.initialize();},"requires an unwrapped mesh","Malformed source failure retains an actionable loading error");
  require(!malformed.view().topology&&malformed.view().mesh==invalid,
          "Failed source initialization cannot publish partial geometry");
  require(malformed.validated(malformed.view().mesh,"Invalid source attributes") &&
          !malformed.view().valid && !malformed.view().validating && !malformed.view().solving,
          "Failed loading validation stops busy state without approving missing topology");
  rejects([&]{malformed.edit(UvAction::Move,.01f,0);},"reopen",
          "Failed loader draft keeps actionable cancel/reopen behavior");
  std::weak_ptr<PaintedMesh> retained;
  {
    auto painted=std::make_shared<PaintedMesh>(grid(1),128);retained=painted;
    auto alias=std::shared_ptr<const Mesh>(painted,&painted->mesh());
    UvWorkspace owned(alias,128);alias.reset();painted.reset();
    require(!retained.expired(),"Alias loading source keeps original PaintedMesh lifetime pinned");
    owned.initialize();
    require(retained.expired()&&owned.view().topology,
            "Initialized workspace safely releases alias after producing independent immutable geometry");
  }
  auto human=create_humanoid(humanoid_baseline_defaults("vroid-beta-female"));
  auto authored=std::make_shared<const Mesh>(human.parts[10].mesh);UvWorkspace real(authored,256);
  real.initialize();
  require(real.view().topology&&real.view().source_vertices->size()==authored->positions.size()&&
          !uv_changed(*authored,*real.view().mesh),
          "Actual CC0 VRoid body initializes complete authored topology without changing UVs");
  return 0;
}catch(const std::exception &failure){count_allocations=false;std::cerr<<"FAIL "<<failure.what()<<'\n';return 1;}
