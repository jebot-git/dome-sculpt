// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_skin_affine_test
#include "skin-affine.cpp"
#undef main
#include <cstdlib>
#include <new>
namespace allocation {thread_local bool armed=false,hit=false;thread_local size_t calls=0,fail=0;}
void *operator new(size_t n){if(allocation::armed&&++allocation::calls>=allocation::fail&&allocation::fail){allocation::hit=true;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void *p)noexcept{std::free(p);}
void operator delete[](void *p)noexcept{std::free(p);}
void operator delete(void *p,size_t)noexcept{std::free(p);}
void operator delete[](void *p,size_t)noexcept{std::free(p);}
int main()try{
 size_t total=0;
 for(auto mode:{SkinAuthoringMode::Automatic,SkinAuthoringMode::Transfer}){
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
 std::cout<<"PASS "<<total<<" affine skin allocation boundaries\n";return 0;
}catch(const std::exception &e){allocation::armed=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
