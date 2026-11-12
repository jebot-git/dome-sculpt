// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include "dome/source-paint.hpp"
#include "dome/source-maps.hpp"
#include "dome/bake-authoring.hpp"
#include "dome/material-bindings.hpp"
#include "emission-bake-source.hpp"
#include <nlohmann/json.hpp>
#include <iostream>
using namespace dome;
int main()try{
 Mesh raw;raw.positions={{-.6f,-.6f,0},{.6f,-.6f,0},{.6f,.6f,0},{-.6f,.6f,0}};raw.normals.assign(4,{0,0,1});raw.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};raw.indices={0,1,2,0,2,3};
 auto maps=std::make_shared<PaintSourceMaps>();maps->emission.image=std::make_shared<const Image>(Image{2,2,{255,0,0,255,0,0,255,255,255,0,0,255,0,0,255,255}});maps->emission.texcoord=0;maps->emission.min_filter=maps->emission.mag_filter=9729;
 auto binding=std::make_shared<SourceMaterialBindings>();binding->has_mtoon=true;binding->native_reference.emission={2,1,4};binding->material=MToonMaterial::from_vrm1({{"extensions",{{"VRMC_materials_mtoon",{{"specVersion","1.0"}}}}}});binding->material.parameters.emission={.5f,.25f,1};binding->material.parameters.emission_strength=4;auto&slot=binding->material.textures[size_t(MToonSlot::Emission)];slot.index=0;slot.texcoord=1;slot.encoding=MToonEncoding::Srgb;binding->roles[size_t(MToonSlot::Emission)]=maps->emission;binding->validate();
 auto coordinates=std::make_shared<std::vector<std::array<float,2>>>(raw.uv);for(auto&q:*coordinates)q[0]=1-q[0];auto unprepared_maps=std::make_shared<PaintSourceMaps>(*maps);unprepared_maps->uv1=coordinates;Scene control;control.active().mesh_active=true;control.active().painted=capture_source_sampling(raw,unprepared_maps,64);control.active().material.emissive={2,1,4};control.active().material_bindings=binding;control.validate();const auto control_data=control.encode();if(Scene::decode(control_data).encode()!=control_data)throw std::runtime_error("Genuine unprepared UV1 roundtrip differs");const auto valid=detail::emission_bake_source(control.active())(0,{.2f,.3f,.5f});std::cout<<"UNPREPARED_GENUINE_UV1 actual="<<valid[0]<<','<<valid[1]<<','<<valid[2]<<" expected=1.8,0,.4"<<std::endl;if(std::abs(valid[0]-1.8f)>2e-6f||std::abs(valid[2]-.4f)>2e-6f)throw std::runtime_error("Valid unprepared current UV1 fallback lost");
 auto original=capture_source_sampling(raw,maps,64);auto atlas=raw;for(auto&q:atlas.uv){q[0]=.125f+.75f*q[0];q[1]=.125f+.75f*q[1];}const std::array<uint32_t,4>ids{0,1,2,3};Scene scene;auto&o=scene.active();o.mesh_active=true;o.painted=prepare_source_paint(original,atlas,ids);o.material.emissive={2,1,4};o.material_bindings=binding;
 try{scene.validate();std::cout<<"MISSING_LOGICAL_UV1 Scenevalidate=ACCEPTED"<<std::endl;}catch(const std::exception&e){std::cout<<"SAFE_REFUSAL Scenevalidate "<<e.what()<<std::endl;return 0;}
 try{const auto data=scene.encode();if(Scene::decode(data).encode()!=data)throw std::runtime_error("Roundtrip differs");std::cout<<"MISSING_LOGICAL_UV1 native_roundtrip=ACCEPTED"<<std::endl;}catch(const std::exception&e){std::cout<<"SAFE_REFUSAL nativeroundtrip "<<e.what()<<std::endl;return 0;}
 bool callback_refuses=false,bake_refuses=false;
 try{const auto value=detail::emission_bake_source(o)(0,{.2f,.3f,.5f});std::cout<<"MISSING_LOGICAL_UV1 callback=SILENT value="<<value[0]<<','<<value[1]<<','<<value[2]<<std::endl;}catch(const std::exception&e){callback_refuses=true;std::cout<<"SAFE_REFUSAL callback "<<e.what()<<std::endl;}
 const auto source=o.id,target=scene.add("Independent target",Volume{});auto&t=scene.object(target);t.mesh_active=true;t.painted=std::make_shared<PaintedMesh>(raw,64);scene.select(target);const auto baseline=scene.encode();ObjectBakeSettings options;options.front_mm=options.back_mm=2;options.padding=0;options.ao_samples=0;
 try{const auto report=bake_object(scene,source,options);const auto value=scene.active().painted->baked()->emission_field->sample(.8f,.5f).resolve({},1);std::cout<<"MISSING_LOGICAL_UV1 actualBake=SILENT projected="<<report.projected<<" emission="<<value[0]<<','<<value[1]<<','<<value[2]<<std::endl;}catch(const std::exception&e){bake_refuses=true;if(scene.encode()!=baseline)throw std::runtime_error("Refusal changed source/target state");std::cout<<"SAFE_REFUSAL actualBake "<<e.what()<<std::endl;}
 if(!callback_refuses||!bake_refuses)throw std::runtime_error("Prepared source substitutes generated PBR carrier for missing typed logical UV1");std::cout<<"PASS Missing logical UV1 refuses without breaking genuine unprepared UV1"<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
