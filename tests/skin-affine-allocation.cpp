// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_skin_affine_test
#include "skin-affine.cpp"
#undef main
#include <cstdlib>
#include <new>
#ifdef DOME_TEST_WEIGHT_TRANSFER_GPU
#include "weight-transfer-device.hpp"
namespace {
bool deny_device=false,denied_device=false;
using dome::detail::weight_transfer::Query;
using dome::detail::weight_transfer::Surface;
}
std::unique_ptr<Query> real_upload(const Surface &)
  asm("__real__ZN4dome6detail15weight_transfer6uploadERKNS1_7SurfaceE");
std::unique_ptr<Query> wrapped_upload(const Surface &)
  asm("__wrap__ZN4dome6detail15weight_transfer6uploadERKNS1_7SurfaceE");
std::unique_ptr<Query> wrapped_upload(const Surface &surface){
  if(deny_device){denied_device=true;throw std::bad_alloc();}
  return real_upload(surface);
}
#endif
namespace allocation {thread_local bool armed=false,hit=false;thread_local size_t calls=0,fail=0;}
void *operator new(size_t n){if(allocation::armed&&++allocation::calls>=allocation::fail&&allocation::fail){allocation::hit=true;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void *p)noexcept{std::free(p);}
void operator delete[](void *p)noexcept{std::free(p);}
void operator delete(void *p,size_t)noexcept{std::free(p);}
void operator delete[](void *p,size_t)noexcept{std::free(p);}
int main()try{
 size_t total=0;
#ifdef DOME_TEST_WEIGHT_TRANSFER_GPU
 constexpr std::array modes{SkinAuthoringMode::Automatic};
#else
 constexpr std::array modes{SkinAuthoringMode::Automatic,SkinAuthoringMode::Transfer};
#endif
 for(auto mode:modes){
  auto source=fixture(2);auto before=source.encode();auto operation=options();operation.mode=mode;operation.seed_distance=1;auto expected=source.snapshot();
  allocation::calls=allocation::fail=0;allocation::armed=true;author_skin(expected,2,operation);allocation::armed=false;auto count=allocation::calls;
  auto result=expected.encode();check(count>20,"Real affine solver/publication allocation boundaries measured");
  for(size_t at=1;at<=count;++at){auto scene=source.snapshot();auto old=scene.active().deformation;auto mesh=scene.active().painted->mesh_owner();
   allocation::calls=0;allocation::fail=at;allocation::hit=false;bool rejected=false;
   allocation::armed=true;try{author_skin(scene,2,operation);}catch(const std::bad_alloc&){rejected=true;}allocation::armed=false;
   check(allocation::hit&&rejected,"Every persistent-denial boundary propagates");
   check(scene.encode()==before&&scene.active().deformation==old&&scene.active().painted->mesh_owner()==mesh,"Failed solve/publication preserves complete scene, rest/morph/skin/paint and owners");
   check(author_skin(scene,2,operation)&&scene.encode()==result,"Rejected affine authoring retries to exact successful endpoint");++total;
  }
  std::cout<<"PASS "<<int(mode)<<": "<<count<<" persistent allocation boundaries\n";
 }
#ifdef DOME_TEST_WEIGHT_TRANSFER_GPU
 // Inject transfer failure at the device adapter boundary. The vendor
 // runtime's own host allocations are outside the application's heap test.
 auto input=fixture(2);
 const auto before=input.encode();
 const auto old=input.active().deformation;
 const auto mesh=input.active().painted->mesh_owner();
 auto operation=options();
 operation.mode=SkinAuthoringMode::Transfer;
 auto expected=input.snapshot();
 check(author_skin(expected,2,operation),"A healthy GPU publishes the affine transfer");
 deny_device=true;
 bool rejected=false;
 try{(void)author_skin(input,2,operation);}catch(const std::bad_alloc &){rejected=true;}
 deny_device=false;
 check(denied_device&&rejected&&input.encode()==before&&input.active().deformation==old&&
       input.active().painted->mesh_owner()==mesh,"Failed GPU preparation preserves the exact scene and mesh owners");
 check(author_skin(input,2,operation)&&input.encode()==expected.encode(),"Retry after GPU setup failure recovers the exact transfer");
 std::cout<<"PASS GPU setup failure and exact affine transfer retry\n";
#endif
 std::cout<<"PASS "<<total<<" affine skin allocation boundaries\n";return 0;
}catch(const std::exception &e){allocation::armed=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
