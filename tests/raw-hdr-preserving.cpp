// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_mtoon_fixture
#include "mtoon-render.cpp"
#undef main
#include "dome/native-authoring.hpp"
#include "dome/material-bindings.hpp"
#include "dome/scene-file.hpp"
#include "source-color-consumer.hpp"
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
  if(argc<2||argc>3)return 2;
  const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
  NativeGraphics graphics;
  const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  std::cout<<"GL_VENDOR="<<vendor<<" GL_RENDERER="<<device<<std::endl;
  check(argc<3||std::string_view(argv[2])!="--hardware"||
    (vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos),"Raw preserving hardware proof requires actual A770");
  Fbo fbo;fbo.bind();SurfaceRenderer native;native.set_frame_time(0);color_consumer::Renderer reader;
  for(int kind=0;kind<8;++kind) {
    auto source=raw_hdr(kind<5?kind:0);auto &m=source.j["materials"][0];
    if(kind==5) {
      source=portable_material_fixture(6);auto &material=source.j["materials"][0];
      material["emissiveFactor"]={1,.002f,.01f};material["extensions"]["KHR_materials_emissive_strength"]={{"emissiveStrength",2}};
      material["emissiveTexture"]={{"index",0},{"texCoord",1}};material["normalTexture"]["texCoord"]=1;
      source.j["extensionsUsed"].push_back("KHR_materials_emissive_strength");
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
      const auto exact=d.copy_scene().encode();d.shading(true);imported_endpoint(d);
      const auto authored=draw(native,d.frame().surface);
      d.shading(false);imported_endpoint(d);const auto reference=draw(native,d.frame().surface);
      const auto path=out/("kind"+std::to_string(kind)+"-phase"+std::to_string(phase)+".glb");
      d.export_asset(path,AssetExportFormat::EditableGlb256).get();
      const auto bytes=file(path);const auto restored=extract_native_authoring(bytes);
      check(restored&&restored->encode()==exact&&d.copy_scene().encode()==exact&&file(input)==raw,
        "Raw preserving editable export keeps the ORIGINAL native Scene and source bytes");
      color_consumer::Model model(path);fbo.bind();reader.render(model.data,nullptr,{},true);
      const auto quality=fidelity(reference,read());
      std::cout<<"RAW_PRESERVING kind="<<kind<<" phase="<<phase<<" visible="<<quality.visible<<" mean="<<quality.mean<<" interior="<<quality.interior_maximum<<std::endl;
      check(quality.visible>20&&quality.mean<=2&&quality.interior_maximum<=2,
        "Independent raw source PBR consumer retains actual sampler/normal/morph pixels within two");
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
      if(kind==5) {
        auto &material=model.data->materials[0];const auto old=material.normal_texture.texture;
        material.normal_texture.texture=nullptr;fbo.bind();reader.render(model.data,nullptr,{},true);
        check(delta(reference,read())>4,"Independent lit raw normal disable counterfactual is visibly sensitive");material.normal_texture.texture=old;
        const auto uv=material.emissive_texture.texcoord;material.emissive_texture.texcoord=0;
        fbo.bind();reader.render(model.data,nullptr,{},true);
        check(delta(reference,read())>4,"Independent wrong raw emission UV set is visibly sensitive");material.emissive_texture.texcoord=uv;
      }
    }
  }
  std::cout<<"PASS "<<checks<<" raw preserving HDR checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
