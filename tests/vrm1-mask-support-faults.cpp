// SPDX-License-Identifier: GPL-3.0-or-later
#include "vrm1-expression-layer.hpp"
#include "vrm1-mask-support.hpp"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <new>
#include <iostream>
#include <numeric>
namespace {thread_local bool injecting=false,denied=false;thread_local size_t allocations=0,boundary=SIZE_MAX;}
[[gnu::noinline]]void *operator new(size_t n){if(injecting&&++allocations>=boundary){denied=true;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]]void *operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
using namespace dome;
int main(int argc,char**argv)try{
 if(argc!=2&&(argc!=3||std::string_view(argv[2])!="--controls-only"))return 2;const bool controls_only=argc==3;const auto scene=Scene::load(argv[1]);const auto exact=scene.encode();const auto &object=scene.active();const auto &raw=*object.appearance->original_bytes;const size_t count=uint32_t(raw[12])|uint32_t(raw[13])<<8|uint32_t(raw[14])<<16|uint32_t(raw[15])<<24;const auto original=nlohmann::json::parse(raw.begin()+20,raw.begin()+20+count);const auto packet=detail::PaintOverlayFactory::capture(*object.painted);SourceUvBakeOptions limits;limits.resolution=256;
 size_t count_new=0,polls=0;detail::Vrm1ExpressionLayer expected;
 for(int repeat=0;repeat<2;++repeat){size_t observed=0;allocations=0;boundary=SIZE_MAX;injecting=true;auto next=detail::compile_vrm1_expression_layer(original,object,*packet,256,limits,[&]{++observed;return false;});injecting=false;if(!next.forward_uv||!next.shade_alias||!allocations||!observed||(repeat&&(allocations!=count_new||observed!=polls||next.image->rgba!=expected.image->rgba||next.paint_coordinates!=expected.paint_coordinates||next.forward_uv!=expected.forward_uv||next.shade_alias!=expected.shade_alias||next.shade_divisor!=expected.shade_divisor)))throw std::runtime_error("Actual repeated expression-layer allocation/poll/output profile is unstable");count_new=allocations;polls=observed;expected=std::move(next);}
 std::cout<<"LAYER_BOUNDARIES allocations="<<count_new<<" cancel="<<polls<<" continuousRGB="<<expected.maximum_color_error<<" coverage="<<expected.maximum_coverage_error<<" working="<<expected.working_bytes<<" operations="<<expected.sample_operations<<std::endl;
 size_t passed=0;if(!controls_only){for(size_t at=1;at<=count_new;++at){allocations=0;boundary=at;denied=false;injecting=true;bool caught=false;try{(void)detail::compile_vrm1_expression_layer(original,object,*packet,256,limits,{});}catch(const std::bad_alloc&){caught=true;}injecting=false;if(!caught||!denied||scene.encode()!=exact)throw std::runtime_error("Persistent expression-layer allocation denial changed native owner or did not propagate");++passed;}
 for(size_t at=1;at<=polls;++at){size_t calls=0;bool caught=false;try{(void)detail::compile_vrm1_expression_layer(original,object,*packet,256,limits,[&]{return ++calls>=at;});}catch(const std::runtime_error&e){caught=std::string_view(e.what()).find("cancelled")!=std::string_view::npos;}if(!caught||calls<at||scene.encode()!=exact)throw std::runtime_error("Actual expression-layer cancellation boundary is unreachable or changed native owner");++passed;}
 std::cout<<"PASS completed actual persistent-prefix="<<count_new<<" and Cancel="<<polls<<std::endl;}
 // Actual MASK proof scans original alpha and the real native premultiplied
 // alpha/coverage grid; output PNG remains an independent colour contribution.
 if(object.material_bindings->material.parameters.alpha!=MToonAlpha::Mask||!expected.shade_alias||scene.encode()!=exact)throw std::runtime_error("Actual masked source authority is absent");++passed;
 auto rejects=[&](const nlohmann::json &metadata,const SourceUvBakeOptions &policy,size_t maximum_polls,std::string_view reason){bool caught=false;size_t observed=0;try{(void)detail::compile_vrm1_expression_layer(metadata,object,*packet,256,policy,[&]{++observed;return false;});}catch(const std::exception&e){caught=std::string_view(e.what()).find(reason)!=std::string_view::npos;}if(!caught||observed>maximum_polls||scene.encode()!=exact)throw std::runtime_error("Actual target preflight refused late, changed source authority or did not refuse precisely");++passed;};

 auto bounded=limits;bounded.max_sample_operations=expected.sample_operations-1;rejects(original,bounded,SIZE_MAX,"prospective");
 bounded=limits;bounded.max_image_pixels=1;rejects(original,bounded,1,"pixel limit");
 auto changed=original;for(auto &bind:changed["extensions"]["VRMC_vrm"]["expressions"]["preset"]["happy"]["materialColorBinds"])if(bind["type"]=="color")bind["targetValue"][3]=.2;rejects(changed,limits,SIZE_MAX,"can change fragment membership");
 changed=original;for(auto &bind:changed["extensions"]["VRMC_vrm"]["expressions"]["preset"]["happy"]["materialColorBinds"])if(bind["type"]=="color")bind["targetValue"][3]=1e21;rejects(changed,limits,1,"source metadata bounds");
 changed=original;for(const char *table:{"preset","custom"})if(changed["extensions"]["VRMC_vrm"]["expressions"].contains(table))for(auto &entry:changed["extensions"]["VRMC_vrm"]["expressions"][table]){entry.erase("materialColorBinds");entry.erase("textureTransformBinds");}rejects(changed,limits,1,"no relevant original binding");

 // This same mesh really contains source cutouts. The admission is local to
 // actual immutable painted support, not a globally opaque source shortcut.
 const auto &image=*object.material_bindings->roles[0].image;size_t holes=0,solid=0;for(size_t i=3;i<image.rgba.size();i+=4){holes+=image.rgba[i]==0;solid+=image.rgba[i]==255;}if(!holes||!solid||object.painted->source_sampling_vertices().empty())throw std::runtime_error("Local support fixture lost real source holes or genuine correspondence");++passed;
 changed=original;for(auto &bind:changed["extensions"]["VRMC_vrm"]["expressions"]["preset"]["happy"]["textureTransformBinds"]){bind["scale"]=std::array<double,2>{.05,.05};bind["offset"]=std::array<double,2>{.15,.8};}rejects(changed,limits,SIZE_MAX,"within actual paint support");
 changed=original;for(auto &bind:changed["extensions"]["VRMC_vrm"]["expressions"]["preset"]["happy"]["textureTransformBinds"]){bind["scale"]=std::array<double,2>{.8,.8};bind["offset"]=std::array<double,2>{-.5,.1};}rejects(changed,limits,SIZE_MAX,"wrapped source footprint adapter");
 // Typed role images are actual current material state: a byte-equal image
 // copy is legal and must compile identically. It does not confer authority
 // over original mesh coordinates or source-vertex associations.
 auto foreign=object;auto binding=std::make_shared<SourceMaterialBindings>(*object.material_bindings);auto equal=std::make_shared<const Image>(*binding->roles[0].image);binding->roles[0].image=equal;binding->roles[1].image=equal;foreign.material_bindings=binding;auto copied=detail::compile_vrm1_expression_layer(original,foreign,*packet,256,limits,{});if(copied.image->rgba!=expected.image->rgba||copied.paint_coordinates!=expected.paint_coordinates||scene.encode()!=exact)throw std::runtime_error("Equal current typed image copy changed actual source interpretation");++passed;
 foreign=object;auto appearance=std::make_shared<CreatorAppearance>(*object.appearance);bool different=false;for(auto vertex:appearance->source_vertices)if(vertex!=appearance->source_vertices.front()){appearance->source_vertices.front()=vertex;different=true;break;}if(!different)throw std::runtime_error("Source-coordinate negative needs genuine distinct vertices");foreign.appearance=appearance;bool owner_refused=false;try{(void)detail::compile_vrm1_expression_layer(original,foreign,*packet,256,limits,{});}catch(const std::exception&e){owner_refused=std::string_view(e.what()).find("Additional source UV association differs")!=std::string_view::npos;}if(!owner_refused||scene.encode()!=exact)throw std::runtime_error("Tampered source-vertex owner gained paint support authority");++passed;
 bounded=limits;bounded.max_working_bytes=expected.working_bytes-1;rejects(original,bounded,SIZE_MAX,"mask support prospective working");
 // The initial complete-output forecast must leave capacity for the actual
 // local support scan too. A budget one unit above that original forecast
 // reaches and refuses in the support scan, before dense PNG allocation.
 // Find the actual complete-output forecast boundary through the public
 // bounded compiler. Additive typed roles have real parameter work, so a
 // hand-coded no-role formula cannot determine that boundary for this input.
 size_t low=0,high=limits.max_sample_operations;
 while(low<high){const auto middle=low+(high-low)/2;auto probe=limits;probe.max_sample_operations=middle;bool before_support=false;
  try{(void)detail::compile_vrm1_expression_layer(original,object,*packet,256,probe,{});}catch(const std::length_error &e){const std::string_view reason=e.what();before_support=reason.find("prospective complete sampling")!=std::string_view::npos||reason.find("prospective parameter work")!=std::string_view::npos;}
  if(before_support)low=middle+1;else high=middle;
 }
 bounded=limits;bounded.max_sample_operations=low;rejects(original,bounded,SIZE_MAX,"mask support prospective work");

 // Independent elementary geometry expectations cover face boundaries and
 // degenerate source-coordinate hulls, rather than mirroring the SAT code.
 using P=detail::mask_support::Point;const std::array<P,3> triangle{P{0,0},P{1,0},P{1,1}};
 if(!detail::mask_support::intersects(triangle,{.6,.1,.7,.2})||detail::mask_support::intersects(triangle,{.1,.6,.2,.7})||!detail::mask_support::intersects(triangle,{.4,.4,.5,.5}))throw std::runtime_error("Paint/source triangle boundary support is wrong");++passed;
 const std::array<P,2> line{P{.2,.2},P{.8,.8}};const std::array<P,1> point{P{.5,.5}};
 if(!detail::mask_support::intersects(line,{.4,.4,.6,.6})||detail::mask_support::intersects(line,{.1,.6,.2,.7})||!detail::mask_support::intersects(point,{.5,.5,.6,.6})||detail::mask_support::intersects(point,{.6,.6,.7,.7}))throw std::runtime_error("Degenerate source-coordinate support is wrong");++passed;
 auto retry=detail::compile_vrm1_expression_layer(original,object,*packet,256,limits,{});if(retry.image->rgba!=expected.image->rgba||retry.paint_coordinates!=expected.paint_coordinates||retry.forward_uv!=expected.forward_uv||retry.shade_alias!=expected.shade_alias||retry.shade_divisor!=expected.shade_divisor||scene.encode()!=exact)throw std::runtime_error("Expression-layer retry after actual denials changed output or authority");
 std::cout<<"PASS "<<passed+1<<" actual local-cutout MASK allocation/cancel/owner/footprint/retry checks\n";return 0;
}catch(const std::exception&e){injecting=false;std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
