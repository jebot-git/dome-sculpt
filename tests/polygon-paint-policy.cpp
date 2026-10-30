// SPDX-License-Identifier: GPL-3.0-or-later
#include "polygon-paint.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
#include <cstdlib>
#include <new>
namespace {thread_local bool measuring=false;thread_local size_t allocations=0,hits=0;thread_local long fail=-1;}
[[gnu::noinline]] void *operator new(size_t n){if(measuring){++allocations;if(fail==0){++hits;throw std::bad_alloc();}if(fail>0)--fail;}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](size_t n){return ::operator new(n);}
[[gnu::noinline]] void operator delete(void*p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void*p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void*p,size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void*p,size_t)noexcept{std::free(p);}
using namespace dome;
namespace {unsigned checks=0;void check(bool v,const char *m){if(!v)throw std::runtime_error(m);++checks;std::cout<<"PASS "<<m<<'\n';}template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}
Mesh triangle(){Mesh m;m.positions={{0,0,0},{1,0,0},{0,1,0}};m.normals.assign(3,{0,0,1});m.uv={{{0,0}},{{1,0}},{{0,1}}};m.indices={0,1,2};return m;}}
int main()try{
 auto original=PaintedMesh::from_mesh(triangle(),64,{});original->add_layer("Authored layer");original->layer_properties("Retained paint",true,.65f);
 original->begin_stroke();Brush b;b.from=b.to={.2f,.2f,0};b.radius=.15f;PaintSettings paint;paint.color={.2f,.6f,.9f,1};original->stamp(b,paint);original->end_stroke();auto before=original->encode();
 auto mesh=triangle();mesh.positions[2].z=.1f;
 PaintSurfaceOptions options;options.policy=PaintUvPolicy::PolygonAuthored;
 check(refuses([&]{PaintedMesh::from_mesh(mesh,64,options);}),"Untextured generic factory cannot mint polygon authority");
 auto result=polygon_paint_geometry(*original,mesh,options);auto encoded=result->encode();
 check(encoded.at("uv_policy")=="polygon-authored"&&result->surface_diagnostics().policy==PaintUvPolicy::PolygonAuthored,"Private geometry preparation records its distinct polygon UV policy");
 check(encoded.at("layers")==before.at("layers")&&encoded.at("active")==before.at("active")&&encoded.at("next_id")==before.at("next_id"),"Geometry replacement retains exact paint tile bytes, layer properties and identities");
 check(original->encode()==before&&original->mesh().positions[2].z==0&&result->mesh().positions[2].z==.1f,"Detached replacement never mutates its source owner");
 check(refuses([&]{PaintedMesh::decode(encoded);})&&refuses([&]{PaintedMesh::decode(encoded,nullptr,false);}),"Existing standalone decoder and explicit false reject unbound polygon payload");
 auto loaded=PaintedMesh::decode(encoded,nullptr,true);check(loaded->encode()==encoded,"Explicit Scene12 structural decoder preserves policy and full paint exactly pending external witness validation");
 auto old=encoded;old["uv_policy"]="source-authored";check(refuses([&]{PaintedMesh::decode(old,nullptr,true);}),"Polygon context never relaxes original-source texture authority");
 old=encoded;old["uv_policy"]="unknown";check(refuses([&]{PaintedMesh::decode(old,nullptr,true);}),"Explicit polygon context rejects unknown policy versions");
 options.cancelled=[] {return true;};check(refuses([&]{polygon_paint_geometry(*original,mesh,options);})&&original->encode()==before,"Cancellation before detached allocation keeps original paint exact");
 size_t calls=0;options.cancelled=[&]{++calls;return false;};auto measured=polygon_paint_geometry(*original,mesh,options);check(calls>2,"Real surface raster preparation exposes cooperative cancellation checkpoints");
 const auto total=calls;for(size_t boundary=1;boundary<=total;++boundary){calls=0;options.cancelled=[&]{return ++calls==boundary;};if(!refuses([&]{polygon_paint_geometry(*original,mesh,options);})||original->encode()!=before)throw std::runtime_error("Cancelled detached paint preparation changed source");}
 check(true,"Every measured cancellation boundary retains complete source and successful retry");
 options.cancelled={};check(polygon_paint_geometry(*original,mesh,options)->encode()==measured->encode(),"Successful retry is deterministic after cancelled preparations");
 allocations=0;fail=-1;measuring=true;auto allocated=polygon_paint_geometry(*original,mesh,options);measuring=false;const auto boundaries=allocations;
 check(boundaries>10,"Measure real detached surface and snapshot allocations before publication");
 for(size_t boundary=0;boundary<boundaries;++boundary){bool caught=false;hits=0;fail=long(boundary);measuring=true;
  try{auto ignored=polygon_paint_geometry(*original,mesh,options);}catch(const std::bad_alloc&){caught=true;}catch(...){measuring=false;throw;}
  measuring=false;fail=-1;
  if(!caught||!hits||original->encode()!=before)throw std::runtime_error("Injected owned paint preparation allocation failed atomicity or missed boundary");
 }
 check(polygon_paint_geometry(*original,mesh,options)->encode()==allocated->encode(),"Every measured persistent allocation failure preserves exact source paint and deterministic retry");
 std::cout<<"PASS "<<checks<<" paint policy checks / "<<total<<" cancellation / "<<boundaries<<" allocation boundaries\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
