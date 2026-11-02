// SPDX-License-Identifier: GPL-3.0-or-later
#include "current-color-target-fixture.hpp"
#include "dome/document.hpp"
#include "dome/uv.hpp"
#include "dome/source-uv-bake.hpp"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#include <numeric>
namespace allocation {
thread_local bool active=false,persistent=true;thread_local long remaining=-1;thread_local size_t calls=0,hits=0;
void visit(){if(!active)return;++calls;if(remaining==0){++hits;if(!persistent)remaining=-1;throw std::bad_alloc();}if(remaining>0)--remaining;}
void begin(long n,bool deny=true){calls=hits=0;remaining=n;persistent=deny;active=true;}void stop(){active=false;}
}
void *operator new(size_t n){allocation::visit();if(auto p=std::malloc(std::max(n,size_t(1))))return p;throw std::bad_alloc();}void *operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
void *operator new(size_t n,std::align_val_t align){allocation::visit();void*p=nullptr;if(posix_memalign(&p,size_t(align),std::max(n,size_t(1))))throw std::bad_alloc();return p;}void*operator new[](size_t n,std::align_val_t a){return ::operator new(n,a);}void operator delete(void*p,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,std::align_val_t)noexcept{std::free(p);}void operator delete(void*p,size_t,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,size_t,std::align_val_t)noexcept{std::free(p);}
using namespace dome;
namespace {void require(bool v,const char *s){if(!v)throw std::runtime_error(s);}void ready(Document &d){require(d.wait_asset()&&d.wait_mesh()&&d.frame().state->error.empty(),"UV fault fixture worker failure");}}
int main(int argc,char**argv)try{
 const bool measure=argc>1&&std::string_view(argv[1])=="--measure";
 const size_t shard=argc>1&&!measure?std::stoull(argv[1]):0,shards=argc>2?std::stoull(argv[2]):1;require(shards&&shard<shards,"Invalid UV fault shard");
 auto initial=current_color_target_fixture::fixture(true);Document document;document.replace(initial);ObjectBakeSettings settings;settings.prepare_colors=true;settings.ao_samples=0;document.bake_from_object(initial.objects().back().id,settings);ready(document);const auto scene=document.copy_scene();const auto before=scene.encode();const auto &source=scene.active();
 UvWorkspace workspace(source.painted->mesh(),64);const auto &mesh=source.painted->mesh();size_t face=0;while(face<mesh.indices.size()/3&&mesh.normals[mesh.indices[face*3]].z<.9f)++face;std::array<float,2> q{};for(size_t k=0;k<3;++k){auto uv=mesh.uv[mesh.indices[face*3+k]];q[0]+=uv[0]/3;q[1]+=uv[1]/3;}workspace.edit(UvAction::Pick,q[0],q[1]);workspace.edit(UvAction::Move,1.f/64,0);auto view=workspace.view();auto derivative=prepare_polygon_uv_derivative(source,*view.mesh,*view.source_vertices,{});
 SourceUvBakeInput input;input.paint=source.painted;input.current_material=source.material_bindings;input.current_deformation=source.deformation;input.polygon_authority=source.polygon;input.target_polygon=derivative.authority;input.target=derivative.mesh;input.source_vertices=derivative.source_vertices;
 auto operation=[&]{auto result=bake_source_uv(input);auto adopted=adopt_source_uv_bake(result);Scene next=scene.snapshot();auto &o=next.active();o.painted=std::move(adopted.paint);o.material_bindings=std::move(adopted.material_bindings);o.deformation=std::move(adopted.deformation);o.appearance=std::move(adopted.appearance);o.polygon=derivative.authority;next.validate();return next;};
 const auto expected=operation().encode();allocation::begin(-1);auto measured=operation();const auto count=allocation::calls;allocation::stop();require(measured.encode()==expected,"Measured UV operation differs");const size_t begin=count*shard/shards,end=count*(shard+1)/shards;std::cout<<"MEASURE "<<count<<" RANGE "<<begin<<' '<<end<<std::endl;if(measure)return 0;
 size_t refusals=0,optional=0;for(size_t boundary=begin;boundary<end;++boundary){bool failed=false;std::optional<Scene> changed;allocation::begin(long(boundary));try{changed=operation();}catch(const std::bad_alloc&){failed=true;}catch(...){allocation::stop();throw;}const auto hit=allocation::hits;allocation::stop();require(hit>0,"Measured actual UV allocation boundary was not reached");require(scene.encode()==before&&scene.active().polygon==source.polygon&&scene.active().material_bindings==source.material_bindings,"Failed UV allocation mutated authoritative input/owners");if(failed)++refusals;else{require(changed&&changed->encode()==expected,"Optional allocation success differs from actual accepted model");++optional;}require(operation().encode()==expected,"Actual UV retry differs after allocation refusal");if((boundary&255)==0)std::cout<<"BOUNDARY "<<boundary<<std::endl;}
 std::cout<<"PASS "<<(end-begin)<<" actual persistent UV23 allocation boundaries; refusals="<<refusals<<" optional="<<optional<<std::endl;return 0;
}catch(const std::exception&e){allocation::stop();std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
