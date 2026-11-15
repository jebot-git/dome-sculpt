// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include "dome/source-paint.hpp"
#include "dome/source-maps.hpp"
#include "dome/bake-authoring.hpp"
#include "dome/bake.hpp"
#include "dome/material-bindings.hpp"
#include "dome/document.hpp"
#include "emission-bake-source.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using namespace dome;
namespace {
size_t checks=0;
void check(bool value,const char*message){if(!value)throw std::runtime_error(message);++checks;}
template<class F>bool refuses(F f){try{f();return false;}catch(const std::exception&){return true;}}
float error(std::array<float,3>a,std::array<float,3>b){float result=0;for(size_t k=0;k<3;++k)result=std::max(result,std::abs(a[k]-b[k]));return result;}
}
int main(int argc,char**argv)try{
 check(argc==2,"Native persistence output path required");
 Mesh raw;raw.positions={{-.6f,-.6f,0},{.6f,-.6f,0},{.6f,.6f,0},{-.6f,.6f,0}};
 raw.normals.assign(4,{0,0,1});raw.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};raw.indices={0,1,2,0,2,3};
 auto maps=std::make_shared<PaintSourceMaps>();
 maps->emission.image=std::make_shared<const Image>(Image{2,2,{255,0,0,255,0,0,255,255,255,0,0,255,0,0,255,255}});
 maps->emission.texcoord=0;maps->emission.min_filter=maps->emission.mag_filter=9729;
 auto original=capture_source_sampling(raw,maps,64);auto atlas=raw;
 for(auto&q:atlas.uv){q[0]=.125f+.75f*q[0];q[1]=.125f+.75f*q[1];}
 auto authored=std::make_shared<std::vector<std::array<float,2>>>(raw.uv);
 for(auto&q:*authored)q[0]=1-q[0];
 const std::array<uint32_t,4>ids{0,1,2,3};
 auto paint=prepare_source_paint(original,atlas,ids,authored,{});
 Scene scene;auto&o=scene.active();o.mesh_active=true;o.painted=paint;o.material.emissive={2,1,4};scene.validate();
 const auto before=scene.encode();
 check(Scene::decode(before).encode()==before,"Native independent source owner roundtrip differs");
 check(paint->source_sampling_prepared()&&paint->source_sampling_authored_uv1()&&paint->source_maps()->emission.texcoord==1&&*paint->source_maps()->uv1!=*paint->source_sampling_authored_uv1(),"Independent PBR carrier and logical authored UV1 owner missing");
 const auto resolve=detail::emission_bake_source(o);
 const auto actual=resolve(0,{.2f,.3f,.5f});
 // Original UV0 is (.8,.5): bilinear red/blue weights are .1/.9.
 const float callback_error=error(actual,{.2f,0,3.6f});
 std::cout<<"EMISSION_DOMAIN callback actual="<<actual[0]<<','<<actual[1]<<','<<actual[2]<<" expected=.2,0,3.6 error="<<callback_error<<std::endl;
 auto typed=scene;auto&typed_source=typed.active();auto binding=std::make_shared<SourceMaterialBindings>();
 binding->has_mtoon=true;binding->native_reference.emission=typed_source.material.emissive;
 binding->material=MToonMaterial::from_vrm1({{"extensions",{{"VRMC_materials_mtoon",{{"specVersion","1.0"}}}}}});
 binding->material.parameters.emission={.5f,.25f,1};binding->material.parameters.emission_strength=4;
 auto&slot=binding->material.textures[size_t(MToonSlot::Emission)];slot.index=0;slot.texcoord=1;slot.encoding=MToonEncoding::Srgb;
 binding->roles[size_t(MToonSlot::Emission)]=maps->emission;binding->validate();typed_source.material_bindings=binding;typed.validate();
 const auto typed_data=typed.encode();check(Scene::decode(typed_data).encode()==typed_data,"Typed original-role Scene roundtrip differs");
 const auto typed_actual=detail::emission_bake_source(typed_source)(0,{.2f,.3f,.5f});
 // Logical artist UV1 mirrors U: (.2,.5), independent of PBR carrier1.
 const float typed_error=error(typed_actual,{1.8f,0,.4f});
 std::cout<<"EMISSION_DOMAIN typed logicalUV1 maximum_error="<<typed_error<<std::endl;
 for(size_t kind=0;kind<3;++kind){SourcePaintOptions limited;if(kind==0)limited.max_coordinate_bytes=1;else if(kind==1)limited.max_working_bytes=1;else limited.max_work=1;
  check(refuses([&]{prepare_source_paint(original,atlas,ids,authored,limited);})&&scene.encode()==before,"Prospective independent-source coordinate/working/work quota changed source");}
 size_t capture_polls=0;bool cancel=false;auto cancellable=detail::emission_bake_source(o,[&]{++capture_polls;return cancel;});cancel=true;
 check(refuses([&]{cancellable(0,{.2f,.3f,.5f});})&&capture_polls>=3&&scene.encode()==before,"Cancelled immutable emission capture changed source");
 BakeSource high;high.mesh=paint->mesh();high.materials[0].maps=detail::raw_bake_source(o);high.materials[0].material=o.material;high.materials[0].resolved_emission=resolve;
 for(size_t kind=0;kind<3;++kind){BakeOptions limited;limited.resolution=64;limited.padding=0;limited.ao_samples=0;if(kind==0)limited.max_texels=4095;else if(kind==1)limited.max_rays=1;else limited.max_raster_samples=1;
  check(refuses([&]{bake_mesh(raw,high,limited);})&&scene.encode()==before,"Prospective actual Bake texel/ray/raster quota changed source");}
 const auto source=o.id;const auto target=scene.add("Independent target",Volume{});auto&t=scene.object(target);t.mesh_active=true;t.painted=std::make_shared<PaintedMesh>(raw,64);scene.select(target);
 ObjectBakeSettings options;options.front_mm=options.back_mm=2;options.padding=0;options.ao_samples=0;
 const auto baseline=scene.snapshot();const auto baseline_data=scene.encode();size_t bake_polls=0;
 auto report=bake_object(scene,source,options,[&]{++bake_polls;return false;});
 const auto&field=*scene.active().painted->baked()->emission_field;float worker_error=0;size_t checked=0;
 for(size_t y=16;y<48;++y)for(size_t x=16;x<48;++x){const float u=(x+.5f)/64,blue=u*2-.5f;
  const auto got=field.values()[y*64+x].resolve({},1);worker_error=std::max(worker_error,error(got,{2*(1-blue),0,4*blue}));++checked;}
 std::cout<<"EMISSION_DOMAIN actualBake projected="<<report.projected<<" independently_checked="<<checked<<" maximum_error="<<worker_error<<std::endl;
 for(size_t boundary:std::array<size_t,4>{1,2,bake_polls/2,bake_polls}){auto trial=baseline.snapshot();size_t polls=0;
  check(refuses([&]{bake_object(trial,source,options,[&]{return ++polls>=boundary;});})&&trial.encode()==baseline_data,"Early/middle/final actual Bake cancellation published partial source/target state");}
 std::cout<<"EMISSION_DOMAIN prospective_quotas=6 cancellation_boundaries=4 observed_Bake_polls="<<bake_polls<<std::endl;
 Document document;document.replace(baseline);check(document.wait_mesh(),"Document source publication failed");document.bake_from_object(source,options);
 auto save=document.save_async(argv[1]);save.get();
 check(document.wait_asset(std::chrono::seconds(120))&&document.wait_mesh()&&document.frame().state->error.empty()&&document.copy_scene().encode()==scene.encode()&&Scene::load(argv[1]).encode()==scene.encode(),"Actual Document orderedSave/reopen differs");
 check(document.undo()&&document.wait_mesh()&&document.copy_scene().encode()==baseline_data&&document.redo()&&document.wait_mesh()&&document.copy_scene().encode()==scene.encode(),"Actual Document source-aware Bake history differs");
 std::cout<<"EMISSION_DOMAIN Document orderedSave/reopen/UndoRedo exact"<<std::endl;
 check(callback_error<=2e-6f&&worker_error<=2e-5f&&typed_error<=2e-6f,"Resolved PBR carrier reads separate logical authoredUV1");
 std::cout<<"PASS "<<checks<<" source-role PBR emission owner domain controls"<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
