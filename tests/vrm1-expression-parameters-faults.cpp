// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-paint-parameters.hpp"
#include "dome/source-maps.hpp"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <iostream>
#include <new>
namespace {
thread_local bool armed=false,hit=false;
thread_local size_t calls=0,cut=0;
void require(bool ok,const char *why){if(!ok)throw std::runtime_error(why);}
}
[[gnu::noinline]] void *operator new(size_t n){if(armed&&++calls>=cut&&cut){hit=true;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]] void *operator new[](size_t n){return ::operator new(n);}
[[gnu::noinline]] void operator delete(void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete(void *p,size_t)noexcept{std::free(p);}
[[gnu::noinline]] void operator delete[](void *p,size_t)noexcept{std::free(p);}
int main()try{
 using namespace dome;Mesh raw;raw.positions={{0,0,0},{1,0,0},{1,1,0},{0,1,0}};raw.normals.assign(4,{0,0,1});raw.uv={{-1,-1},{2,-1},{2,2},{-1,2}};raw.indices={0,1,2,0,2,3};
 auto maps=std::make_shared<PaintSourceMaps>();maps->color.image=std::make_shared<const Image>(Image{1,1,{180,70,210,255}});auto original=capture_source_sampling(raw,maps,64);
 Mesh atlas=raw;atlas.uv={{.1f,.1f},{.9f,.1f},{.9f,.9f},{.1f,.9f}};const std::array<uint32_t,4> vertices{0,1,2,3};auto paint=prepare_source_paint(original,atlas,vertices);
 Brush brush;brush.from=brush.to={.5f,.5f,0};brush.normal={0,0,1};brush.radius=.7f;PaintSettings ink;ink.color={.2f,.8f,.4f,.6f};ink.front_only=false;paint->begin_stroke();paint->stamp(brush,ink);require(paint->end_stroke(),"Actual source parameter fixture paints colour");
 paint->add_layer("Independent physical contribution");ink.channel=PaintChannel::Emission;ink.emission_units=PaintEmissionUnits::Physical;ink.emission={.005f,10000,0};paint->begin_stroke();paint->stamp(brush,ink);require(paint->end_stroke()&&paint->has_physical_emission(),"Actual source parameter fixture has independent physical sparse data");
 const auto before=paint->encode(),raw_before=original->encode();std::array<std::array<float,6>,5> transforms;transforms.fill({1,0,0,1,0,0});transforms[0]={.6f,-.2f,.1f,1.3f,.2f,-.3f};
 auto operation=[&](const SourcePaintOptions &options=SourcePaintOptions{}){return detail::rebind_source_paint_transforms(*paint,transforms,options);};
 calls=cut=0;armed=true;auto control=operation();armed=false;const auto count=calls;require(count>0&&count<2048,"Actual source parameter allocations measured and bounded");const auto expected=control->encode();require(expected!=before,"Real parameter factory changes source matrices");
 require(PaintedMesh::decode(expected,nullptr,false,nullptr,nullptr,SourcePaintDecodeContext{true,true})->encode()==expected,"Actual Source4+physical result independently replays and reopens");
 for(size_t layer=0;layer<paint->layers().size();++layer)require(control->layers()[layer].tiles==paint->layers()[layer].tiles&&control->layers()[layer].emission_tiles==paint->layers()[layer].emission_tiles,"Legacy/physical paint owners remain shared exact");
 for(size_t boundary=1;boundary<=count;++boundary){calls=0;cut=boundary;hit=false;bool failed=false;armed=true;try{(void)operation();}catch(const std::bad_alloc&){failed=true;}armed=false;require(hit&&failed,"Every actual allocation boundary is persistently refused without partial success");require(paint->encode()==before&&original->encode()==raw_before,"Every allocation denial preserves current/source data and paint owners");require(operation()->encode()==expected,"Actual retry after each denial is exact");}
 size_t polls=0;SourcePaintOptions options;options.cancelled=[&]{++polls;return false;};require(operation(options)->encode()==expected,"Counted cancellation factory output stays exact");const auto total=polls;require(total>1&&total<2048,"Real parameter cancellation polls measured");
 for(size_t boundary=1;boundary<=total;++boundary){polls=0;options.cancelled=[&]{return ++polls>=boundary;};bool stopped=false;try{(void)operation(options);}catch(const std::runtime_error&){stopped=true;}require(stopped&&polls==boundary&&paint->encode()==before&&original->encode()==raw_before,"Every actual cancellation leaves exact input and source owners");}
 std::cout<<"PASS "<<count<<" persistent allocation boundaries and "<<total<<" real cancellation polls; physical/source restart exact\n";return 0;
}catch(const std::exception &e){armed=false;std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
