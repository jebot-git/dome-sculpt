// SPDX-License-Identifier: GPL-3.0-or-later
#include "normal-domain-fixtures.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
namespace allocation {thread_local bool armed=false,hit=false;thread_local size_t calls=0,cut=0;}
void *operator new(size_t n){if(allocation::armed&&++allocation::calls>=allocation::cut&&allocation::cut){allocation::hit=true;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}void operator delete(void *p)noexcept{std::free(p);}void operator delete[](void *p)noexcept{std::free(p);}void operator delete(void *p,size_t)noexcept{std::free(p);}void operator delete[](void *p,size_t)noexcept{std::free(p);}
int main()try{
 using namespace dome;const auto source=normal_domain_fixture::model();const auto before=source.encode();const auto paired=encode_normal_surface(source,source.active().id);
 size_t total=0;
 for(int operation=0;operation<2;++operation){
  allocation::cut=allocation::calls=0;allocation::armed=true;
  if(operation==0){auto out=encode_normal_surface(source,source.active().id);(void)out;}else{auto out=decode_normal_surface(paired);(void)out;}
  allocation::armed=false;const auto measured=allocation::calls;if(measured<100)throw std::runtime_error("Paired source graph did not exercise a genuine archive allocation path");size_t refused=0,completed=0;
  for(size_t cut=1;cut<=measured;++cut){allocation::calls=0;allocation::cut=cut;allocation::hit=false;bool failed=false;
   allocation::armed=true;try{if(operation==0){auto out=encode_normal_surface(source,source.active().id);(void)out;}else{auto out=decode_normal_surface(paired);(void)out;}}catch(const std::bad_alloc&){failed=true;}allocation::armed=false;
   if(!allocation::hit)continue;if(failed)++refused;else ++completed;
   if(source.encode()!=before)throw std::runtime_error("Persistent paired archive denial changed original source/frame/paint authority");
  }
  if(decode_normal_surface(paired).encode()!=before)throw std::runtime_error("Paired source graph cannot retry exactly after persistent denial");
  total+=refused+completed;std::cout<<"PASS paired operation="<<operation<<" measured="<<measured<<" denied="<<refused<<" cache-only="<<completed<<std::endl;
 }
 std::cout<<"PASS "<<total<<" reached paired archive allocation boundaries\n";return 0;
}catch(const std::exception &error){allocation::armed=false;std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
