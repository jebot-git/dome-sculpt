// SPDX-License-Identifier: GPL-3.0-or-later
#include "vrm1-expression-layer.hpp"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <new>
#include <iostream>
namespace {thread_local bool injecting=false,denied=false;thread_local size_t allocations=0,boundary=SIZE_MAX;}
[[gnu::noinline]]void *operator new(size_t n){if(injecting&&++allocations>=boundary){denied=true;throw std::bad_alloc();}if(auto p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
[[gnu::noinline]]void *operator new[](size_t n){return ::operator new(n);}void operator delete(void*p)noexcept{std::free(p);}void operator delete[](void*p)noexcept{std::free(p);}void operator delete(void*p,size_t)noexcept{std::free(p);}void operator delete[](void*p,size_t)noexcept{std::free(p);}
using namespace dome;
int main(int argc,char**argv)try{
 if((argc!=2&&argc!=3)||(argc==3&&std::string_view(argv[2])!="--preflight-only"))return 2;const bool all_prefixes=argc==2;const auto scene=Scene::load(argv[1]);const auto exact=scene.encode();const auto &object=scene.active();const auto &raw=*object.appearance->original_bytes;const size_t count=uint32_t(raw[12])|uint32_t(raw[13])<<8|uint32_t(raw[14])<<16|uint32_t(raw[15])<<24;const auto original=nlohmann::json::parse(raw.begin()+20,raw.begin()+20+count);const auto packet=detail::PaintOverlayFactory::capture(*object.painted);SourceUvBakeOptions limits;limits.resolution=256;
 size_t count_new=0,polls=0;detail::Vrm1ExpressionLayer expected;
 for(int repeat=0;repeat<2;++repeat){size_t observed=0;allocations=0;boundary=SIZE_MAX;injecting=true;auto next=detail::compile_vrm1_expression_layer(original,object,*packet,256,limits,[&]{++observed;return false;});injecting=false;if(next.forward_uv||next.scale!=std::array<float,2>{1,1}||next.offset!=std::array<float,2>{}||!allocations||!observed||(repeat&&(allocations!=count_new||observed!=polls||next.image->rgba!=expected.image->rgba||next.paint_coordinates!=expected.paint_coordinates||next.forward_uv!=expected.forward_uv||next.shade_alias!=expected.shade_alias||next.shade_divisor!=expected.shade_divisor)))throw std::runtime_error("Actual repeated expression-layer allocation/poll/output profile is unstable");count_new=allocations;polls=observed;expected=std::move(next);}
 std::cout<<"LAYER_BOUNDARIES allocations="<<count_new<<" cancel="<<polls<<" continuousRGB="<<expected.maximum_color_error<<" coverage="<<expected.maximum_coverage_error<<" working="<<expected.working_bytes<<" operations="<<expected.sample_operations<<std::endl;
 size_t passed=0;for(size_t at=1;all_prefixes&&at<=count_new;++at){allocations=0;boundary=at;denied=false;injecting=true;bool caught=false;try{(void)detail::compile_vrm1_expression_layer(original,object,*packet,256,limits,{});}catch(const std::bad_alloc&){caught=true;}injecting=false;if(!caught||!denied||scene.encode()!=exact)throw std::runtime_error("Persistent expression-layer allocation denial changed native owner or did not propagate");++passed;}
 for(size_t at=1;all_prefixes&&at<=polls;++at){size_t calls=0;bool caught=false;try{(void)detail::compile_vrm1_expression_layer(original,object,*packet,256,limits,[&]{return ++calls>=at;});}catch(const std::runtime_error&e){caught=std::string_view(e.what()).find("cancelled")!=std::string_view::npos;}if(!caught||calls<at||scene.encode()!=exact)throw std::runtime_error("Actual expression-layer cancellation boundary is unreachable or changed native owner");++passed;}
 // Genuine uniform-role source takes both complete RGB image scans. The
 // immutable images are real source resources, not synthetic proof receipts.
 size_t scan_pixels=0;for(size_t slot:{size_t(1),size_t(3)}){const auto &image=object.material_bindings->roles[slot].image;if(!image)throw std::runtime_error("Actual constant-role proof fixture has no source image");scan_pixels+=size_t(image->width)*image->height;}
 if(expected.sample_operations<scan_pixels*3||scene.encode()!=exact)throw std::runtime_error("Actual source-role image proof was not charged prospectively");++passed;
 auto rejects=[&](std::string_view control,const nlohmann::json &metadata,const SourceUvBakeOptions &policy,size_t maximum_polls,std::string_view reason,bool precise=false,uint32_t output_size=256){
  bool caught=false;size_t observed=0;std::string diagnostic;
  try{(void)detail::compile_vrm1_expression_layer(metadata,object,*packet,output_size,policy,[&]{++observed;return false;});}
  catch(const std::exception&e){diagnostic=e.what();caught=precise?diagnostic==reason:diagnostic.find(reason)!=std::string::npos;}
  const bool unchanged=scene.encode()==exact;
  std::cout<<"PREFLIGHT_CONTROL "<<control<<" observed="<<diagnostic<<" expected="<<reason<<" polls="<<observed<<" maximum="<<maximum_polls<<" source_unchanged="<<unchanged<<std::endl;
  if(!caught||observed>maximum_polls||!unchanged)throw std::runtime_error("Actual target preflight refused late, changed source authority or did not refuse precisely: "+std::string(control));
  ++passed;
 };
 size_t definitions=0;for(const char *table:{"preset","custom"})if(original["extensions"]["VRMC_vrm"]["expressions"].contains(table))definitions+=original["extensions"]["VRMC_vrm"]["expressions"][table].size();
 auto bounded=limits;bounded.max_sample_operations=scan_pixels*3-1;rejects("invariant-role-work",original,bounded,1+2*definitions+object.material_bindings->roles[1].image->height,"invariant-role prospective");
 bounded=limits;bounded.max_image_pixels=1;rejects("native-output-pixel-cap",original,bounded,1,"VRM1 expression-layer pixel limit exceeded",true);
 // ff734178 moved the unchanged native/output pixel cap ahead of every
 // callback-bearing source-image, normal and additive scan. Preserve that
 // prospective contract and its exact diagnostic; do not restore late work.
 constexpr uint32_t output_size=256;const size_t output_pixels=size_t(output_size)*output_size;
 const size_t native_pixels=size_t(packet->resolution())*packet->resolution();
 if(native_pixels>=output_pixels)throw std::runtime_error("Actual output-only pixel control needs its genuine smaller native grid");
 bounded=limits;bounded.max_image_pixels=output_pixels-1;
 rejects("output-only-pixel-cap",original,bounded,1,"VRM1 expression-layer pixel limit exceeded",true);
 // Retain a genuinely independent source-image quota check too. Here
 // requested64 and original native64 both fit, while the REAL128 shade
 // image exceeds the policy. No source owner/image/packet is substituted.
 const auto &shade_image=*object.material_bindings->roles[size_t(MToonSlot::Shade)].image;
 const size_t shade_pixels=size_t(shade_image.width)*shade_image.height;
 if(native_pixels>=shade_pixels||size_t(64)*64>=shade_pixels)throw std::runtime_error("Actual source-only image control needs its genuine larger original image");
 bounded=limits;bounded.max_image_pixels=shade_pixels-1;
 rejects("source-role-only-pixel-cap",original,bounded,1+definitions,"VRM1 invariant-role prospective pixel/work limit exceeded",true,64);
 // Native-only cannot be isolated here: this genuine fixture is native64,
 // and64 is the compiler's minimum output resolution. Never resize/relabel
 // the source merely to manufacture a distinct native-bound proof.
 std::cout<<"NATIVE_ONLY_PIXEL_CONTROL unavailable native="<<packet->resolution()<<" minimum_output=64"<<std::endl;
 bounded=limits;bounded.max_image_pixels=output_pixels;
 const auto endpoint=detail::compile_vrm1_expression_layer(original,object,*packet,output_size,bounded,{});
 if(endpoint.image->rgba!=expected.image->rgba||endpoint.paint_coordinates!=expected.paint_coordinates||endpoint.sample_operations!=expected.sample_operations||scene.encode()!=exact)throw std::runtime_error("Actual exact output pixel endpoint changed its source authority or result");++passed;
 bounded=limits;bounded.max_image_pixels=1;bounded.max_sample_operations=0;
 rejects("pixel-before-role-work",original,bounded,1,"VRM1 expression-layer pixel limit exceeded",true);
 size_t immediate_polls=0;bool immediate_cancel=false;
 try{(void)detail::compile_vrm1_expression_layer(original,object,*packet,output_size,bounded,[&]{++immediate_polls;return true;});}
 catch(const std::runtime_error&e){immediate_cancel=std::string_view(e.what())=="VRM1 expression-layer compilation cancelled";}
 if(!immediate_cancel||immediate_polls!=1||scene.encode()!=exact)throw std::runtime_error("Actual initial cancellation must retain its one-poll preflight and unchanged owner");++passed;

 auto changed=original;changed["extensions"]["VRMC_vrm"]["expressions"]["custom"]["cross"]["textureTransformBinds"][0]["scale"][0]=1e21;rejects("metadata-target-bound",changed,limits,1,"source metadata bounds");
 changed=original;for(const char *table:{"preset","custom"})if(changed["extensions"]["VRMC_vrm"]["expressions"].contains(table))for(auto &entry:changed["extensions"]["VRMC_vrm"]["expressions"][table]){entry.erase("materialColorBinds");entry.erase("textureTransformBinds");}rejects("empty-binding-authority",changed,limits,1,"no relevant original binding");
 // A genuine unused material declaration is valid metadata; its Rim bind
 // must not activate work or confer authority on the current material. This
 // is a private metadata-selector control, never an import/publication seal.
 const auto &rim=object.material_bindings->material.parameters.rim_color;
 if(object.material_bindings->roles[size_t(MToonSlot::Rim)].image||rim.x!=0||rim.y!=0||rim.z!=0)throw std::runtime_error("Inactive-Rim relevance fixture unexpectedly has an active Rim owner");
 auto unrelated=original;const auto unrelated_material=unrelated.at("materials").size();auto declaration=unrelated.at("materials").at(object.appearance->source_material);declaration["name"]="Actual unused material for relevance counter";unrelated["materials"].push_back(std::move(declaration));
 auto &first_expression=unrelated["extensions"]["VRMC_vrm"]["expressions"]["preset"].begin().value();first_expression["materialColorBinds"].push_back({{"material",unrelated_material},{"type","rimColor"},{"targetValue",std::array<float,4>{.2f,.3f,.4f,17}}});
 size_t unrelated_polls=0;const auto foreign=detail::compile_vrm1_expression_layer(unrelated,object,*packet,256,limits,[&]{++unrelated_polls;return false;});
 if(unrelated_polls!=polls||foreign.image->rgba!=expected.image->rgba||foreign.paint_coordinates!=expected.paint_coordinates||foreign.forward_uv!=expected.forward_uv||foreign.scale!=expected.scale||foreign.offset!=expected.offset||foreign.sample_operations!=expected.sample_operations||foreign.working_bytes!=expected.working_bytes||scene.encode()!=exact)throw std::runtime_error("Unrelated material Rim metadata activated current work or changed actual source/paint result");++passed;
 for(const char *table:{"preset","custom"})if(unrelated["extensions"]["VRMC_vrm"]["expressions"].contains(table))for(auto &entry:unrelated["extensions"]["VRMC_vrm"]["expressions"][table])for(const char *kind:{"materialColorBinds","textureTransformBinds"})if(entry.contains(kind)){auto retained=nlohmann::json::array();for(const auto &bind:entry[kind])if(bind.at("material").get<int>()!=object.appearance->source_material)retained.push_back(bind);entry[kind]=std::move(retained);}
 rejects("foreign-only-binding-authority",unrelated,limits,1,"no relevant original binding");
 auto retry=detail::compile_vrm1_expression_layer(original,object,*packet,256,limits,{});if(retry.image->rgba!=expected.image->rgba||retry.paint_coordinates!=expected.paint_coordinates||retry.forward_uv!=expected.forward_uv||retry.shade_alias!=expected.shade_alias||retry.shade_divisor!=expected.shade_divisor||scene.encode()!=exact)throw std::runtime_error("Expression-layer retry after actual denials changed output or authority");
 std::cout<<"PASS "<<passed+1<<(all_prefixes?" actual invariant-role allocation/cancel/target/retry checks\n":" actual invariant-role preflight/target/retry checks (prefixes not requested)\n");return 0;
}catch(const std::exception&e){injecting=false;std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
