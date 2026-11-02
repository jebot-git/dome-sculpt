// SPDX-License-Identifier: GPL-3.0-or-later
#include "normal-domain-fixtures.hpp"
#include "paint-normal-access.hpp"
#include <iostream>
using namespace dome;
namespace {unsigned checks=0;void check(bool ok,const char *why){if(!ok)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<'\n';}template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}}
int main()try{
 auto scene=normal_domain_fixture::model();const auto original=scene.encode();auto &object=scene.active();const auto authority=object.painted->baked()->normal_authority;const auto field=SourceNormalPreparationAccess::resolved(*object.painted);
 check(authority&&std::any_of(authority->coverage.begin(),authority->coverage.end(),[](auto a){return a>0&&a<255;}),"Fixture retains genuine protected/fractional/full normal-specific metadata");
 auto paint=object.painted->snapshot();auto rgba=*paint->baked();rgba.maps.normal={};rgba.maps.mtoon_normal={};rgba.normal_authority.reset();std::fill(rgba.coverage.begin(),rgba.coverage.end(),255);std::fill(rgba.maps.color.rgba.begin(),rgba.maps.color.rgba.end(),255);paint->install_bake(std::move(rgba),BakedNormalWrite::PreserveRetained);object.painted=paint;scene.validate();
 auto current=SourceNormalPreparationAccess::resolved(*paint);
 check(paint->baked()->normal_authority==authority&&current->pbr->rgba==field->pbr->rgba&&current->mtoon->rgba==field->mtoon->rgba,"RGBA-only replacement preserves exact normal event, pre-filter planes and independent coverage");
 check(std::all_of(paint->baked()->coverage.begin(),paint->baked()->coverage.end(),[](auto a){return a==255;})&&paint->baked()->normal_authority->coverage==authority->coverage,"Expanded common RGBA coverage cannot invent normal detail");
 check(Scene::decode(scene.encode()).encode()==scene.encode(),"RGBA-only retained normal semantics survive complete native restart");
 const auto before=scene.encode();auto incoming=*paint->baked();incoming.normal_authority.reset();
 check(refuses([&]{paint->install_bake(std::move(incoming));})&&scene.encode()==before,"A byte-identical positive normal Bake refuses atomically until a genuine new owning node exists");
 auto cleared=scene.snapshot();cleared.active().painted=cleared.active().painted->snapshot();cleared.active().painted->clear_bake();cleared.validate();auto intrinsic=SourceNormalPreparationAccess::resolved(*cleared.active().painted);
 check(intrinsic->pbr==cleared.active().painted->normal_projection()->pbr&&intrinsic->mtoon==cleared.active().painted->normal_projection()->mtoon,"Explicit Clear switches the retained domain back to its owning intrinsic field");
 check(cleared.active().painted->preview(cleared.active().material)->normal.rgba==intrinsic->pbr->rgba&&Scene::decode(cleared.encode()).encode()==cleared.encode(),"Clear preview and restart preserve intrinsic interpretation without reusing mapped metadata");
 const auto paired=encode_normal_surface(scene,object.id);check(decode_normal_surface(paired).encode()==scene.encode(),"Complete paired archive roundtrip retains edited RGBA with independent captured normal authority");
 int polls=0;check(refuses([&]{encode_normal_surface(scene,object.id,[&]{return ++polls==3;});})&&polls==3&&scene.encode()==before,"Late paired cancellation after complete Scene encoding publishes no archive and preserves exact source authority");
 polls=0;check(refuses([&]{decode_normal_surface(paired,[&]{return ++polls==2;});})&&polls==2&&scene.encode()==before,"Late paired cancellation after complete Scene decoding publishes no surface and permits exact retry");
 auto wrong=paired;auto &details=wrong["scene"]["normal_sampling"]["details"];details.push_back(details[0]);wrong["scene"]["objects"][0]["painted"]["baked"]["normal_authority"]=details.size()-1;
 check(refuses([&]{decode_normal_surface(wrong);}),"Equal-valued but different archived normal event cannot impersonate the captured owner");
 wrong=paired;wrong["scene"]["source_materials"]["normal_preparations"].back()["sampling_domain"]["paint"]=0;
 check(refuses([&]{decode_normal_surface(wrong);}),"A detached paint cannot borrow another retained material's before-state proof");
 SourceUvBakeInput input;input.paint=object.painted;input.current_material=object.material_bindings;input.current_deformation=object.deformation;input.target=object.painted->mesh();input.source_vertices.resize(input.target.positions.size());std::iota(input.source_vertices.begin(),input.source_vertices.end(),0);for(auto &uv:input.target.uv)for(auto &q:uv)q=.5f+(q-.5f)*.9f;SourceUvBakeOptions options;options.normal_sampling=SourceNormalUvMethod::RetainSource;
 auto adopted=adopt_source_uv_bake(bake_source_uv(input,options));auto repeat=scene.snapshot();repeat.active().painted=adopted.paint;repeat.active().material_bindings=adopted.material_bindings;repeat.active().deformation=adopted.deformation;repeat.active().appearance=adopted.appearance;repeat.validate();auto resolved=SourceNormalPreparationAccess::resolved(*repeat.active().painted);
 check(resolved->pbr->rgba==field->pbr->rgba&&resolved->mtoon->rgba==field->mtoon->rgba&&Scene::decode(repeat.encode()).encode()==repeat.encode(),"Repeated UV after independent RGBA coverage retains the original pre-filter normal field exactly");
 check(original!=scene.encode(),"RGBA replacement is a real edit rather than an accidental no-op");std::cout<<"PASS "<<checks<<" retained normal transition/paired checks\n";return 0;
}catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
