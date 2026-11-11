// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/material-visibility.hpp"
#include <iostream>
#include <optional>
#include <stdexcept>
using namespace dome;
namespace {
size_t checks=0;
void check(bool ok,const char*why){if(!ok)throw std::runtime_error(why);++checks;}
Mesh fixture(){Mesh m;for(uint32_t i=0;i<64;++i){const float x=float(i)*.1f;const auto first=uint32_t(m.positions.size());m.positions.insert(m.positions.end(),{{x,0,0},{x+.01f,0,0},{x,.01f,0}});m.normals.insert(m.normals.end(),3,Vec3{0,0,1});m.indices.insert(m.indices.end(),{first,first+1,first+2});}m.validate();return m;}
struct Payload {std::vector<uint8_t>bytes=std::vector<uint8_t>(1<<20,42);};
bool equal(const SurfaceQueryResult&a,const SurfaceQueryResult&b){return a.surface.triangle==b.surface.triangle&&a.surface.position==b.surface.position&&a.surface.normal==b.surface.normal&&a.surface.barycentric==b.surface.barycentric&&a.surface.distance==b.surface.distance&&a.squared_distance==b.squared_distance;}
template<class F>bool cancels(F f){try{f();}catch(const std::runtime_error&e){return std::string(e.what())=="Mesh processing cancelled";}return false;}
void copied_query(){
 auto mesh=fixture();SurfaceQuery reference(mesh);auto owner=std::make_shared<Payload>();std::weak_ptr<Payload>weak=owner;size_t calls=0;
 std::function<bool()>callback=[keep=owner,&calls]{++calls;return keep->bytes.front()!=42;};
 SurfaceQuery query(mesh,callback);check(calls>3,"Real copied-query construction invokes its build callback inside the BVH");const auto built=calls;
 owner.reset();callback={};std::cout<<"COPIED_CALLBACK retained_after_build="<<!weak.expired()<<" build_calls="<<built<<'\n';
 check(weak.expired(),"Copied SurfaceQuery must release its build-only captured owner before publication");
 const Vec3 point{.002f,.003f,.1f};const auto a=query.nearest_result(point,{}),b=reference.nearest_result(point,{});check(a&&b&&equal(*a,*b),"Releasing the build callback preserves exact binary64 nearest result, IDs, barycentrics and authored normal");
 check(a->surface.triangle==0&&std::abs(a->surface.barycentric[0]-.5f)<1e-6f&&std::abs(a->surface.barycentric[1]-.2f)<1e-6f&&std::abs(a->surface.barycentric[2]-.3f)<1e-6f,"Nearest point retains an independent analytic triangle/barycentric oracle");
 const auto ray=query.raycast_result(point,{0,0,-1},1,{}),expected=reference.raycast_result(point,{0,0,-1},1,{});check(ray&&expected&&equal(*ray,*expected),"Cleanup preserves exact native ray metric/position/normal results");
 check(calls==built,"Subsequent native queries do not re-invoke a completed construction callback");
 SurfaceQueryOptions options;size_t active=0;options.cancelled=[&]{++active;return true;};check(cancels([&]{query.nearest(point,options);})&&active>0,"Per-query cancellation remains active independently of construction callback cleanup");
 mesh.positions[0].x=99;check(query.mesh().positions[0].x==0&&query.nearest_result(point,{})->squared_distance==a->squared_distance,"Public query keeps its defensive geometry copy after caller mutation");
}
void material_query(){
 const auto mesh=fixture();SurfaceRange range{17,0,mesh.indices.size(),Material{}};auto owner=std::make_shared<Payload>();std::weak_ptr<Payload>weak=owner;size_t calls=0;std::function<bool()>callback=[keep=owner,&calls]{++calls;return keep->bytes.front()!=42;};
 MaterialSurfaceQuery query(mesh,std::span(&range,1),callback);check(calls>3,"Actual material-query composition reaches native copied BVH construction");const auto built=calls;owner.reset();callback={};check(weak.expired(),"Published MaterialSurfaceQuery must not retain its worker's build callback owner");
 const auto hit=query.raycast_target({.002f,.003f,.1f},{0,0,-1},1);check(hit&&hit->packed()&&hit->object==17&&hit->surface.triangle==0,"Material ownership/global triangle identity remains exact after callback owner release");check(calls==built,"Material picking does not call a completed construction callback");
}
void construction_errors(){
 const auto mesh=fixture();size_t full=0;{SurfaceQuery measured(mesh,[&]{++full;return false;});}check(full>3,"Cancellation controls measure actual complete BVH callback boundaries");
 for(const size_t stop:{size_t(1),full/2,full}){auto owner=std::make_shared<Payload>();std::weak_ptr<Payload>weak=owner;size_t calls=0;std::function<bool()>callback=[keep=owner,&calls,stop]{return ++calls==stop;};std::optional<SurfaceQuery>result;check(cancels([&]{result.emplace(mesh,callback);})&&!result&&calls==stop,"Original first/mid/last construction cancellation still refuses any partial query publication");owner.reset();callback={};check(weak.expired(),"Cancelled construction releases every copied build callback capture");SurfaceQuery retry(mesh);check(retry.closest({.002f,.003f,.1f}).triangle==0,"Actual native retry succeeds after cancelled construction");}
 auto owner=std::make_shared<Payload>();std::weak_ptr<Payload>weak=owner;size_t calls=0;std::function<bool()>callback=[keep=owner,&calls,full]{if(++calls==full/2)throw std::runtime_error("test build callback exception");return false;};bool threw=false;try{SurfaceQuery rejected(mesh,callback);}catch(const std::runtime_error&e){threw=std::string(e.what())=="test build callback exception";}owner.reset();callback={};check(threw&&weak.expired(),"Thrown callback exceptions propagate and release the partial query's captured owner");
}
}
int main()try{copied_query();material_query();construction_errors();std::cout<<"PASS "<<checks<<" native surface-query callback lifetime controls\n";return 0;}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
