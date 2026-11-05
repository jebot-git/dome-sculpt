// SPDX-License-Identifier: GPL-3.0-or-later
#include "vrm1-expression-layer.hpp"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <new>
#include <iostream>
namespace {thread_local bool injecting=false;thread_local size_t calls=0,limit=SIZE_MAX;}
[[gnu::noinline]]void *operator new(size_t n){if(injecting&&calls++>=limit)throw std::bad_alloc();if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]]void *operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
using namespace dome;
int main(int argc,char**argv)try{
 if(argc<2||argc>3||(argc==3&&std::string_view(argv[2])!="--controls-only"))return 2;
 const bool exhaustive=argc==2;const auto scene=Scene::load(argv[1]);const auto exact=scene.encode();const auto &object=scene.active();const auto &raw=*object.appearance->original_bytes;const size_t count=uint32_t(raw[12])|uint32_t(raw[13])<<8|uint32_t(raw[14])<<16|uint32_t(raw[15])<<24;const auto original=nlohmann::json::parse(raw.begin()+20,raw.begin()+20+count);const auto packet=detail::PaintOverlayFactory::capture(*object.painted);SourceUvBakeOptions policy;policy.resolution=256;size_t measured=0,polls=0,passed=0;detail::Vrm1ExpressionLayer expected;
 auto check=[&](bool value,const char*what){if(!value)throw std::runtime_error(what);++passed;};
 for(int repeat=0;repeat<3;++repeat){calls=0;limit=SIZE_MAX;size_t observed=0;injecting=true;auto next=detail::compile_vrm1_expression_layer(original,object,*packet,256,policy,[&]{++observed;return false;});injecting=false;check(calls&&observed&&(!repeat||(calls==measured&&observed==polls&&next.image->rgba==expected.image->rgba&&next.paint_coordinates==expected.paint_coordinates)),"Unstable actual outline factory baseline");measured=calls;polls=observed;expected=std::move(next);}
 std::cout<<"OUTLINE_BOUNDARIES allocations="<<measured<<" polls="<<polls<<std::endl;
 for(size_t prefix=0;exhaustive&&prefix<measured;++prefix){calls=0;limit=prefix;bool refused=false;injecting=true;try{(void)detail::compile_vrm1_expression_layer(original,object,*packet,256,policy,{});}catch(const std::bad_alloc&){refused=true;}injecting=false;check(refused&&calls>prefix&&scene.encode()==exact,"Persistent outline allocation denial changed complete source or failed to propagate");}
 for(size_t at=1;exhaustive&&at<=polls;++at){size_t observed=0;bool refused=false;try{(void)detail::compile_vrm1_expression_layer(original,object,*packet,256,policy,[&]{return ++observed>=at;});}catch(const std::runtime_error&e){refused=std::string_view(e.what()).find("cancelled")!=std::string_view::npos;}check(refused&&observed>=at&&scene.encode()==exact,"Outline cancellation boundary failed exact rollback");}
 auto refuses=[&](auto change){auto edited=object;auto binding=std::make_shared<SourceMaterialBindings>(*object.material_bindings);edited.material_bindings=binding;change(*binding);bool refused=false;try{(void)detail::compile_vrm1_expression_layer(original,edited,*packet,256,policy,{});}catch(const std::invalid_argument&e){refused=std::string_view(e.what()).find("unlit world-space")!=std::string_view::npos;}check(refused&&scene.encode()==exact,"Unsupported outline mode failed precise guard/rollback");};
 refuses([](auto&b){b.material.parameters.outline_mix=.5f;});refuses([](auto&b){b.material.parameters.outline=MToonOutline::Screen;});refuses([](auto&b){b.material.parameters.alpha=MToonAlpha::Mask;b.material.parameters.render_queue=2450;});
 auto small=policy;small.max_image_pixels=1;size_t early=0;bool rejected=false;try{(void)detail::compile_vrm1_expression_layer(original,object,*packet,256,small,[&]{++early;return false;});}catch(const std::length_error&e){rejected=std::string_view(e.what()).find("pixel limit")!=std::string_view::npos;}check(rejected&&early<=1&&scene.encode()==exact,"Outline size preflight refused late");
 const auto retry=detail::compile_vrm1_expression_layer(original,object,*packet,256,policy,{});check(retry.image->rgba==expected.image->rgba&&retry.paint_coordinates==expected.paint_coordinates&&scene.encode()==exact,"Outline retry changed image/coordinates/source");std::cout<<"PASS "<<passed<<" actual outline "<<(exhaustive?"exhaustive factory":"controls-only")<<" checks\n";return 0;
}catch(const std::exception&e){injecting=false;std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
