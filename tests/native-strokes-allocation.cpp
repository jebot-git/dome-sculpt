// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "ray-index.hpp"
#include <nlohmann/json.hpp>
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#include <pthread.h>
#include <set>
#include <tuple>
using namespace dome;
namespace {std::atomic_bool tracking{false},hit{false};std::atomic<size_t> calls{0},failure{0};}
void *operator new(size_t n){char name[16]{};if(tracking&&pthread_getname_np(pthread_self(),name,sizeof(name))==0&&std::string_view(name)=="dome-native"){auto index=++calls;if(index==failure.load()){failure=0;hit=true;throw std::bad_alloc();}}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void operator delete(void *p)noexcept{std::free(p);}void operator delete(void *p,size_t)noexcept{std::free(p);}
namespace dome {struct DocumentNativeTestAccess {
 static auto tokens(const Document &d){std::lock_guard lock(d.model_mutex_);return std::tuple{d.content_,d.next_content_,d.saved_content_,d.undo_.size(),d.redo_.size()};}
};}
namespace {
void require(bool v,const char*m){if(!v)throw std::runtime_error(m);}
Volume sphere(){Volume v(.04f);v.begin_stroke();Brush b;b.radius=.237f;b.blend=0;v.stamp(b);v.end_stroke();return v.geometry_snapshot();}
nlohmann::json ordered(const Volume &v){auto j=v.encode();std::sort(j["samples"].begin(),j["samples"].end());for(auto &l:j["sculpt_layers"])std::sort(l["delta"].begin(),l["delta"].end());return j;}
std::unique_ptr<Document> setup(){auto d=std::make_unique<Document>();d->replace(sphere());require(d->wait_mesh(),"Initial surface");d->begin_desktop();Brush b;b.tool=Tool::Carve;b.radius=.08f;b.from=b.to={0,0,.23f};d->stamp_desktop(b);d->end_desktop();require(d->undo(),"Create retained redo");require(d->wait_mesh(),"Redo baseline ready");
  // Prime only global cache bookkeeping capacity, then discard the entry.
  // Every measured/faulted native worker still performs the same COLD index
  // construction; LRU growth from prior cases cannot shift its late boundaries.
  auto held=d->copy_volume();held.raycast({0,0,1},{0,0,-1},2);dome::sculpt_detail::clear_ray_index_cache();return d;}
void gesture(Document &d){require(d.begin_native_stroke(),"Start native allocation fixture");NativeStrokeSample s;s.origin={0,0,1};s.direction={0,0,-1};s.brush.tool=Tool::Carve;s.brush.radius=.08f;s.brush.blend=0;require(d.sample_native_stroke(s)||hit.load(),"Submit native allocation fixture");d.end_native_stroke();require(d.wait_native_stroke(),"Native allocation worker drains");}
}
int main()try{
 // Device runtimes perform process-global JIT/plugin setup only on the first
 // native worker. Warm that uninstrumented path before counting comparable
 // worker transactions. setup() still clears the ray index for EVERY measured
 // and faulted worker; actual injection and exact rollback remain mandatory.
 if(compute_backend()!="CPU") {
   auto warm=setup();gesture(*warm);
   require(warm->frame().state->error.empty(),"GPU native warm-up succeeds");
 }
 auto measured=setup();calls=0;failure=0;tracking=true;gesture(*measured);tracking=false;size_t count=calls.load();require(count>32,"Measured worker allocation coverage");require(measured->frame().state->error.empty(),"Measured transaction succeeds");measured.reset();
 std::set<size_t> boundaries;for(size_t n=1;n<count;n*=2)boundaries.insert(n);for(size_t n=count>48?count-48:1;n<=count;++n)boundaries.insert(n);
 for(auto boundary:boundaries){auto d=setup();auto original=ordered(d->copy_volume());auto tokens=DocumentNativeTestAccess::tokens(*d);auto revision=d->frame().state->revision;calls=0;failure=boundary;hit=false;tracking=true;gesture(*d);tracking=false;failure=0;
  if(!hit)std::cerr<<"Unreached boundary "<<boundary<<" / measured "<<count<<"; actual worker allocations "<<calls.load()<<"\n";
  require(hit,"Every selected worker boundary was actually injected");require(ordered(d->copy_volume())==original,"Allocation failure preserves exact original sculpt");require(DocumentNativeTestAccess::tokens(*d)==tokens,"Allocation failure preserves undo/redo/content/nextContent identities");require(d->frame().state->revision==revision&&!d->frame().state->native_pending&&!d->frame().state->error.empty(),"Allocation failure clears pending lease and publishes ordinary error status without revision change");require(d->redo(),"Prior redo survives allocation failure");
 }
 std::cout<<"PASS "<<boundaries.size()<<" measured preparation/late-publication worker allocation boundaries through "<<count<<" allocations; exact scene, redo/content/nextContent preserved\n";return 0;
}catch(const std::exception&e){tracking=false;failure=0;std::cerr<<e.what()<<'\n';return 1;}
