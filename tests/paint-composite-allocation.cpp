// SPDX-License-Identifier: GPL-3.0-or-later
// Owned host-stage allocation proof only. No foreign GPU/SDK heap denial.
#include "dome/scene.hpp"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <iostream>
namespace {thread_local int64_t deny=-1;thread_local bool observing=false,hit=false;thread_local size_t allocations=0;void allocation(){if(observing)++allocations;if(deny==0){hit=true;throw std::bad_alloc();}if(deny>0)--deny;}}
void*operator new(size_t n){allocation();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void*operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
using namespace dome;
std::string_view host_backend() asm("__wrap__ZN4dome15compute_backendEv");std::string_view host_backend(){return "TEST-HOST-PAINT-ALLOCATION";}
int main()try{
 std::cout<<"HOST-ONLY production aggregate packing/cache allocation proof; native CPU sampler, no GPU/SDK heap or numerical claim\n";set_paint_composite_cache_budget(0);
 Mesh m;m.positions={{0,0,0},{1,0,0},{1,1,0},{0,1,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};auto seed=std::make_shared<PaintedMesh>(m,512);
 PaintSettings settings;settings.color={.31f,.07f,.83f,.61f};Brush b;b.from={.1f,.2f,0};b.to={.9f,.8f,0};b.radius=.2f;b.normal={0,0,1};b.pressure=.8f;b.strength=.71f;seed->begin_stroke();seed->stamp(b,settings);if(!seed->end_stroke())throw std::runtime_error("Real allocation fixture requires nonempty paint");auto raw=seed->encode();
 Material a,other;other.color={.13f,.61f,.07f,.997f};other.metallic=.71f;other.roughness=.17f;
 auto prepare=[&]{auto p=PaintedMesh::decode(raw);auto old=p->composite(a);return std::pair{p,old};};
 auto [baseline,old]=prepare();allocations=0;observing=true;auto expected=baseline->composite(other);observing=false;const auto total=allocations;if(total<10)throw std::runtime_error("Allocation baseline did not enter real packing/image stages");
 size_t failures=0;
 for(size_t boundary=0;boundary<total;++boundary){
  auto [p,held]=prepare();auto bytes=held->color.rgba;const auto owner=p->mesh_owner();hit=false;bool failed=false;deny=int64_t(boundary);
  try{p->composite(other);}catch(const std::bad_alloc&){failed=true;}deny=-1;
  if(!hit||!failed||p->encode()!=raw||p->mesh_owner()!=owner||held->color.rgba!=bytes||p->composite(a)!=held)throw std::runtime_error("Measured allocation failure changed source/cache/held frame or missed injection");
  auto retry=p->composite(other);if(retry->color.rgba!=expected->color.rgba||retry->metal_rough.rgba!=expected->metal_rough.rgba||retry->emission.rgba!=expected->emission.rgba||retry->color_coverage.rgba!=expected->color_coverage.rgba||retry->emission_coverage.rgba!=expected->emission_coverage.rgba)throw std::runtime_error("Same-source allocation retry changed output");++failures;
 }
 std::cout<<"PASS all "<<failures<<" measured persistent host allocation prefixes, exact cache/source/held images and same-source retries\n";return 0;
}catch(const std::exception&e){deny=-1;observing=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
