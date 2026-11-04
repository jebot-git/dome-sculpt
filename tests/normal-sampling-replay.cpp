// SPDX-License-Identifier: GPL-3.0-or-later
#include "bake-recipe-capabilities.hpp"
#include "prepared-normal-shapes-fixtures.hpp"
#include "current-color-target-fixture.hpp"
#include "polygon-paint.hpp"
#include "dome/normal-sampling.hpp"
#include "normal-sampling-access.hpp"
#include "paint-normal-access.hpp"
#include <iostream>
#include <numeric>

using namespace dome;
namespace {
unsigned checks=0;
void check(bool result,const char *message){
  if(!result)throw std::runtime_error(message);++checks;
  std::cout<<"PASS "<<message<<'\n';
}
template<class F>bool refuses(F operation){try{operation();}catch(const std::exception&){return true;}return false;}
}
int main()try{
  const auto scene=prepared_shape_fixture::model(true);
  const auto original=scene.encode();const auto &source=scene.active();
  SourceUvBakeInput input;input.paint=source.painted;input.current_material=source.material_bindings;
  input.current_deformation=source.deformation;input.appearance=source.appearance;
  input.target=source.painted->mesh();
  input.source_vertices.resize(input.target.positions.size());
  std::iota(input.source_vertices.begin(),input.source_vertices.end(),0);
  for(auto &uv:input.target.uv)for(auto &q:uv)q=.5f+(q-.5f)*.95f;
  const auto transferred=bake_source_uv(input);
  const auto normal=detail::transfer_normal_detail(*input.paint,input.target,{});
  check(normal&&transferred.baked&&normal->coverage==transferred.baked->coverage&&
        normal->pbr->rgba==transferred.baked->maps.normal.rgba&&
        normal->mtoon->rgba==transferred.baked->maps.mtoon_normal.rgba,
        "Normal-only replay reproduces historical SourceUV metadata byte for byte");
  const auto resolved=SourceNormalPreparationAccess::resolved(*source.painted);
  const auto preview=source.painted->preview(source.material);
  check(resolved->pbr->rgba==preview->normal.rgba&&resolved->mtoon->rgba==preview->mtoon_normal.rgba,
        "Normal-only resolver reproduces the actual historical pre-filter composite exactly");
  auto leaf=std::make_shared<NormalSamplingDomain>();leaf->resolution=input.paint->resolution();
  leaf->coordinates=input.paint->mesh().uv;leaf->indices=input.target.indices;
  leaf->resolved_pbr=resolved->pbr;leaf->resolved_mtoon=resolved->mtoon;leaf->detail=normal;
  auto projection=std::make_shared<PaintNormalProjection>();projection->version=2;
  projection->pbr=source.painted->normal_projection()->pbr;
  projection->mtoon=source.painted->normal_projection()->mtoon;projection->sampling_domain=leaf;
  auto layout=std::make_shared<SourceNormalUvLayout>();layout->source_vertices=input.source_vertices;
  layout->indices=input.target.indices;layout->uv=input.target.uv;
  layout->pins=input.target.uv_pins;layout->seams=input.target.uv_seams;
  SourceNormalSampling proof;proof.projection=projection;proof.layout=layout;
  proof.before_paint=source.painted->snapshot();proof.before_material=proof.sampling_material=source.material_bindings;
  proof.before_appearance=source.appearance;proof.before_deformation=source.deformation;
  proof.receipt.resolution=source.painted->resolution();proof.receipt.vertices=input.target.positions.size();
  proof.receipt.indices=input.target.indices.size();proof.receipt.correspondence_bytes=layout->storage_bytes()+leaf->storage_bytes();
  proof.validate(true);proof.validate_current(input.target,nullptr);
  check(true,"Owning detached field replay authenticates source planes, ordered correspondence and current normal metadata");
  auto bounded=proof;bounded.receipt.max_working_bytes=4096;
  check(refuses([&]{bounded.validate(true);}),"Forged small normal scratch receipt cannot hide concurrently live replay planes");
  check(NormalSamplingReceipt::decode(proof.receipt.encode()).encode()==proof.receipt.encode(),
        "Exact-domain receipt roundtrip does not relabel raster quality counters");
  auto changed=proof;auto wrong=std::make_shared<NormalSamplingDomain>(*leaf);
  wrong->coordinates[0][0]+=.01f;
  auto wrong_projection=std::make_shared<PaintNormalProjection>(*projection);wrong_projection->sampling_domain=wrong;
  changed.projection=wrong_projection;
  check(refuses([&]{changed.validate(true);}),"Wrong source coordinates cannot be authenticated by nearby geometry");
  wrong=std::make_shared<NormalSamplingDomain>(*leaf);
  auto image=std::make_shared<Image>(*leaf->resolved_pbr);image->rgba[0]^=1;wrong->resolved_pbr=image;
  wrong_projection=std::make_shared<PaintNormalProjection>(*projection);wrong_projection->sampling_domain=wrong;changed.projection=wrong_projection;
  check(refuses([&]{changed.validate(true);}),"A changed resolved source byte refuses even when its normal metadata is unchanged");
  wrong=std::make_shared<NormalSamplingDomain>(*leaf);
  auto wrong_detail=std::make_shared<NormalDetailAuthority>(*normal);wrong_detail->coverage[0]^=1;wrong->detail=wrong_detail;
  wrong_projection=std::make_shared<PaintNormalProjection>(*projection);wrong_projection->sampling_domain=wrong;changed.projection=wrong_projection;
  check(refuses([&]{changed.validate(true);}),"A forged mapped normal coverage record refuses owning replay");
  auto current=*transferred.baked;current.normal_authority=normal;
  check(detail::same_normal_detail(normal.get(),&current,leaf->resolution),
        "Captured event identity and current normal planes pair explicitly");
  current.normal_authority=std::make_shared<const NormalDetailAuthority>(*normal);
  check(!detail::same_normal_detail(normal.get(),&current,leaf->resolution),
        "A byte-identical new event cannot impersonate retained resolved authority");
  current.normal_authority=normal;std::fill(current.coverage.begin(),current.coverage.end(),255);
  check(detail::same_normal_detail(normal.get(),&current,leaf->resolution),
        "Common RGBA coverage does not expand the captured normal-specific coverage");
  size_t polls=0;
  check(refuses([&]{proof.validate(true,[&]{return ++polls>4;});})&&polls>4,
        "Owning replay cancellation exposes no partial field or token");
  SourceUvBakeOptions retained_options;retained_options.normal_sampling=SourceNormalUvMethod::RetainSource;
  const auto retained_result=bake_source_uv(input,retained_options);
  auto adopted=adopt_source_uv_bake(retained_result);
  auto current_scene=scene.snapshot();auto &object=current_scene.active();
  object.painted=adopted.paint;object.material_bindings=adopted.material_bindings;
  object.deformation=adopted.deformation;object.appearance=adopted.appearance;
  current_scene.validate();
  check(object.painted->normal_projection()->version==2&&object.painted->baked()->normal_authority==
        object.painted->normal_projection()->sampling_domain->detail,
        "Detached exact Scale adoption binds the captured normal event to its own owning recipe");
  const auto saved=current_scene.encode();
  check(bake_recipe_fixture::scene30(current_scene,21,saved)&&saved.at("source_materials").at("version")==7,
        "Only actual retained-domain authoring selects Scene21/material7");
  auto restarted=Scene::decode(saved);
  const auto &restarted_paint=*restarted.active().painted;
  check(restarted_paint.baked()->normal_authority==restarted_paint.normal_projection()->sampling_domain->detail&&
        restarted.encode()==saved,"Complete Scene restart preserves event/domain alias and the owning source graph");
  const auto domain_preview=object.painted->preview(object.material);
  check(domain_preview->normal.rgba==resolved->pbr->rgba&&domain_preview->mtoon_normal.rgba==resolved->mtoon->rgba&&domain_preview->normal_projection==object.painted->normal_projection(),
        "Explicit domain preview retains the historical pre-filter resolved planes without another atlas resample");
  check(refuses([&]{bake_source_uv({object.painted,object.appearance,object.material_bindings,std::nullopt,object.deformation,object.painted->mesh(),input.source_vertices});}),
        "Legacy raster method refuses an exact retained field rather than reinterpreting its atlas coordinates");
  auto joint=current_color_target_fixture::fixture();auto &color_object=joint.active();
  auto strict=polygon_paint_geometry(*color_object.painted,color_object.painted->mesh(),{});
  auto descriptor=std::make_shared<PolygonColorSurface>();descriptor->topology=std::make_shared<const PolygonMesh>(color_object.polygon->topology);descriptor->field=color_object.polygon->colors;
  ColorPreparationOptions color_options;color_options.resolve_identity=true;
  auto color=prepare_source_colors({strict,color_object.appearance,color_object.material_bindings,color_object.deformation,color_object.material,descriptor},color_options);
  color_object.painted=color.paint;color_object.material_bindings=color.material_bindings;
  const auto normal_id=joint.add("Retained normal field",Volume{});auto &normal_object=joint.object(normal_id);normal_object=object;normal_object.id=normal_id;
  joint.validate();const auto joint_saved=joint.encode();auto joint_restarted=Scene::decode(joint_saved);
  check(bake_recipe_fixture::scene30(joint,21,joint_saved)&&joint_saved.at("source_materials").at("version")==7&&joint_restarted.encode()==joint_saved,
        "Normal7 owns its real Scene20 color pool without requiring or discarding an unused feature");
  check(joint_restarted.objects()[0].polygon->colors==joint_restarted.objects()[0].material_bindings->color_preparation->polygon_colors->field,
        "Joint restart preserves current-corner field identity independently of normal-event identity");
  const auto paired=encode_normal_surface(current_scene,object.id);
  check(decode_normal_surface(paired).encode()==saved,
        "Owning standalone surface envelope retains complete source and authoring context");
  check(refuses([&]{object.painted->encode();}),"Bare retained Paint archive refuses missing source context");
  auto malformed=bake_recipe_fixture::legacy(current_scene,21);malformed["version"]=20;
  check(refuses([&]{Scene::decode(malformed);}),"Legacy Scene versions cannot relabel retained-domain semantics");
  malformed=saved;malformed["source_materials"]["normal_preparations"].back()["sampling_domain"]["sampling_binding"]=999;
  check(refuses([&]{Scene::decode(malformed);}),"Missing source sampler history refuses before editable publication");
  malformed=saved;malformed["normal_sampling"]["domains"].push_back(malformed["normal_sampling"]["domains"][0]);
  check(refuses([&]{Scene::decode(malformed);}),"Unreachable geometry-only normal leaves cannot smuggle source authority");
  check(refuses([&]{decode_normal_surface(paired,[]{return true;});}),"Paired archive cancellation exposes no partial surface");
  check(scene.encode()==original,"Detached replay and every refusal preserve the complete source model");
  std::cout<<"PASS "<<checks<<" owning normal sampling replay checks\n";
  return 0;
}catch(const std::exception &error){std::cerr<<"FAIL "<<error.what()<<'\n';return 1;}
