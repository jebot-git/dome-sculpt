// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/shapekey-authoring.hpp"
#include "dome/normal-preparation.hpp"
#include "normal-key-derivation.hpp"
#include <cstdlib>
#include <new>
#include <iostream>
namespace failure {thread_local long remaining=-1;thread_local bool count=false;thread_local size_t calls=0;}
void *operator new(std::size_t size){if(failure::count)++failure::calls;if(failure::remaining>=0&&failure::remaining--==0){failure::remaining=0;throw std::bad_alloc();}if(auto p=std::malloc(size?size:1))return p;throw std::bad_alloc();}
void *operator new[](std::size_t size){return ::operator new(size);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,std::size_t)noexcept{std::free(p);}void operator delete[](void*p,std::size_t)noexcept{std::free(p);}
using namespace dome;
namespace {size_t checks=0;void check(bool b,const char*m){if(!b)throw std::runtime_error(m);++checks;std::cout<<"PASS "<<m<<std::endl;}
void sweep(const Scene&source,ShapeKeyEdit edit){
 const auto exact=source.encode();for(int i=0;i<2;++i){auto warmed=source;author_shapekey(warmed,warmed.active().id,edit);}
 auto positive=source;failure::calls=0;failure::count=true;author_shapekey(positive,positive.active().id,edit);failure::count=false;const auto allocations=failure::calls;check(allocations>0&&allocations<100'000,"Actual named factory has a bounded measured standard-C++ allocation set");
 const auto endpoint=positive.encode();size_t denied=0;for(size_t cut=0;cut<allocations;++cut){auto candidate=source;failure::remaining=long(cut);bool rejected=false;try{author_shapekey(candidate,candidate.active().id,edit);}catch(const std::bad_alloc&){rejected=true;}catch(...){failure::remaining=-1;throw;}failure::remaining=-1;if(!rejected)throw std::runtime_error("Named operation completed after persistent prefix "+std::to_string(cut)+" of "+std::to_string(allocations));if(candidate.encode()!=exact)throw std::runtime_error("Named allocation failure modified the complete Scene at prefix "+std::to_string(cut));++denied;}
 std::cout<<"NAMED_ALLOCATION_PREFIXES "<<int(edit.action)<<' '<<denied<<std::endl;check(denied==allocations&&source.encode()==exact,"Every persistent named allocation failure preserves full source, paint/events/rig and history");auto retry=source;author_shapekey(retry,retry.active().id,edit);check(retry.encode()==endpoint,"Identical named operation retries to the exact endpoint after all allocation failures");
}
}
int main(int argc,char**argv)try{if(argc!=2)return 2;auto source=Scene::load(argv[1]);ShapeKeyEdit edit;edit.action=ShapeKeyAction::Replace;edit.target=0;edit.positions=source.active().painted->mesh().positions;edit.positions[0].z+=.013f;edit.positions[3].z-=.017f;author_shapekey(source,source.active().id,edit);
 ShapeKeyEdit copy;copy.action=ShapeKeyAction::Duplicate;copy.target=0;copy.name="Fault duplicate";sweep(source,copy);
 auto deform=std::make_shared<DeformationState>(*source.active().deformation);deform->morph_weights.assign(deform->morphs->targets.size(),.35f);source.active().deformation=deform;source.validate();ShapeKeyEdit mix;mix.action=ShapeKeyAction::CaptureMix;mix.name="Fault mix";sweep(source,mix);
 auto capped=source;for(size_t n=capped.active().deformation->morphs->targets.size();n<128;++n){ShapeKeyEdit next;next.name="Budget "+std::to_string(n);author_shapekey(capped,capped.active().id,next);}const auto exact=capped.encode();bool refused=false;try{author_shapekey(capped,capped.active().id,copy);}catch(const std::length_error&e){refused=std::string_view(e.what()).find("budget")!=std::string_view::npos;}check(refused&&capped.encode()==exact,"Actual 128-target native registry refuses a new named allocation without publication");
 std::cout<<"PASS "<<checks<<" named allocation and quota checks"<<std::endl;return 0;
}catch(const std::exception&e){failure::remaining=-1;failure::count=false;std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
