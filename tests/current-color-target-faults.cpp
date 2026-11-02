// SPDX-License-Identifier: GPL-3.0-or-later
#include "current-color-target-fixture.hpp"
#include "dome/bake-authoring.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
namespace {
using namespace dome;
thread_local bool tracking=false;
bool enabled=false,hit=false;size_t calls=0,boundary=SIZE_MAX;std::string mode;
void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
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
 try{auto value=packed_real(field,mesh,limits);tracking=old;return value;}catch(...){tracking=old;throw;}
}
int main(int argc,char **argv)try{
 require(argc==2||argc==3||argc==4,"Choose packed/precommit/encode/decode and optional shard/count or measure");mode=argv[1];require(mode=="packed"||mode=="precommit"||mode=="encode"||mode=="decode","Declared actual target fault phase");
 const bool measure_only=argc==3&&std::string_view(argv[2])=="measure";require(argc!=3||measure_only,"Declared measurement mode");
 const size_t shard=argc==4?std::stoul(argv[2]):0,shards=argc==4?std::stoul(argv[3]):1;require(shards&&shards<=16&&shard<shards,"Declared bounded fault shard");
 set_paint_composite_cache_budget(0);auto original=current_color_target_fixture::fixture(true);const auto source=original.objects().back().id;
 auto &target=original.active();target.painted=target.painted->snapshot();Brush brush;brush.from=brush.to={0,0,.2f};brush.radius=.3f;PaintSettings ink;ink.front_only=false;ink.color={.8f,.2f,.5f,.7f};target.painted->begin_stroke();target.painted->stamp(brush,ink);target.painted->end_stroke();target.painted->layer_properties("Fractional retained paint",true,.6f);target.painted->add_layer("Hidden retained paint");target.painted->layer_properties("Hidden retained paint",false,.3f);
 ObjectBakeSettings settings;settings.prepare_colors=true;settings.ao_samples=0;settings.padding=4;
 auto prepared=original.snapshot();bake_object(prepared,source,settings);const auto expected=prepared.encode();const bool bake_phase=mode=="packed"||mode=="precommit";
 const auto &input=bake_phase?original:prepared;const auto before=input.encode();
 auto progress=[](float p){if(mode=="precommit"){if(p>=.89999f&&p<1)tracking=enabled;if(p==1)tracking=false;}};
 auto operation=[&](Scene &scene){
  if(bake_phase){bake_object(scene,source,settings,{},progress);return;}
  tracking=enabled;
  if(mode=="encode"){auto value=scene.encode();tracking=false;require(value==expected,"Actual Scene20 encoding remains exact");}
  else{auto value=Scene::decode(expected);tracking=false;require(value.encode()==expected,"Actual Scene20 decoding remains exact");}
 };
 auto measured=input.snapshot();calls=0;boundary=SIZE_MAX;hit=false;enabled=true;operation(measured);tracking=enabled=false;const auto count=calls;
 require(count>1&&count<65536,"Actual bounded owned target allocation count measured");std::cout<<mode<<" actual allocation count "<<count<<std::endl;if(measure_only)return 0;
 const size_t first=1+count*shard/shards,last=count*(shard+1)/shards;require(first<=last,"Every shard contains measured real boundaries");std::cout<<"Actual range "<<first<<".."<<last<<" of "<<count<<std::endl;
 for(size_t at=first;at<=last;++at){
  auto scene=input.snapshot();auto paint=scene.active().painted;auto field=scene.active().polygon->colors;auto authority=scene.active().polygon;auto shape=scene.active().shape_source;auto binding=scene.active().material_bindings;auto source_paint=scene.object(source).painted;
  calls=0;boundary=at;hit=false;enabled=true;bool failed=false;try{operation(scene);}catch(const std::bad_alloc&){failed=true;}tracking=enabled=false;
  require(hit&&failed,"Every measured target allocation is actually denied persistently");
  require(scene.encode()==before&&scene.active().painted==paint&&scene.active().polygon==authority&&scene.active().polygon->colors==field&&scene.active().shape_source==shape&&scene.active().material_bindings==binding&&scene.object(source).painted==source_paint,"Failed target operation retains exact scene, paint, field/basis, shape, material and independent source owners");
  operation(scene);require(scene.encode()==(bake_phase?expected:before),"Actual complete target operation retries deterministically after every failure");
  require(prepared.encode()==expected,"Cached immutable prepared/input authority is unchanged by every failure and retry");
  if((at&31)==0)std::cout<<"Reached target "<<mode<<" allocation "<<at<<std::endl;
 }
 std::cout<<"PASS all "<<(last-first+1)<<" actual target "<<mode<<" failures in "<<first<<".."<<last<<" of "<<count<<" retain exact source/paint/field/capsules and full retry\n";return 0;
}catch(const std::exception &e){tracking=enabled=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
