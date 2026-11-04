// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_mtoon_fixture
#include "mtoon-render.cpp"
#undef main
#include "dome/document.hpp"
#include "dome/native-authoring.hpp"
#include "dome/scene-file.hpp"
#include "dome/source-paint-bake.hpp"
#include "dome/source-uv-bake.hpp"
#include "source-paint-fixtures.hpp"
#include "source-color-consumer.hpp"
#include "prepared-normal-consumer.hpp"
#include "source-paint-bake-fixtures.hpp"
#include "physical-portable-oracle.hpp"
#include <fstream>
#include <numeric>
namespace {
std::vector<uint8_t> file(const std::filesystem::path &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void physical_ink(Scene &scene){
 auto &object=scene.active();object.painted=object.painted->snapshot();auto &paint=*object.painted;
 paint.add_layer("Physical float, partial coverage");PaintSettings settings;settings.front_only=false;
 settings.channel=PaintChannel::Emission;settings.emission_units=PaintEmissionUnits::Physical;
 settings.emission={12,.005f,3};Brush brush;brush.from=brush.to={-.02f,.015f,0};brush.normal={0,0,1};brush.radius=.19f;brush.strength=.73f;
 paint.begin_stroke();paint.stamp(brush,settings);check(paint.end_stroke(),"Actual physical paint produces sparse float emission");
 paint.layer_properties("Physical float, partial coverage",true,.63f);
 settings.channel=PaintChannel::Mask;settings.mask=.4f;brush.from=brush.to={.07f,.01f,0};brush.radius=.1f;
 paint.begin_stroke();paint.stamp(brush,settings);check(paint.end_stroke(),"Physical emission uses its real editable layer mask");
 paint.add_layer("Hidden HDR must not contribute");settings.channel=PaintChannel::Emission;settings.emission={10000,10000,10000};
 paint.begin_stroke();paint.stamp(brush,settings);paint.end_stroke();paint.layer_properties("Hidden HDR must not contribute",false,1);
}
}
int main(int argc,char **argv)try{
 if(argc<2||argc>3)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 NativeGraphics graphics;const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
 std::cout<<"GL_VENDOR="<<vendor<<" GL_RENDERER="<<device<<std::endl;
 check(argc<3||std::string_view(argv[2])!="--hardware"||(vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos),"Requested physical export consumer requires actual A770");
 Fbo fbo;fbo.bind();SurfaceRenderer native;native.set_frame_time(0);color_consumer::Renderer independent;
 for(int kind=0;kind<7;++kind){
  auto source=portable_material_fixture(kind==1?1:kind==3?6:0);auto &material=source.j["materials"][0];
  material["emissiveFactor"]={.025f,.002f,.02f};material["extensions"]["KHR_materials_emissive_strength"]={{"emissiveStrength",8}};
  source.j["extensionsUsed"].push_back("KHR_materials_emissive_strength");material["emissiveTexture"]={{"index",0},{"texCoord",0}};
  if(kind==2){material["alphaMode"]="MASK";material["alphaCutoff"]=.4f;}
  if(kind==3){material["emissiveTexture"]["texCoord"]=1;material["emissiveTexture"]["extensions"]["KHR_texture_transform"]={{"offset",{.13f,-.27f}},{"scale",{.8f,1.1f}},{"rotation",.17f},{"texCoord",1}};source.j["extensionsUsed"].push_back("KHR_texture_transform");}
  if(kind==5)material["extensions"]["KHR_materials_unlit"]=nlohmann::json::object();
  if(kind==6){material["emissiveFactor"]={0,0,0};material["extensions"].erase("KHR_materials_emissive_strength");}
  const auto input=out/("input"+std::to_string(kind)+".glb");source.save(input);
  Scene initial;initial.active().visible=false;ImportAuthoringOptions options;options.paint_resolution=64;options.prepare_texture_atlas=true;
  std::cout<<"STAGE physical kind="<<kind<<" initial-source-import"<<std::endl;
  auto imported=prepare_authoring_import(initial,input,options);
  if(kind!=6)for(auto id:imported.objects)imported.scene.object(id).material.emissive={8,.016f,.16f};
  ink(imported.scene);physical_ink(imported.scene);
  if(kind==4){auto binding=std::make_shared<SourceMaterialBindings>(*imported.scene.active().material_bindings);binding->physical_emission=std::array<float,3>{0,.15f,2};imported.scene.active().material_bindings=std::move(binding);}
  const auto active=imported.scene.active().id;auto hidden=imported.scene.duplicate(active);imported.scene.object(hidden).visible=false;imported.scene.select(active);
  const auto exact=imported.scene.encode();const auto before_owner=imported.scene.active().painted;
  check(before_owner->source_sampling_prepared()&&before_owner->has_physical_emission(),"Actual imported source owns independent coordinates and physical float paint");
  Document document;document.replace(imported.scene.snapshot());document.shading(false);settle(document);
  const auto original_frame=document.frame().surface;
  const auto reference=draw(native,original_frame);
  const auto package=out/("physical"+std::to_string(kind)+".glb");
  std::cout<<"STAGE physical kind="<<kind<<" editable-export"<<std::endl;
  document.export_asset(package,AssetExportFormat::EditableGlb256).get();
  const auto bytes=file(package);auto recovered=extract_native_authoring(bytes);
  check(recovered&&recovered->encode()==exact&&document.copy_scene().encode()==exact&&imported.scene.active().painted==before_owner,"Physical editable export retains exact float/source/hidden project data without mutating owners");
  color_consumer::Model parsed(package);fbo.bind();
  if(kind==3){prepared_normal_consumer::Renderer roles;roles.render(parsed.data);}
  else independent.render(parsed.data,nullptr,{},true);
  const auto error=fidelity(reference,read());std::cout<<"PHYSICAL_AUTHORING kind="<<kind<<" mean="<<error.mean<<" interior="<<error.interior_maximum<<" max="<<error.maximum<<std::endl;
  if(kind==3){
   check(error.visible>20&&error.interior_maximum>2,"Original two-level pixel-bound failure remains a genuine finite-filter counterexample");
   physical_oracle::boundary(imported.scene,*original_frame,parsed.data->materials[0]);
  }else check(error.visible>20&&error.mean<=2&&error.interior_maximum<=2,"Physical standard Core-PBR output meets the unchanged finite nominal-view bounds");
  size_t descriptors=0;for(const auto &m:parsed.json.at("materials"))if(m.contains("extras")&&m.at("extras").contains("domeSculptPortableEmission")){
   ++descriptors;const auto &metrics=m.at("extras").at("domeSculptPortableEmission");
   check(!metrics.at("lossless").get<bool>()&&metrics.at("clampedTexelSamples")==0&&metrics.at("unlitIgnoresEmission")==bool(kind==5),"Finite physical descriptor distinguishes quantization, unlit omission and exact native ownership");
   check(m.at("emissiveFactor")[0].get<float>()<=1&&m.at("emissiveFactor")[1].get<float>()<=1&&m.at("emissiveFactor")[2].get<float>()<=1,"Portable emission factors stay within glTF unit bounds");
  }
  check(descriptors==(kind==6?1:imported.objects.size()),"Each actual physical emitted object receives its own measured material descriptor");
  Document reopened;reopened.open_async(package).get();reopened.shading(false);settle(reopened);
  check(reopened.copy_scene().encode()==exact&&reopened.frame().state->dirty&&reopened.frame().state->project_path.empty()&&delta(reference,draw(native,reopened.frame().surface))==0,"Real native recovery restores exact physical/source pixels and unsaved project identity");
  const auto saved=out/("recovered"+std::to_string(kind)+".dsculpt");reopened.save_async(saved).get();
  const auto cycle=out/("second"+std::to_string(kind)+".glb");reopened.export_asset(cycle,AssetExportFormat::EditableGlb256).get();
  check(Scene::load(saved).encode()==exact&&file(cycle)==bytes,"Physical native Save and second editable export cycle are exact and deterministic");
 }
 std::cout<<"PASS "<<checks<<" physical editable export checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
