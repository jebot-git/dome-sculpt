// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include "dome/shapekey-authoring.hpp"
#include "normal-key-derivation.hpp"
#include <iostream>
#include <cstdlib>
#include <new>
namespace allocation_probe {
thread_local bool active=false;thread_local size_t calls=0,limit=SIZE_MAX;
void hit(){if(active&&calls++>=limit)throw std::bad_alloc();}
struct Guard{explicit Guard(size_t at){calls=0;limit=at;active=true;}~Guard(){active=false;}};
}
void *operator new(size_t n){allocation_probe::hit();if(void *p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void *operator new[](size_t n){return ::operator new(n);}
void operator delete(void *p)noexcept{std::free(p);}
void operator delete[](void *p)noexcept{std::free(p);}
void operator delete(void *p,size_t)noexcept{std::free(p);}
void operator delete[](void *p,size_t)noexcept{std::free(p);}
void *operator new(size_t n,std::align_val_t a){allocation_probe::hit();void *p=nullptr;if(const auto error=posix_memalign(&p,std::max(size_t(a),sizeof(void*)),n?n:1)){std::fprintf(stderr,"ALIGNED_NEW_FAILURE bytes=%zu alignment=%zu error=%d\n",n,size_t(a),error);throw std::bad_alloc();}return p;}
void *operator new[](size_t n,std::align_val_t a){return ::operator new(n,a);}
void operator delete(void *p,std::align_val_t)noexcept{std::free(p);}
void operator delete[](void *p,std::align_val_t)noexcept{std::free(p);}
void operator delete(void *p,size_t,std::align_val_t)noexcept{std::free(p);}
void operator delete[](void *p,size_t,std::align_val_t)noexcept{std::free(p);}
void *operator new(size_t n,const std::nothrow_t&)noexcept{try{return ::operator new(n);}catch(...){return nullptr;}}
void *operator new[](size_t n,const std::nothrow_t&)noexcept{try{return ::operator new[](n);}catch(...){return nullptr;}}
void *operator new(size_t n,std::align_val_t a,const std::nothrow_t&)noexcept{try{return ::operator new(n,a);}catch(...){return nullptr;}}
void *operator new[](size_t n,std::align_val_t a,const std::nothrow_t&)noexcept{try{return ::operator new[](n,a);}catch(...){return nullptr;}}
void operator delete(void*p,const std::nothrow_t&)noexcept{std::free(p);}
void operator delete[](void*p,const std::nothrow_t&)noexcept{std::free(p);}
void operator delete(void*p,std::align_val_t,const std::nothrow_t&)noexcept{std::free(p);}
void operator delete[](void*p,std::align_val_t,const std::nothrow_t&)noexcept{std::free(p);}

using namespace dome;
namespace {
void sweep(const Scene &source,const ShapeKeyEdit &edit,const char *label,bool measure){
 const auto exact=source.encode();size_t baseline=0;Scene expected;
 for(size_t pass=0;pass<2;++pass){auto candidate=source;size_t count=0;{allocation_probe::Guard guard(SIZE_MAX);author_shapekey(candidate,candidate.active().id,edit);count=allocation_probe::calls;}baseline=std::max(baseline,count);if(!pass)expected=std::move(candidate);else if(candidate.encode()!=expected.encode())throw std::runtime_error("Repeated actual key factory differs before fault injection");}
 const auto positive=expected.encode();std::cout<<"KEY_CPP_BASELINE "<<label<<" "<<baseline<<std::endl;if(measure)return;
 size_t denied=0,success=0;
 for(size_t cut=0;cut<=baseline;++cut){auto candidate=source;bool failed=false;size_t calls=0;{allocation_probe::Guard guard(cut);try{author_shapekey(candidate,candidate.active().id,edit);}catch(const std::bad_alloc&){failed=true;}calls=allocation_probe::calls;}
  if(failed){if(calls<=cut||candidate.encode()!=exact||source.encode()!=exact)throw std::runtime_error(std::string("Actual key persistent allocation denial changed complete caller: ")+label+" prefix "+std::to_string(cut));++denied;}
  else{if(candidate.encode()!=positive||source.encode()!=exact)throw std::runtime_error(std::string("Actual key allocation success differs from exact complete reference: ")+label+" prefix "+std::to_string(cut));++success;}
  if(cut==0||cut==baseline/2||cut==baseline){auto retry=source;author_shapekey(retry,retry.active().id,edit);if(retry.encode()!=positive)throw std::runtime_error("Persistent allocation context retry differs");}
  if(cut%250==0)std::cout<<"KEY_CPP_PREFIX "<<label<<" "<<cut<<std::endl;
 }
 if(!denied||!success)throw std::runtime_error("Persistent factory allocation cohort lacks denial or success endpoint");std::cout<<"PASS KEY_CPP_PREFIXES "<<label<<" denied="<<denied<<" successful="<<success<<" baseline="<<baseline<<std::endl;
}
}
int main(int argc,char **argv)try{
 if(argc<2||argc>3)return 2;const bool measure=argc==3&&std::string_view(argv[2])=="--measure";auto source=Scene::load(argv[1]);const auto exact=source.encode();ShapeKeyEdit create;create.name="Persistent current37 new key";sweep(source,create,"Create",measure);author_shapekey(source,source.active().id,create);ShapeKeyEdit replace;replace.action=ShapeKeyAction::Replace;replace.target=source.active().deformation->morphs->targets.size()-1;replace.positions=source.active().painted->mesh().positions;replace.positions.at(0).z+=.007f;replace.positions.at(3).z-=.006f;sweep(source,replace,"Replace",measure);std::cout<<"PASS actual post37 persistent key factory controls\n";return 0;
}catch(const std::exception &e){allocation_probe::active=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
