// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/rig-edit.hpp"
#include "dome/creator-archive.hpp"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#include <set>
#include <nlohmann/json.hpp>
namespace {
thread_local bool tracked=false;
thread_local size_t calls=0,fail_at=0;
void*allocate(size_t n,size_t alignment=0){if(tracked){++calls;if(fail_at&&calls>=fail_at)throw std::bad_alloc();}void*p=nullptr;if(alignment){if(posix_memalign(&p,alignment,n?n:1))p=nullptr;}else p=std::malloc(n?n:1);if(!p)throw std::bad_alloc();return p;}
}
void*operator new(size_t n){return allocate(n);}void*operator new[](size_t n){return allocate(n);}
void*operator new(size_t n,std::align_val_t a){return allocate(n,size_t(a));}void*operator new[](size_t n,std::align_val_t a){return allocate(n,size_t(a));}
void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}
void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
void operator delete(void*p,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,std::align_val_t)noexcept{std::free(p);}
void operator delete(void*p,size_t,std::align_val_t)noexcept{std::free(p);}void operator delete[](void*p,size_t,std::align_val_t)noexcept{std::free(p);}
using namespace dome;
int main(int argc,char**argv)try{
 if(argc!=2)throw std::invalid_argument("complete bound907 source");
 const auto source=Scene::load(argv[1]);uint64_t id=0;for(const auto&o:source.objects())if(o.appearance&&o.deformation&&o.deformation->skin){id=o.id;break;}
 if(!id)throw std::invalid_argument("Actual current rig source needed");
 RigEdit edit;edit.joint=JointAdjustment{source.object(id).deformation->humanoid->joints.at("head"),{0,0,30},{}};
 const auto before=source.encode();auto expected=source;calls=0;tracked=true;edit_rig(expected,id,edit);tracked=false;const auto total=calls;
 if(!total||expected.encode()==before)throw std::runtime_error("Measured actual changed preparation required");
 std::set<size_t>prefixes;for(size_t n=1;n<=total;n*=2)prefixes.insert(n);for(size_t n=0;n<16&&n<total;++n)prefixes.insert(total-n);
 size_t checks=0;for(auto prefix:prefixes){auto candidate=source;bool refused=false;calls=0;fail_at=prefix;tracked=true;
  try{edit_rig(candidate,id,edit);}catch(const std::bad_alloc&){refused=true;}catch(...){tracked=false;throw;}
  tracked=false;fail_at=0;
  if(!refused||calls<prefix||candidate.encode()!=before)throw std::runtime_error("Actual persistent allocation prefix did not preserve complete source");
  for(const auto&o:source.objects()){const auto&a=candidate.object(o.id);if(a.deformation!=o.deformation||a.painted!=o.painted||a.volume!=o.volume||a.appearance!=o.appearance||a.rig_source!=o.rig_source||a.shape_source!=o.shape_source||a.material_bindings!=o.material_bindings||a.bake_recipe!=o.bake_recipe)throw std::runtime_error("Failed preparation replaced a retained owner");}
  ++checks;std::cout<<"PASS actual persistent preparation prefix="<<prefix<<" reached="<<calls<<std::endl;
 }
 auto retry=source;edit_rig(retry,id,edit);if(retry.encode()!=expected.encode())throw std::runtime_error("Real retry changed its actual output");
 std::cout<<"Persistent actual pose preparation sampled="<<checks<<" measured_allocations="<<total<<"; powers-of-two and final16, not exhaustive"<<std::endl;return 0;
}catch(const std::exception&e){tracked=false;std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
