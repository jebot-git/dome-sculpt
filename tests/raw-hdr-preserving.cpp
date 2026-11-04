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
#include <jpeglib.h>
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
  if(argc<2||argc>3)return 2;
  const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
  NativeGraphics graphics;
  const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  std::cout<<"GL_VENDOR="<<vendor<<" GL_RENDERER="<<device<<std::endl;
  check(argc<3||std::string_view(argv[2])!="--hardware"||
    (vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos),"Raw preserving hardware proof requires actual A770");
  Fbo fbo;fbo.bind();SurfaceRenderer native;native.set_frame_time(0);raw_preserving_consumer::Renderer reader;
  for(int kind=0;kind<16;++kind) {
    auto source=raw_hdr(kind<5?kind:0);auto &m=source.j["materials"][0];
    if(kind==5||kind>=8) {
      source=portable_material_fixture(6);auto &material=source.j["materials"][0];
      material["emissiveFactor"]={1,.002f,.01f};material["extensions"]["KHR_materials_emissive_strength"]={{"emissiveStrength",2}};
      material["emissiveTexture"]={{"index",0},{"texCoord",1}};material["normalTexture"]["texCoord"]=1;
      source.j["extensionsUsed"].push_back("KHR_materials_emissive_strength");
      if(kind==8||kind==11){
        while(source.bin.size()%4)source.bin.push_back(0);
        auto &primitive=source.j["meshes"][0]["primitives"][0];
        primitive["attributes"]["TEXCOORD_2"]=source.values({.15f,.85f,.85f,.85f,.6f,.15f,.2f,.25f},2,"VEC2");
        primitive["attributes"]["TEXCOORD_3"]=source.values({.01f,.1f,.31f,.2f,.61f,.3f,.91f,.4f},2,"VEC2");
        material["normalTexture"]["texCoord"]=2;material["emissiveTexture"]["texCoord"]=2;
        if(kind==11){primitive["attributes"].erase("TANGENT");for(auto &target:primitive["targets"])target.erase("TANGENT");}
      }
      if(kind==9){material["emissiveTexture"]["texCoord"]=0;material["emissiveTexture"]["extensions"]["KHR_texture_transform"]={{"offset",{.13f,-.27f}},{"scale",{.8f,1.1f}},{"rotation",.17f},{"texCoord",1}};source.j["extensionsUsed"].push_back("KHR_texture_transform");}
      if(kind==10)source.j["nodes"][1]["scale"]={-5,5,5};
      if(kind==12){
        auto &primitive=source.j["meshes"][0]["primitives"][0];
        const auto values=source.values({.15f,.85f,.85f,.85f,.6f,.15f,.2f,.25f},2,"VEC2");
        while(source.bin.size()%4)source.bin.push_back(0);const auto at=source.bin.size();source.bin.insert(source.bin.end(),{0,1,2,3});
        const auto view=source.j["bufferViews"].size();source.j["bufferViews"].push_back({{"buffer",0},{"byteOffset",at},{"byteLength",4}});
        const auto accessor=source.j["accessors"].size();source.j["accessors"].push_back({{"componentType",5126},{"count",4},{"type","VEC2"},{"sparse",{{"count",4},{"indices",{{"bufferView",view},{"componentType",5121}}},{"values",{{"bufferView",source.j["accessors"][values]["bufferView"]}}}}}});
        primitive["attributes"]["TEXCOORD_1"]=accessor;
      }
      if(kind==13){
        while(source.bin.size()%4)source.bin.push_back(0);const auto at=source.bin.size();
        for(uint16_t v:{9830,55705,55705,55705,39321,9830,13107,16384}){source.bin.push_back(uint8_t(v));source.bin.push_back(uint8_t(v>>8));}
        const auto view=source.j["bufferViews"].size();source.j["bufferViews"].push_back({{"buffer",0},{"byteOffset",at},{"byteLength",16}});
        const auto accessor=source.j["accessors"].size();source.j["accessors"].push_back({{"bufferView",view},{"componentType",5123},{"normalized",true},{"count",4},{"type","VEC2"}});source.j["meshes"][0]["primitives"][0]["attributes"]["TEXCOORD_1"]=accessor;
      }
      if(kind==14){
        jpeg_compress_struct encoder{};jpeg_error_mgr errors{};encoder.err=jpeg_std_error(&errors);jpeg_create_compress(&encoder);unsigned char *bytes=nullptr;unsigned long count=0;jpeg_mem_dest(&encoder,&bytes,&count);encoder.image_width=encoder.image_height=16;encoder.input_components=3;encoder.in_color_space=JCS_RGB;jpeg_set_defaults(&encoder);jpeg_set_quality(&encoder,100,TRUE);jpeg_start_compress(&encoder,TRUE);
        std::array<unsigned char,48> row{};while(encoder.next_scanline<16){const auto y=encoder.next_scanline;for(size_t x=0;x<16;++x){row[x*3]=uint8_t(20+x*13);row[x*3+1]=uint8_t(30+y*12);row[x*3+2]=uint8_t(220-x*5-y*4);}JSAMPROW pixels=row.data();jpeg_write_scanlines(&encoder,&pixels,1);}jpeg_finish_compress(&encoder);jpeg_destroy_compress(&encoder);
        while(source.bin.size()%4)source.bin.push_back(0);const auto at=source.bin.size();source.bin.insert(source.bin.end(),bytes,bytes+count);std::free(bytes);const auto view=source.j["bufferViews"].size();source.j["bufferViews"].push_back({{"buffer",0},{"byteOffset",at},{"byteLength",count}});source.j["images"][0]["bufferView"]=view;source.j["images"][0]["mimeType"]="image/jpeg";
      }
      if(kind==15){
        auto sampler=source.j["samplers"][0];sampler["wrapS"]=33071;sampler["wrapT"]=33648;sampler["minFilter"]=9728;sampler["magFilter"]=9729;const auto sampler_id=source.j["samplers"].size();source.j["samplers"].push_back(sampler);
        auto texture=source.j["textures"][0];texture["sampler"]=sampler_id;const auto texture_id=source.j["textures"].size();source.j["textures"].push_back(texture);material["emissiveTexture"]["index"]=texture_id;
      }

    }
    if(kind==6) {
      auto &p=source.j["meshes"][0]["primitives"][0];
      p["attributes"]["TEXCOORD_0"]=source.values({0,0,1,0,1,1,0,1},2,"VEC2");
      m["alphaMode"]="MASK";m["alphaCutoff"]=.4f;
    }
    if(kind==7) {
      m["pbrMetallicRoughness"].erase("baseColorTexture");m.erase("emissiveTexture");
      m["extensions"].erase("VRMC_materials_mtoon");
      // A genuine joint/IBM/weight source, not pose applied to exported pixels.
      auto &primitive=source.j["meshes"][0]["primitives"][0];
      const auto offset=source.bin.size();
      for(int v=0;v<16;++v){source.bin.push_back(0);source.bin.push_back(0);}
      const auto view=source.j["bufferViews"].size();
      source.j["bufferViews"].push_back({{"buffer",0},{"byteOffset",offset},{"byteLength",32}});
      const auto joints=source.j["accessors"].size();
      source.j["accessors"].push_back({{"bufferView",view},{"componentType",5123},{"count",4},{"type","VEC4"}});
      primitive["attributes"]["JOINTS_0"]=joints;
      primitive["attributes"]["WEIGHTS_0"]=source.values({1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0},4,"VEC4");
      const auto inverse_bind=source.values({5,0,0,0,0,5,0,0,0,0,5,0,0,0,0,1},16,"MAT4");
      const auto joint=source.j["nodes"].size();source.j["nodes"].push_back({{"name","Real HDR source joint"}});
      source.j["nodes"][0]["children"].push_back(joint);source.j["nodes"][1]["skin"]=0;
      source.j["skins"]={{{"joints",{joint}},{"inverseBindMatrices",inverse_bind}}};
    }
    const auto input=out/("raw"+std::to_string(kind)+".glb");source.save(input);const auto raw=file(input);
    Scene initial;initial.active().visible=false;Document d;d.replace(initial.snapshot());
    ImportSettings settings;settings.paint_resolution=64;settings.prepare_texture_atlas=false;
    d.import_asset(input,settings);
    if(argc==3&&std::string_view(argv[2])=="--old-negative") {
      try{imported_endpoint(d);}catch(const std::exception &e){
        std::cout<<"OLD_RAW_PRESERVING_NEGATIVE "<<e.what()<<std::endl;
        check(d.copy_scene().encode()==initial.encode()&&file(input)==raw,
          "Old preserving-HDR admission refusal keeps exact caller Scene and source bytes");
        return 1;
      }
      throw std::runtime_error("Old preserving-HDR counterfactual unexpectedly admitted");
    }
    imported_endpoint(d);
    const auto base=d.copy_scene().encode();
    check(!d.copy_scene().active().painted->source_sampling_prepared()&&
      *d.copy_scene().active().appearance->original_bytes==raw&&file(input)==raw,
      "Actual native preserving import retains raw HDR source and makes no atlas claim");
    const auto before=d.frame().state;
    check(!d.begin_native_stroke()&&!d.frame().state->native_pending&&
      d.copy_scene().encode()==base&&d.frame().state->revision==before->revision&&
      d.frame().state->can_undo==before->can_undo,
      "Raw HDR paint admission refuses before gesture/history including unit UV0");
    const auto saved=out/("native"+std::to_string(kind)+".dsculpt");d.save_async(saved).get();
    check(Scene::load(saved).encode()==base,"Preserving-only native Save/restart keeps complete raw HDR state");
    for(int phase=0;phase<2;++phase) {
      if(phase) {
        d.edit_scene("Current raw radiance and existing morph",[](Scene &scene,const auto&){
          auto &o=scene.active();o.material.emissive={3,.002f,.1f};
          if(o.deformation){auto def=std::make_shared<DeformationState>(*o.deformation);
            if(def->morphs)def->morph_weights.assign(def->morphs->targets.size(),.35f);
            if(def->skeleton){if(def->pose.empty())for(const auto &bone:def->skeleton->bones)def->pose.push_back(bone.rest);def->pose[0].rotation={std::cos(.075f),0,std::sin(.075f),0};}
            o.deformation=std::move(def);}
        });imported_endpoint(d);
        const auto edited=d.copy_scene().encode();
        check(d.undo()&&d.copy_scene().encode()==base&&d.redo()&&d.copy_scene().encode()==edited,
          "Current raw radiance/morph edit has exact native Undo/Redo");imported_endpoint(d);
      }
      std::cout<<"STAGE raw-preserving "<<kind<<"/"<<phase<<" actual-native-render"<<std::endl;
      const auto exact=d.copy_scene().encode();d.shading(true);imported_endpoint(d);
      const auto authored=draw(native,d.frame().surface);
      d.shading(false);imported_endpoint(d);const auto reference=draw(native,d.frame().surface);
      const auto path=out/("kind"+std::to_string(kind)+"-phase"+std::to_string(phase)+".glb");
      std::cout<<"STAGE raw-preserving "<<kind<<"/"<<phase<<" actual-native-export"<<std::endl;
      d.export_asset(path,AssetExportFormat::EditableGlb256).get();
      const auto bytes=file(path);const auto restored=extract_native_authoring(bytes);
      check(restored&&restored->encode()==exact&&d.copy_scene().encode()==exact&&file(input)==raw,
        "Raw preserving editable export keeps the ORIGINAL native Scene and source bytes");
      color_consumer::Model model(path);fbo.bind();reader.render(model.data,nullptr,{},false);
      const auto quality=fidelity(reference,read());
      std::cout<<"RAW_PRESERVING kind="<<kind<<" phase="<<phase<<" visible="<<quality.visible<<" mean="<<quality.mean<<" interior="<<quality.interior_maximum<<std::endl;
      check(quality.visible>20&&quality.mean<=2&&quality.interior_maximum<=2,
        "Independent raw source PBR consumer retains actual sampler/normal/morph pixels within two");
      if(kind==8||kind==11){const auto &primitive=model.json.at("meshes").at(0).at("primitives").at(0);
        check(primitive.at("attributes").contains("TEXCOORD_0")&&primitive.at("attributes").contains("TEXCOORD_1")&&primitive.at("attributes").contains("TEXCOORD_2")&&primitive.at("attributes").contains("TEXCOORD_3")&&primitive.at("material")==0,
          "Raw preserving output retains genuine contiguous authored and unused source UV arrays");}
      bool descriptor=false;
      for(const auto &material:model.json["materials"])if(material.contains("extras")&&material["extras"].contains("domeSculptPortableEmission")) {
        const auto &e=material["extras"]["domeSculptPortableEmission"];
        descriptor|=e.at("evaluation")=="current-raw-source-sampling"&&!e.at("imageResampled").get<bool>()&&
          e.at("originalFactorsRetained").get<bool>()==!bool(phase);
      }
      check(descriptor,"Raw native producer declares actual original sampling and truthful factor provenance");
      Document reopened;reopened.open_async(path).get();imported_endpoint(reopened);
      check(delta(authored,draw(native,reopened.frame().surface))==0,
        "Raw native recovery preserves default authored pixels exactly");
      reopened.shading(false);imported_endpoint(reopened);
      check(delta(reference,draw(native,reopened.frame().surface))==0,
        "Raw native recovery preserves actual CorePBR pixels exactly");
      const auto again=out/("again"+std::to_string(kind)+"-"+std::to_string(phase)+".glb");
      reopened.export_asset(again,AssetExportFormat::EditableGlb256).get();
      check(extract_native_authoring(file(again))->encode()==exact&&file(path)==bytes,
        "Recovered raw native project re-exports without recapturing the package as source");
      if(kind==5||kind>=8) {
        auto &material=model.data->materials[0];const auto old=material.normal_texture.texture;
        material.normal_texture.texture=nullptr;fbo.bind();reader.render(model.data,nullptr,{},false);
        check(delta(reference,read())>4,"Independent lit raw normal disable counterfactual is visibly sensitive");material.normal_texture.texture=old;
        const auto uv=material.emissive_texture.texcoord;const auto transformed_uv=material.emissive_texture.transform.texcoord;const bool transformed=material.emissive_texture.has_transform&&material.emissive_texture.transform.has_texcoord;if(transformed)material.emissive_texture.transform.texcoord=0;else material.emissive_texture.texcoord=0;
        fbo.bind();reader.render(model.data,nullptr,{},false);
        check(delta(reference,read())>4,"Independent wrong raw emission UV set is visibly sensitive");material.emissive_texture.texcoord=uv;material.emissive_texture.transform.texcoord=transformed_uv;
        if(kind==9){auto texture=*material.emissive_texture.texture;auto sampler=*texture.sampler;const auto prior=material.emissive_texture.texture;sampler.mag_filter=cgltf_filter_type_linear;sampler.min_filter=cgltf_filter_type_linear;texture.sampler=&sampler;material.emissive_texture.texture=&texture;fbo.bind();reader.render(model.data,nullptr,{},false);check(delta(reference,read())>4,"Independent wrong raw emission filter remains visibly sensitive");material.emissive_texture.texture=prior;}
      }
    }
  }
  std::cout<<"PASS "<<checks<<" raw preserving HDR checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
