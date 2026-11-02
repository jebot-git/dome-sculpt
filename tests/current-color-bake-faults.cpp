// SPDX-License-Identifier: GPL-3.0-or-later
#include "current-color-bake-fixture.hpp"
#include "dome/bake-authoring.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
using namespace dome;
namespace {
thread_local bool tracking=false;
bool enabled=false,hit=false;size_t calls=0,boundary=SIZE_MAX;
std::string mode;
void require(bool yes,const char *why){if(!yes)throw std::runtime_error(why);}
}
[[gnu::noinline]] void *operator new(size_t n){if(tracking&&enabled&&++calls>=boundary){hit=true;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](size_t n){return ::operator new(n);}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p,size_t)noexcept{std::free(p);}
extern "C" std::vector<PolygonRgba> packed_real(const PolygonColorField*,const PolygonMesh&,const PolygonLimits&) asm("__real__ZNK4dome17PolygonColorField6packedERKNS_11PolygonMeshERKNS_13PolygonLimitsE");
extern "C" std::vector<PolygonRgba> packed_wrap(const PolygonColorField*,const PolygonMesh&,const PolygonLimits&) asm("__wrap__ZNK4dome17PolygonColorField6packedERKNS_11PolygonMeshERKNS_13PolygonLimitsE");
extern "C" std::vector<PolygonRgba> packed_wrap(const PolygonColorField *field,const PolygonMesh &mesh,const PolygonLimits &limits){
 const bool old=tracking;if(enabled&&mode=="packed")tracking=true;
 try{auto result=packed_real(field,mesh,limits);tracking=old;return result;}catch(...){tracking=old;throw;}
}
int main(int argc,char **argv)try{
 require(argc==2||argc==4,"Choose actual packed or precommit fault phase and optional shard/count");mode=argv[1];require(mode=="packed"||mode=="precommit","Declared current-source Bake fault phase");set_paint_composite_cache_budget(0);
 const size_t shard=argc==4?std::stoul(argv[2]):0,shards=argc==4?std::stoul(argv[3]):1;
 require(shards>0&&shards<=16&&shard<shards,"Declared bounded fault shard");
 auto original=current_color_bake_fixture::fixture(true,true);auto source=original.objects().front().id;
 original.active().painted=std::make_shared<PaintedMesh>(original.active().painted->mesh(),64);original.validate();auto before=original.encode();
 ObjectBakeSettings settings;settings.prepare_colors=true;settings.ao_samples=0;settings.padding=4;
 auto progress=[](float value){if(mode=="precommit"){if(value>=.89999f&&value<1)tracking=enabled;if(value==1)tracking=false;}};
 auto measured=original.snapshot();calls=0;boundary=SIZE_MAX;hit=false;enabled=true;bake_object(measured,source,settings,{},progress);tracking=enabled=false;auto count=calls;
 require(count>1&&count<8192,"Actual bounded owned field/commit allocations measured");auto expected=measured.encode();
 std::cout<<mode<<" actual owned allocation count "<<count<<std::endl;
 const size_t first=1+count*shard/shards,last=count*(shard+1)/shards;
 require(first<=last,"Each registered fault shard contains real measured boundaries");
 std::cout<<"Testing complete boundary range "<<first<<".."<<last<<" of "<<count<<std::endl;
 for(size_t at=first;at<=last;++at){
  auto scene=original.snapshot();auto paint=scene.active().painted;auto field=scene.object(source).polygon->colors;auto capsule=scene.object(source).shape_source;auto binding=scene.active().material_bindings;
  calls=0;boundary=at;hit=false;enabled=true;bool failed=false;try{bake_object(scene,source,settings,{},progress);}catch(const std::bad_alloc&){failed=true;}tracking=enabled=false;
  require(hit&&failed,"Every measured current-source Bake allocation actually denies the requested phase");
  require(scene.encode()==before&&scene.active().painted==paint&&scene.active().material_bindings==binding&&scene.object(source).polygon->colors==field&&scene.object(source).shape_source==capsule,"Failed source Bake retains complete target/source data and exact immutable/mutable authority owners");
  auto result=bake_object(scene,source,settings);require(result.projected&&scene.encode()==expected,"Actual failed current-source Bake retries deterministically with the full original source retained");
  if((at&31)==0)std::cout<<"Reached "<<mode<<" owned failure "<<at<<std::endl;
 }
 std::cout<<"PASS all "<<(last-first+1)<<" actual "<<mode<<" current-source Bake allocation failures in "<<first<<".."<<last<<" of "<<count<<" preserve exact Scene/owners and full-helper retry\n";return 0;
}catch(const std::exception &e){tracking=enabled=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
