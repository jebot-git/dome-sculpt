// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/rig.hpp"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <new>
#include <iostream>
namespace {thread_local bool counting=false;thread_local size_t allocations=0;}
[[gnu::noinline]]void *operator new(size_t n){if(counting)++allocations;if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]]void *operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
using namespace dome;
size_t checks=0;void check(bool good,const char *text){if(!good)throw std::runtime_error(text);++checks;std::cout<<"PASS "<<text<<std::endl;}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}
int main()try{
 for(bool reverse:{false,true}){Skeleton rig;for(size_t i=0;i<256;++i){Bone bone;bone.name="b"+std::to_string(i);bone.parent=reverse?(i==255?-1:int(i+1)):(i?int(i-1):-1);bone.length=.1f;bone.rest.translation.y=.001f;rig.bones.push_back(bone);}allocations=0;counting=true;try{rig.validate();}catch(...){counting=false;throw;}counting=false;std::cout<<"RIG_VALIDATION reverse="<<reverse<<" bones=256 allocations="<<allocations<<std::endl;check(allocations<=512,"Maximum legal ordered/unordered ancestry has bounded prospective allocation work");
  const auto local=rig.local_matrices(),world=rig.world(),binds=rig.inverse_bind();float y=0;for(size_t depth=0;depth<256;++depth){y+=.001f;const size_t at=reverse?255-depth:depth;check(world[at][13]==y&&local[at][13]==.001f,"All256 exact hierarchy endpoints retain existing float transform arithmetic");}auto copy=Skeleton::decode(rig.encode());check(copy.encode()==rig.encode()&&copy.world()==world&&copy.inverse_bind()==binds,"Maximum-depth rig rest/bind/native persistence remains exact");
  auto wrong=rig;wrong.bones[reverse?255:0].parent=reverse?0:255;check(refuses([&]{wrong.validate();}),"Full256-node ancestry cycle refuses before any out-of-range membership access");wrong=rig;wrong.bones[0].parent=-2;check(refuses([&]{wrong.validate();}),"Negative out-of-range parent remains refused");wrong=rig;wrong.bones[0].parent=256;check(refuses([&]{wrong.validate();}),"Positive out-of-range parent remains refused");wrong=rig;wrong.bones[0].parent=0;check(refuses([&]{wrong.validate();}),"Direct self-parent remains refused");wrong=rig;wrong.bones[255].name=wrong.bones[0].name;check(refuses([&]{wrong.validate();}),"Actual duplicate bone names remain refused");wrong=rig;wrong.bones.push_back(Bone{});check(refuses([&]{wrong.validate();}),"Native256-bone maximum remains unchanged");
 }
 Skeleton empty;check(refuses([&]{empty.validate();}),"Empty skeleton remains refused");std::cout<<"PASS "<<checks<<" bounded rig ancestry checks\n";return 0;
}catch(const std::exception&e){counting=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
