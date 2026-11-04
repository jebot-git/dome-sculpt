// SPDX-License-Identifier: GPL-3.0-or-later
#include "geometry-reference-replay.hpp"
#include "geometry-field-packing.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#include <numeric>
thread_local bool tracked=false;thread_local size_t calls=0,fail_at=0;
void*allocate(size_t n){if(tracked){++calls;if(fail_at&&calls>=fail_at)throw std::bad_alloc();}auto p=std::malloc(n?n:1);if(!p)throw std::bad_alloc();return p;}
void*operator new(size_t n){return allocate(n);}void*operator new[](size_t n){return allocate(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
using namespace dome;using namespace dome::detail;
int main()try{
 const std::function<bool()>never;auto source=std::make_shared<DeformationState>();Skeleton original;for(int n=0;n<54;++n){Bone b;b.name="joint_"+std::string(48,'x')+std::to_string(n);b.parent=n-1;b.length=.03f;b.rest.translation={0,.01f,0};original.bones.push_back(b);}original.validate();source->skeleton=original;SkinGeometryWork g(100000,never);auto target=skin_reframe(original,{},g);
 size_t canonical_allocations=0;{GeometryReferenceReplay replay(source);weight_surface::Budget w{0,500000000,never};calls=0;tracked=true;bool yes=replay.matches(target,{},true,w,1024*1024);tracked=false;canonical_allocations=calls;if(!yes||!canonical_allocations)throw std::runtime_error("Actual allocating canonical baseline needed");}
 for(size_t prefix=1;prefix<=canonical_allocations;++prefix){GeometryReferenceReplay replay(source);weight_surface::Budget w{0,500000000,never};calls=0;fail_at=prefix;bool refused=false;tracked=true;try{(void)replay.matches(target,{},true,w,1024*1024);}catch(const std::bad_alloc&){refused=true;}catch(...){tracked=false;throw;}tracked=false;fail_at=0;
  if(!refused||calls<prefix||replay.canonical_builds()!=0||replay.retained_workspace()!=0||!geometry_rig_correspondence::same_rig(*source->skeleton,original))throw std::runtime_error("Persistent canonical failure changed source/committed partial cache");
  weight_surface::Budget retry{0,500000000,never};if(!replay.matches(target,{},true,retry,1024*1024)||replay.canonical_builds()!=1)throw std::runtime_error("Canonical same-instance retry failed");}
 std::cout<<"PASS all actual canonical allocation prefixes="<<canonical_allocations<<" same-instance retries exact"<<std::endl;
 std::vector<uint32_t>labels(54);std::iota(labels.begin(),labels.end(),0);std::vector<double>row(54);for(size_t n=0;n<54;++n)row[n]=double(n+1)/1500;const auto saved=row;weight_surface::Budget setup{0,500000000,never};GeometryFieldPacker pack(labels,54,{4,1e-5f},setup);weight_surface::Budget measure{0,500000000,never};calls=0;tracked=true;auto expected=pack.pack(row,measure);tracked=false;const size_t packing_allocations=calls;
 for(size_t prefix=1;prefix<=packing_allocations;++prefix){weight_surface::Budget w{0,500000000,never};calls=0;fail_at=prefix;bool refused=false;tracked=true;try{auto result=pack.pack(row,w);(void)result;}catch(const std::bad_alloc&){refused=true;}catch(...){tracked=false;throw;}tracked=false;fail_at=0;if(!refused||calls<prefix||row!=saved)throw std::runtime_error("Persistent packing failure changed input");weight_surface::Budget retry{0,500000000,never};if(pack.pack(row,retry)!=expected)throw std::runtime_error("Same-instance packet retry differs");}
 std::cout<<"PASS all actual packing allocation prefixes="<<packing_allocations<<" same-instance retries exact"<<std::endl;return 0;
}catch(const std::exception&e){tracked=false;std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
