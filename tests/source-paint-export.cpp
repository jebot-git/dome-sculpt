// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_mtoon_suite
#include "mtoon-render.cpp"
#undef main
#include "dome/document.hpp"
#include "dome/import-authoring.hpp"
#include "dome/material-bindings.hpp"
#include "import-authoring-fixtures.hpp"
#include "source-color-consumer.hpp"
#include "prepared-normal-consumer.hpp"
namespace {
import_fixture::Fixture source(int kind,bool flat,const char *alpha,float weight){
 import_fixture::Fixture f;while(f.bin.size()%4)f.bin.push_back(0);
 auto &p=f.j["meshes"][0]["primitives"][0];p["attributes"]["TEXCOORD_0"]=f.values({-1.25f,-.5f,1.75f,-.5f,1.75f,2.5f,-1.25f,2.5f},2,"VEC2");
 f.j["nodes"][0]["translation"]={0,0,0};f.j["nodes"][0]["scale"]={3,3,3};
 f.j["nodes"][1]["translation"]={-.14f,0,0};f.j["nodes"][1]["scale"]={1,1,1};f.j["nodes"][2]["translation"]={.14f,0,0};
 f.j["nodes"][1]["weights"]={weight};f.j["nodes"][2]["weights"]={weight};
 auto &m=f.j["materials"][0];m.erase("extensions");m["alphaMode"]=alpha;m["pbrMetallicRoughness"]["baseColorFactor"][3]=.75f;
 if(kind==1){m["pbrMetallicRoughness"]["baseColorTexture"]["texCoord"]=1;
  auto png=image({230,153,204,255})->png();while(f.bin.size()%4)f.bin.push_back(0);const auto view=f.j["bufferViews"].size();
  f.j["bufferViews"].push_back({{"buffer",0},{"byteOffset",f.bin.size()},{"byteLength",png.size()}});f.bin.insert(f.bin.end(),png.begin(),png.end());
  const auto im=f.j["images"].size();f.j["images"].push_back({{"bufferView",view},{"mimeType","image/png"}});const auto texture=f.j["textures"].size();f.j["textures"].push_back({{"source",im},{"sampler",0}});m["normalTexture"]={{"index",texture},{"texCoord",1},{"scale",.6f}};
 }
 if(kind>=2)m["pbrMetallicRoughness"].erase("baseColorTexture");
 if(kind==3){p["attributes"].erase("TEXCOORD_0");p["attributes"].erase("TEXCOORD_1");}
 if(flat){p["attributes"].erase("NORMAL");p["targets"][0].erase("NORMAL");}
 return f;
}
void settled(Document &d){check(d.wait_mesh()&&d.frame().state->error.empty(),"Actual imported native project publishes its complete render surface");}
}
int main(int argc,char **argv)try{
 if(argc<2||argc>3)return 2;const std::filesystem::path out=argv[1];std::filesystem::create_directories(out);
 NativeGraphics graphics;const std::string vendor=reinterpret_cast<const char*>(glGetString(GL_VENDOR)),device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));
 std::cout<<"GL_VENDOR="<<vendor<<" GL_RENDERER="<<device<<'\n';check(argc<3||std::string_view(argv[2])!="--hardware"||(vendor.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos),"Requested standard source export proof requires actual A770");
 Fbo fbo;fbo.bind();SurfaceRenderer native;color_consumer::Renderer independent;prepared_normal_consumer::Renderer separate_roles;int maximum=0;
 Scene initial;initial.active().visible=false;ImportAuthoringOptions options;options.paint_resolution=64;options.prepare_texture_atlas=true;
 for(int kind=0;kind<4;++kind)for(bool flat:{false,true})for(const char *alpha:{"OPAQUE","MASK","BLEND"})for(float weight:{0.f,.65f}){
  auto fixture=source(kind,flat,alpha,weight);const auto prefix=std::to_string(kind)+(flat?"-flat-":"-artist-")+alpha+"-"+std::to_string(weight);const auto path=out/(prefix+"-input.glb");fixture.save(path);
  auto imported=prepare_authoring_import(initial,path,options);const auto before=imported.scene.encode();
  Document document;document.replace(Scene::decode(before));document.shading(false);settled(document);const auto pixels=draw(native,document.frame().surface);
  size_t visible=0;for(size_t p=3;p<pixels.size();p+=4)visible+=pixels[p]>0;check(visible>128,"Independent export comparison contains substantial visible surface coverage");
  const auto output=out/(prefix+"-output.glb");document.export_asset(output).get();
  color_consumer::Model model(output);fbo.bind();if(kind==1)separate_roles.render(model.data);else independent.render(model.data);const int error=delta(pixels,read());maximum=std::max(maximum,error);
  std::cout<<"SOURCE_EXPORT "<<prefix<<" max="<<error<<'\n';check(error<=2,"Independently parsed standard GLB matches actual source-aware imported PBR pixels, source sampling, frames, alpha, colour and morphs");
  check(document.copy_scene().encode()==before&&Scene::decode(before).encode()==before,"Standard export compilation leaves exact native paint, source coordinate owners and source history unchanged");
  size_t materials=0;for(size_t n=0;n<model.data->materials_count;++n){const auto &material=model.data->materials[n];
   if(kind<2){const auto &texture=material.pbr_metallic_roughness.base_color_texture;check(texture.texture&&texture.texcoord==kind&&texture.texture->sampler&&texture.texture->sampler->wrap_s==cgltf_wrap_mode_mirrored_repeat&&texture.texture->sampler->min_filter==cgltf_filter_type_nearest_mipmap_linear,"Standard export retains original image coordinate, mirrored repeat and mip filter");++materials;}
   else check(!material.pbr_metallic_roughness.base_color_texture.texture,"Untextured export invents no replacement image");
   if(kind==1)check(material.normal_texture.texture&&material.normal_texture.texcoord==1&&std::abs(material.normal_texture.scale-.6f)<1e-6f,"Original UV1 normal role and strength survive writable atlas import/export");
  }
  check(kind>=2||materials==2,"Both selected source instances retain their independently emitted material bindings");
  if(kind==0&&!flat&&std::string_view(alpha)=="OPAQUE"&&weight==0){
   int polls=0;auto fresh=Scene::decode(before);auto probe=fresh.export_surfaces([&]{++polls;return false;});check(polls>1&&probe.size()==2,"Actual original-coordinate export staging exposes cancellation checkpoints");
   for(int boundary=1;boundary<=polls;++boundary){int count=0;auto candidate=Scene::decode(before);check(rejects([&]{candidate.export_surfaces([&]{return ++count>=boundary;});})&&count==boundary&&candidate.encode()==before,"Every actual export coordinate cancellation preserves the original complete native scene");}
   auto &object=imported.scene.active();auto paint=object.painted->snapshot();PaintSettings ink;ink.front_only=false;ink.color={.1f,.8f,.3f,.6f};Brush brush;brush.from=brush.to={0,0,0};brush.normal={0,0,1};brush.radius=1;paint->begin_stroke();paint->stamp(brush,ink);check(paint->end_stroke(),"Real source-overlay paint contributes before the unimplemented portable bake control");object.painted=paint;
   const auto painted=imported.scene.encode();check(rejects([&]{imported.scene.export_surfaces();})&&imported.scene.encode()==painted,"Unimplemented painted standard bake refuses without silently losing paint or changing source state");
  }
 }
 std::cout<<"PASS "<<checks<<" source-aware original standard export checks maxerror="<<maximum<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
