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
  const bool old_negative=argc==3&&std::string_view(argv[2])=="--old-negative";
  Scene initial;initial.active().visible=false;
  ImportAuthoringOptions options;options.paint_resolution=64;options.prepare_texture_atlas=true;
  if(old_negative){auto source=raw_hdr(0);const auto path=out/"raw-hdr.glb";source.save(path);const auto before=initial.encode();
    try{prepare_authoring_import(initial,path,options);}catch(const std::exception &e){std::cerr<<"RAW_HDR_INITIAL_ADOPTION_NEGATIVE "<<e.what()<<std::endl;check(initial.encode()==before&&file(path)==source.bytes(),"Old raw-HDR adoption refusal preserves actual source and caller");return 1;}
    throw std::runtime_error("Old raw-HDR negative unexpectedly admitted");
  }
  NativeGraphics graphics;const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
  std::cout<<"GL_VENDOR="<<vendor<<" GL_RENDERER="<<device<<std::endl;
  check(argc<3||std::string_view(argv[2])!="--hardware"||(vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos),"Raw HDR hardware proof requires actual A770");
  Fbo fbo;fbo.bind();SurfaceRenderer native;native.set_frame_time(0);color_consumer::Renderer reader;
  for(int kind=0;kind<5;++kind){
    auto source=raw_hdr(kind);const auto input=out/("raw"+std::to_string(kind)+".glb");source.save(input);const auto raw_bytes=source.bytes();
    auto raw_options=options;raw_options.prepare_texture_atlas=false;
    const auto raw_native=prepare_authoring_import(initial,input,raw_options);
    check(!raw_native.scene.active().painted->source_sampling_prepared()&&
      *raw_native.scene.active().appearance->original_bytes==raw_bytes&&file(input)==raw_bytes,
      "Raw-HDR preserving admission keeps genuine source sampling and exact bytes without claiming an editable atlas");
    Document imported;imported.replace(initial.snapshot());ImportSettings settings;settings.paint_resolution=64;settings.prepare_texture_atlas=true;imported.import_asset(input,settings);imported_endpoint(imported);
    auto scene=imported.copy_scene();const auto id=scene.active().id;const auto &o=scene.object(id);const auto paint=o.painted;
    check(paint&&paint->resolution()==64&&paint->source_sampling_prepared()&&paint->valid_source_sampling_attachment()&&*o.appearance->original_bytes==raw_bytes,"Actual native raw-HDR import publishes genuine owned Source4 and exact source bytes");
    const auto &original=o.appearance->asset->materials.at(size_t(o.appearance->material));
    bool units=true;for(size_t c=0;c<3;++c)units&=o.material.emissive[c]==original.emissive[c]*original.emissive_strength&&o.material_bindings->native_reference.emission[c]==o.material.emissive[c];
    check(units&&original.emissive_strength==(kind==1?3:8)&&(kind==2?!paint->source_sampling_original()->source_maps()->emission.image:bool(paint->source_sampling_original()->source_maps()->emission.image)),"Raw factor and strength remain separate originals while native radiance uses their actual product");
    const auto pristine=scene.encode();check(Scene::decode(pristine).encode()==pristine&&!paint->has_physical_emission(),"Pristine HDR adoption has no fabricated float paint and owning codec restores exactly");
    for(int phase=0;phase<2;++phase){
      if(phase){ink(scene);physical_ink(scene);scene.validate();}
      const auto exact=scene.encode();Document d;d.replace(scene.snapshot());imported_endpoint(d);const auto authored_reference=draw(native,d.frame().surface);d.shading(false);imported_endpoint(d);const auto reference=draw(native,d.frame().surface);
      const auto package=out/("kind"+std::to_string(kind)+"-phase"+std::to_string(phase)+".glb");d.export_asset(package,AssetExportFormat::EditableGlb256).get();
      const auto captured=file(package);auto recovered=extract_native_authoring(captured);
      check(recovered&&recovered->encode()==exact&&d.copy_scene().encode()==exact&&file(input)==raw_bytes,"Actual pristine/painted raw-HDR exporter preserves complete exact native project and raw source");
      color_consumer::Model parsed(package);fbo.bind();reader.render(parsed.data,nullptr,{},true);const auto quality=fidelity(reference,read());
      std::cout<<"RAW_HDR kind="<<kind<<" phase="<<phase<<" visible="<<quality.visible<<" mean="<<quality.mean<<" interior="<<quality.interior_maximum<<std::endl;
      check(quality.visible>20&&quality.mean<=2&&quality.interior_maximum<=2,"Actual raw-HDR finite PBR consumer preserves representative two-pixel bounds");
      const auto &j=parsed.json;check(j.at("extras").contains("domeSculptAuthoring")&&!j.at("extras").contains("domeSculptEmission"),"Generic raw-HDR project uses actual native Scene authority without competing HDR records");
      Document reopened;reopened.open_async(package).get();imported_endpoint(reopened);check(delta(authored_reference,draw(native,reopened.frame().surface))==0,"Native raw-HDR recovery preserves actual default authored/MToon pixels");reopened.shading(false);imported_endpoint(reopened);
      check(reopened.copy_scene().encode()==exact&&delta(reference,draw(native,reopened.frame().surface))==0,"Actual native raw-HDR Open restores exact float/source units and pixels");
      const auto saved=out/("saved"+std::to_string(kind)+"-"+std::to_string(phase)+".dsculpt");reopened.save_async(saved).get();check(Scene::load(saved).encode()==exact,"Following native Save preserves exact raw-HDR project");
      const auto second=out/("second"+std::to_string(kind)+"-"+std::to_string(phase)+".glb");reopened.export_asset(second,AssetExportFormat::EditableGlb256).get();check(file(second)==captured,"Raw-HDR native recovery and second portable cycle are byte-identical");
    }
  }
  {const auto path=out/"raw0.glb";const auto before=initial.encode();const auto raw=file(path);size_t polls=0;auto counted=options;counted.limits.cancelled=[&]{++polls;return false;};prepare_authoring_import(initial,path,counted);
    for(size_t at:{size_t(1),polls/4,polls/2,polls*3/4,polls-1}){size_t calls=0;auto cancel=options;cancel.limits.cancelled=[&]{return ++calls>=at;};bool refused=false;try{prepare_authoring_import(initial,path,cancel);}catch(const std::exception&e){refused=std::string_view(e.what()).find("cancelled")!=std::string_view::npos;}check(refused&&initial.encode()==before&&file(path)==raw,"Raw-HDR capture/parse/frame/atlas cancellation preserves caller and original file");}}
  {auto source=raw_hdr(0);source.j["materials"][0]["extensions"]["KHR_materials_emissive_strength"]["emissiveStrength"]=10001;const auto path=out/"over-native-bound.glb";source.save(path);const auto before=initial.encode();bool refused=false;try{prepare_authoring_import(initial,path,options);}catch(const std::runtime_error&e){refused=std::string_view(e.what())=="Invalid bounded source emissive strength";}check(refused&&initial.encode()==before&&file(path)==source.bytes(),"Raw-HDR initial adoption retains the exact existing source/native radiance bound and rollback");}
  std::cout<<"PASS "<<checks<<" raw HDR initial-adoption checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
