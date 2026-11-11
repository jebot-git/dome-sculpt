// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/bake-authoring.hpp"
#include "dome/document.hpp"
#include "dome/gltf-export.hpp"
#include "emission-bake-source.hpp"
#include "emission-export.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>

namespace {
using namespace dome;
size_t checks=0;
void check(bool v,const char *message){++checks;if(!v)throw std::runtime_error(message);}
Mesh quad(){Mesh m;m.positions={{-.5f,-.5f,0},{.5f,-.5f,0},{.5f,.5f,0},{-.5f,.5f,0}};m.normals.assign(4,{0,0,1});m.uv={{{0,0}},{{1,0}},{{1,1}},{{0,1}}};m.indices={0,1,2,0,2,3};return m;}
Image image(std::array<uint8_t,4> v){Image i{64,64,std::vector<uint8_t>(4096*4)};for(size_t k=0;k<i.rgba.size();++k)i.rgba[k]=v[k%4];return i;}
struct Fixture{Scene scene;uint64_t target,source;std::vector<BakedEmissionSample> radiance;};
Fixture fixture(){
  Fixture f;f.target=f.scene.active().id;f.scene.active().mesh_active=true;f.scene.active().painted=std::make_shared<PaintedMesh>(quad(),64);
  f.scene.active().painted->add_layer("Retained hidden target");f.scene.active().painted->layer_properties("Retained hidden target",false,.25f);
  f.source=f.scene.add("Actual retained FLOAT source",Volume{});auto &o=f.scene.active();o.mesh_active=true;o.visible=false;o.locked=true;o.material.emissive={0,0,0};
  auto mesh=quad();for(auto &p:mesh.positions)p.z=.005f;o.painted=std::make_shared<PaintedMesh>(mesh,64);
  BakedTextures b;b.maps.color=image({24,65,117,255});b.maps.metal_rough=image({177,42,99,255});b.coverage.assign(4096,255);
  f.radiance.resize(4096);
  for(size_t i=0;i<4096;++i)f.radiance[i]={{},{50000.f+float(i%64)*32,.00001f*float(i/64+1),2},1};
  b.emission_field=BakedEmissionField::create(64,f.radiance);o.painted->install_bake(std::move(b));
  f.scene.select(f.target);f.scene.validate();return f;
}
void finish(Document &doc){check(doc.wait_asset(std::chrono::seconds(120)),"actual worker finishes within original120s bound");if(!doc.frame().state->error.empty())throw std::runtime_error(doc.frame().state->error);}
void saved(std::future<void> f){check(f.wait_for(std::chrono::seconds(120))==std::future_status::ready,"ordered Save finishes within120s");f.get();}
} // namespace
int main(int argc,char **argv)try{
  using namespace dome;
  if(argc!=2)throw std::invalid_argument("baked-emission-workflow output-directory");
  const auto directory=std::filesystem::path(argv[1]);std::filesystem::create_directories(directory);
  auto f=fixture();const auto before=f.scene.encode();const auto source=f.scene.object(f.source).painted;
  const auto source_value=source->encode();const auto target_layers=f.scene.active().painted->layers();
  auto sampler=detail::emission_bake_source(f.scene.object(f.source));
  check(bool(sampler),"source sampler is derived from a genuine owning native FLOAT field");
  auto sample=sampler(0,{.25f,.25f,.5f});
  check(sample[0]>50000&&sample[1]>0,"actual immutable projection-source sampler retains HDR and tiny terms");
  auto cancelled=f.scene;bool stop=false;ObjectBakeSettings settings;settings.front_mm=10;settings.back_mm=2;settings.padding=2;settings.ao_samples=0;
  bool rejected=false;try{bake_object(cancelled,f.source,settings,[&]{return stop;},[&](float progress){if(progress>=.8f)stop=true;});}catch(const std::exception &){rejected=true;}
  check(rejected&&stop&&cancelled.encode()==before,"late actual Bake cancellation retains full source/target/history owners");
  Document document;document.replace(f.scene);document.bake_from_object(f.source,settings);
  auto output=document.save_async(directory/"accepted.dsculpt");
  finish(document);saved(std::move(output));
  const auto accepted=document.copy_scene();const auto after=accepted.encode();const auto paint=accepted.active().painted;
  check(after!=before&&paint->baked()&&paint->baked()->emission_field&&paint->baked()->maps.emission.rgba.empty(),"actual worker adopts a distinct FLOAT Bake beneath editable paint");
  check(accepted.object(f.source).painted->encode()==source_value&&source->encode()==source_value,"hidden locked source and immutable held source unchanged");
  check(paint->layers().size()==target_layers.size(),"all target editable layers retained");
  for(size_t i=0;i<target_layers.size();++i){const auto &a=target_layers[i],&b=paint->layers()[i];check(a.tiles==b.tiles&&a.emission_tiles==b.emission_tiles&&a.pbr_tiles==b.pbr_tiles&&a.visible==b.visible&&a.opacity==b.opacity&&a.name==b.name,"hidden target ownership/opacity/name survive worker adoption");}
  const auto values=paint->baked()->emission_field->values();
  for(size_t i=0;i<values.size();++i){
    check(values[i].coverage==1&&values[i].legacy==std::array<float,3>{},"projected Bake is explicitly resolved absolute replacement");
    for(size_t k=0;k<3;++k)check(std::abs(double(values[i].absolute[k])-f.radiance[i].absolute[k])<=std::max(1e-10,double(f.radiance[i].absolute[k])*2e-6),"actual projection agrees with independent source field oracle");
  }
  check(document.undo()&&document.copy_scene().encode()==before,"actual global Undo restores exact native before-state");
  saved(document.save_async(directory/"undone.dsculpt"));
  check(document.redo()&&document.copy_scene().encode()==after,"actual global Redo restores exact native FLOAT Bake");
  saved(document.save_async(directory/"redone.dsculpt"));
  Document reopened;reopened.open(directory/"accepted.dsculpt");
  check(reopened.copy_scene().encode()==after,"fresh actual Document publication/reopen retains exact complete native state");
  check(Scene::load(directory/"undone.dsculpt").encode()==before&&Scene::load(directory/"redone.dsculpt").encode()==after,"ordered Save history endpoints retain exact native state");
  const auto owner=std::make_shared<const Scene>(accepted);
  const NativeEmissionExport export_owner{owner,NativeEmissionExport::Scope::WholeScene,true};
  const auto surfaces=emission_export_surfaces(*owner);
  const auto prepared=detail::prepare_emission_export(surfaces,export_owner,{});
  check(prepared.images.size()==1,"actual export evaluates only the visible target while owning the complete hidden source");
  const auto &field=prepared.images.front().image;
  check(field.version==2,"actual export field declares resolved FLOAT interpretation");
  auto exr=encode_emission_exr(field);auto restored=decode_emission_exr(exr);
  check(restored.version==2&&restored.width==field.width&&restored.height==field.height&&restored.pixels==field.pixels,"actual export FLOAT32 EXR preserves every radiance/contribution/coverage bit");
  GltfExportOptions export_options;export_options.emission=std::make_shared<const NativeEmissionExport>(export_owner);
  const auto glb=directory/"resolved-bake.glb";export_glb(surfaces,glb,export_options);
  std::ifstream stream(glb,std::ios::binary);std::vector<uint8_t> payload((std::istreambuf_iterator<char>(stream)),{});
  const auto imported=extract_native_emission(payload);
  check(imported&&imported->encode()==after,"actual GLB native/FLOAT export restores full source/target/current material/history recipe state");
  std::cout<<"Baked emission worker "<<checks<<" checks PASS\n";return 0;
}catch(const std::exception &e){std::cerr<<"Baked emission worker FAIL after "<<checks<<": "<<e.what()<<'\n';return 1;}
