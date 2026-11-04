// SPDX-License-Identifier: GPL-3.0-or-later
#include "geometry-reference-replay.hpp"
#include <iostream>
#include <cstdlib>
#include <new>
#include <chrono>
thread_local bool trace_allocations=false;thread_local size_t allocations=0;
void*operator new(size_t n){if(trace_allocations)++allocations;auto p=std::malloc(n?n:1);if(!p)throw std::bad_alloc();return p;}
void*operator new[](size_t n){return ::operator new(n);}
void operator delete(void*p)noexcept{std::free(p);}
void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}
void operator delete[](void*p,size_t)noexcept{std::free(p);}
using namespace dome;using namespace dome::detail;
namespace {size_t checks=0;void check(bool yes,const char*s){if(!yes)throw std::runtime_error(s);++checks;std::cout<<"PASS "<<s<<std::endl;}template<class F>bool bad(F f){try{f();return false;}catch(const std::exception&){return true;}}}
int main()try{
 auto source=std::make_shared<DeformationState>();Skeleton s;for(int n=0;n<54;++n){Bone b;b.name="joint"+std::to_string(n);b.parent=n-1;b.rest.translation={0,.01f,0};b.length=.02f;s.bones.push_back(b);}s.validate();source->skeleton=s;
 const std::function<bool()>never;weight_surface::Budget work{0,500000000,never};GeometryReferenceReplay replay(source);size_t peak=0;
 check(replay.matches(s,{},true,work,1024*1024,&peak)&&replay.canonical_builds()==0,"Exact absent-space source accepts without canonical allocation");
 SkinGeometryWork g(100000,never);auto target=skin_reframe(s,{},g);size_t oldwork=0;
 for(int n=0;n<19;++n){weight_surface::Budget old{0,500000000,never};check(geometry_reference_rig_matches(s,target,{},true,old,1024*1024)&&replay.matches(target,{},true,work,1024*1024,&peak),"Every target agrees with independent original exact replay");oldwork+=old.used;}
 check(replay.canonical_builds()==1&&work.used<oldwork&&peak>0,"Nineteen full target comparisons share one actual allocating canonical reconstruction");
 const auto baseline=target;
 auto refuse=[&](Skeleton v,const char*label){const auto count=replay.canonical_builds();check(!replay.matches(v,{},true,work,1024*1024,&peak)&&replay.canonical_builds()==count,label);};
 {auto t=baseline;t.bones[7].length=std::nextafter(t.bones[7].length,1.f);refuse(t,"One-bit current length change is independently checked after cache reuse");}
 {auto t=baseline;t.bones[7].deform=false;refuse(t,"Current deform-role changes cannot borrow canonical proof");}
 {auto t=baseline;t.bones[7].name+="x";refuse(t,"Every current joint name remains compared");}
 {auto t=baseline;t.bones[7].parent=0;refuse(t,"Every current parent identity remains compared");}
 {auto t=baseline;t.bones[7].rest.translation.x=-0.f;refuse(t,"Signed-zero current rest value remains bit exact");}
 {auto t=baseline;t.bones[7].rest_matrix=AffineTransform{}.float_matrix();refuse(t,"Current optional rest-matrix presence cannot disappear");}
 {auto t=baseline;t.inverse_bind_override=std::vector<RigMatrix>(54,AffineTransform{}.float_matrix());refuse(t,"Current inverse-bind presence remains independent");}
 {auto t=baseline;t.space_matrix->at(12)=.01f;refuse(t,"Every current bind-space matrix component remains independent");}
 // The original node is legally mutable; the production owning Scene contract
 // uses its const view. This target is changed BETWEEN comparisons, never UB.
 auto mutable_target=std::make_shared<DeformationState>();mutable_target->skeleton=baseline;check(replay.matches(*mutable_target->skeleton,{},true,work,1024*1024),"Legally mutable test target baseline agrees");mutable_target->skeleton->bones[7].length+=.01f;check(!replay.matches(*mutable_target->skeleton,{},true,work,1024*1024),"Same-address target mutation does not reuse a target proof");
 {GeometryReferenceReplay small(source);weight_surface::Budget w{0,500000000,never};check(bad([&]{small.matches(baseline,{},true,w,4096);})&&small.canonical_builds()==0,"Workspace refusal happens before allocating a canonical skeleton");}
 {GeometryReferenceReplay limited(source);weight_surface::Budget w{0,100,never};check(bad([&]{limited.matches(baseline,{},true,w,1024*1024);})&&limited.canonical_builds()==0,"Charged work refusal happens before canonical construction");}
 {const std::function<bool()>cancel=[] {return true;};GeometryReferenceReplay stopped(source);weight_surface::Budget w{0,500000000,cancel};check(bad([&]{stopped.matches(baseline,{},true,w,1024*1024);}),"Real cancellation remains active during reference replay");}
 {GeometryReferenceReplay other(source);weight_surface::Budget w{0,500000000,never};check(other.matches(baseline,{},false,w,1024*1024)&&other.matches(baseline,{},false,w,1024*1024)&&other.canonical_builds()==0,"Non-same-frame path stays original uncached exact algorithm");}
 size_t original_allocations=0,shared_allocations=0;double original_seconds=0,shared_seconds=0;
 {GeometryReferenceReplay measured(source);weight_surface::Budget shared{0,500000000,never};
  for(size_t n=0;n<19;++n){weight_surface::Budget old{0,500000000,never};allocations=0;trace_allocations=true;const auto start=std::chrono::steady_clock::now();const bool ok=geometry_reference_rig_matches(s,baseline,{},true,old,1024*1024);original_seconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();trace_allocations=false;original_allocations+=allocations;if(!ok)throw std::runtime_error("Original measured exact replay failed");
   allocations=0;trace_allocations=true;const auto begin=std::chrono::steady_clock::now();const bool yes=measured.matches(baseline,{},true,shared,1024*1024);shared_seconds+=std::chrono::duration<double>(std::chrono::steady_clock::now()-begin).count();trace_allocations=false;shared_allocations+=allocations;if(!yes)throw std::runtime_error("Shared measured exact replay failed");}
 }
 check(shared_allocations>0&&original_allocations==19*shared_allocations,"Independent allocator observes nineteen-to-one actual reconstruction allocations");
 std::cout<<"MEASURE original_allocations="<<original_allocations<<" shared_allocations="<<shared_allocations<<" original_seconds="<<original_seconds<<" shared_seconds="<<shared_seconds<<std::endl;
 {GeometryReferenceReplay mixed(source);weight_surface::Budget w{0,500000000,never};check(mixed.matches(baseline,{},true,w,1024*1024)&&mixed.retained_workspace()>0,"Same-frame replay owns its explicitly charged canonical data");
  check(bad([&]{mixed.matches(baseline,{},true,w,4096);}),"A reused canonical owner cannot ignore a smaller workspace bound");
  check(mixed.matches(baseline,{},false,w,1024*1024)&&mixed.retained_workspace()==0,"Relative-frame replay releases cached allocation before original uncached work");}
 source->skeleton->bones[7].length+=.01f;GeometryReferenceReplay fresh(source);weight_surface::Budget newwork{0,500000000,never};check(!fresh.matches(baseline,{},true,newwork,1024*1024),"A new top-level call cannot borrow an earlier source proof");
 std::cout<<"Reference replay checks "<<checks<<" originalwork="<<oldwork<<" sharedwork="<<work.used<<" canonicalbuilds="<<replay.canonical_builds()<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
