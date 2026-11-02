// SPDX-License-Identifier: GPL-3.0-or-later
#include "prepared-normal-shapes-fixtures.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
namespace allocation {
thread_local bool armed=false,hit=false;
thread_local size_t calls=0,failure=0;
}
void *operator new(size_t n){if(allocation::armed&&++allocation::calls>=allocation::failure&&allocation::failure){allocation::hit=true;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void *p)noexcept{std::free(p);}void operator delete[](void *p)noexcept{std::free(p);}
void operator delete(void *p,size_t)noexcept{std::free(p);}void operator delete[](void *p,size_t)noexcept{std::free(p);}
int main()try{
 using namespace dome;auto source=prepared_shape_fixture::model(true);const auto before=source.encode();
 ShapeKeyEdit edit;edit.action=ShapeKeyAction::Replace;edit.positions=source.active().painted->mesh().positions;edit.positions[0].z=.08f;
 auto probe=source;allocation::failure=allocation::calls=0;allocation::armed=true;author_shapekey(probe,1,edit);allocation::armed=false;const auto measured=allocation::calls;
 if(measured<100)throw std::runtime_error("Prepared authoring did not exercise its real allocation path");
 size_t refused=0,completed=0;
 for(size_t n=1;n<=measured;++n){auto trial=source;allocation::failure=n;allocation::calls=0;allocation::hit=false;bool failed=false;
  allocation::armed=true;try{author_shapekey(trial,1,edit);}catch(const std::bad_alloc&){failed=true;}allocation::armed=false;
  if(!allocation::hit)continue; // A best-effort immutable proof cache can change allocation count.
  if(failed){if(trial.encode()!=before)throw std::runtime_error("Persistent allocation denial changed current prepared/source authority");++refused;}
  else{trial.validate();if(trial.active().deformation->morphs->targets[0].delta[0].z!=.08f)throw std::runtime_error("Best-effort cache denial published an incorrect key");++completed;}
  if(source.encode()!=before)throw std::runtime_error("Persistent denial mutated the retained source graph");
 }
 auto retry=Scene::decode(before);author_shapekey(retry,1,edit);retry.validate();
 if(retry.active().deformation->morphs->targets[0].delta[0].z!=.08f)throw std::runtime_error("Prepared replacement cannot retry after allocation denial");
 std::cout<<"PASS "<<refused<<" persistent-denial refusals, "<<completed<<" complete best-effort-cache outcomes, "<<measured<<" measured allocation boundaries; exact original/source and retry\n";return 0;
}catch(const std::exception &e){allocation::armed=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
