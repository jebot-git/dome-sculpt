// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_mtoon_fixture
#include "mtoon-render.cpp"
#undef main
#include "dome/native-authoring.hpp"
#include "dome/material-bindings.hpp"
#include "dome/scene-file.hpp"
#include "source-color-consumer.hpp"
#include "raw-preserving-consumer.hpp"
#include "source-paint-bake-fixtures.hpp"
#include <fstream>
#include "native-authoring-capture.hpp"
#include "gltf-document.hpp"
#include "dome/source-paint.hpp"
#include "emission-export.hpp"
namespace {
std::vector<uint8_t> file(const std::filesystem::path &p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
void physical_ink(Scene &scene) {
  auto &o=scene.active();o.painted=o.painted->snapshot();auto &paint=*o.painted;
  paint.add_layer("Actual physical raw-HDR overlay");PaintSettings settings;settings.front_only=false;settings.channel=PaintChannel::Emission;settings.emission_units=PaintEmissionUnits::Physical;settings.emission={12,.005f,3};
  Brush b;b.from=b.to={-.02f,.015f,0};b.normal={0,0,1};b.radius=.19f;b.strength=.73f;paint.begin_stroke();paint.stamp(b,settings);check(paint.end_stroke(),"Raw-HDR source receives actual physical float paint");paint.layer_properties("Actual physical raw-HDR overlay",true,.63f);
  settings.channel=PaintChannel::Mask;settings.mask=.4f;b.from=b.to={.07f,.01f,0};b.radius=.1f;paint.begin_stroke();paint.stamp(b,settings);check(paint.end_stroke(),"Raw-HDR float overlay uses actual editable layer mask");
  paint.add_layer("Hidden physical raw-HDR overlay");settings.channel=PaintChannel::Emission;settings.emission={10000,10000,10000};paint.begin_stroke();paint.stamp(b,settings);paint.end_stroke();paint.layer_properties("Hidden physical raw-HDR overlay",false,1);
}
void imported_endpoint(Document &d) {
  if(!d.wait_asset()||!d.wait_mesh()||!d.frame().state->error.empty())
    throw std::runtime_error("Raw HDR native worker: "+d.frame().state->error);
}
import_fixture::Fixture raw_hdr(int kind) {
  auto source=portable_material_fixture(kind==4?1:0);
  auto &m=source.j["materials"][0];
  m["emissiveFactor"]=kind==1?nlohmann::json{.21f,.42f,.06f}:nlohmann::json{1,.002f,.5f};
  m["extensions"]["KHR_materials_emissive_strength"]={{"emissiveStrength",kind==1?3:8}};
  source.j["extensionsUsed"].push_back("KHR_materials_emissive_strength");
  m["emissiveTexture"]={{"index",0},{"texCoord",0}};
  if(kind==2)m.erase("emissiveTexture");
  if(kind==3)m["extensions"]["KHR_materials_unlit"]=nlohmann::json::object();
  return source;
}
}
int main(int argc,char **argv)try {
 if(argc!=2)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 auto source=raw_hdr(0);const auto input=out/"raw.glb";source.save(input);
 Scene initial;initial.active().visible=false;ImportAuthoringOptions options;options.paint_resolution=64;
 auto model=prepare_authoring_import(initial,input,options).scene;const auto original=model.encode();
 auto &o=model.active();auto asset=std::make_shared<ImportedAsset>(*o.appearance->asset);
 auto appearance=std::make_shared<CreatorAppearance>(*o.appearance);appearance->asset=asset;
 auto binding=std::make_shared<SourceMaterialBindings>(*o.material_bindings);
 auto origin=std::make_shared<SourceUvOrigin>(*binding->uv_origin);origin->appearance=appearance;binding->uv_origin=origin;
 o.appearance=appearance;o.material_bindings=binding;
 // A completely consistent equal-valued clone is legitimate. The serialized
 // canonical model remains the original raw bytes and source-role metadata.
 model.validate();check(model.encode()==original,"Equal-valued complete parsed-model clone retains exact native encoding");
 NativeAuthoringExport owner{std::make_shared<const Scene>(model.snapshot()),NativeAuthoringExport::Scope::WholeScene,{},true};
 export_glb_authoring(owner,out/"equal-clone.glb");
 auto &material=asset->materials.at(size_t(appearance->material));
 for(auto &c:material.emissive)c*=.5f;material.emissive_strength*=2;
 model.validate();check(model.encode()==original,"Legally mutable DTO factor/strength tamper preserves product and canonical native bytes");
 const auto destination=out/"must-not-publish.glb";{std::ofstream f(destination);f<<"sentinel";}const auto before=file(destination);
 bool refused=false;
 try{NativeAuthoringExport tampered{std::make_shared<const Scene>(model.snapshot()),NativeAuthoringExport::Scope::WholeScene,{},true};export_glb_authoring(tampered,destination);}
 catch(const std::invalid_argument &e){std::cout<<"PROVENANCE_REFUSAL "<<e.what()<<std::endl;refused=true;}
 if(!refused){const auto b=file(destination);const auto n=uint32_t(b[12])|uint32_t(b[13])<<8|uint32_t(b[14])<<16|uint32_t(b[15])<<24;const auto j=nlohmann::json::parse(b.begin()+20,b.begin()+20+n);std::cout<<"UNAUTHENTICATED_EMITTED_MATERIAL "<<j.at("materials").at(0).dump()<<std::endl;}
 check(refused&&file(destination)==before&&model.encode()==original,
   "Original factor and strength require canonical source provenance, not product or mutable DTO trust");
 // A coordinated sampler edit used to pass matching DTO/map checks, publish,
 // then fail re-export after native decoding restored the actual raw sampler.
 auto sampling=prepare_authoring_import(initial,input,options).scene;auto &active=sampling.active();
 auto sampler_asset=std::make_shared<ImportedAsset>(*active.appearance->asset);
 auto sampler_appearance=std::make_shared<CreatorAppearance>(*active.appearance);sampler_appearance->asset=sampler_asset;
 auto sampler_binding=std::make_shared<SourceMaterialBindings>(*active.material_bindings);
 auto sampler_origin=std::make_shared<SourceUvOrigin>(*sampler_binding->uv_origin);sampler_origin->appearance=sampler_appearance;sampler_binding->uv_origin=sampler_origin;
 const auto &source_material=sampler_asset->materials.at(size_t(sampler_appearance->material));const int texture_index=source_material.emissive_texture;
 auto maps=std::make_shared<PaintSourceMaps>(*active.painted->source_maps());auto &source_texture=sampler_asset->textures.at(size_t(texture_index));source_texture.wrap_s=source_texture.wrap_s==33071?10497:33071;
 auto change=[&](PaintSourceTexture &slot,int id){if(id==texture_index&&slot.image)slot.wrap_s=source_texture.wrap_s;};
 change(maps->color,source_material.color_texture);change(maps->metal_rough,source_material.metal_rough_texture);change(maps->normal,source_material.normal_texture);change(maps->emission,source_material.emissive_texture);change(maps->occlusion,source_material.occlusion_texture);
 PaintSurfaceOptions surface;surface.policy=PaintUvPolicy::SourceAuthored;active.painted=PaintedMesh::from_maps(active.painted->mesh(),maps,active.painted->resolution(),surface);sampler_origin->paint=active.painted;active.appearance=sampler_appearance;active.material_bindings=sampler_binding;
 sampling.validate();const auto sampler_before=sampling.encode();const auto sampler_path=out/"sampler-must-not-publish.glb";{std::ofstream f(sampler_path);f<<"sampler sentinel";}const auto sampler_sentinel=file(sampler_path);refused=false;
 try{export_glb_authoring(NativeAuthoringExport{std::make_shared<const Scene>(sampling.snapshot()),NativeAuthoringExport::Scope::WholeScene,{},true},sampler_path);}
 catch(const std::invalid_argument&e){std::cout<<"SAMPLER_PROVENANCE_REFUSAL "<<e.what()<<std::endl;refused=true;}
 check(refused&&file(sampler_path)==sampler_sentinel&&sampling.encode()==sampler_before,"Coordinated mutable sampler/maps cannot replace canonical raw source authority");

 return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<std::endl;return 1;}
