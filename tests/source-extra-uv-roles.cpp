// SPDX-License-Identifier: GPL-3.0-or-later
#define DOME_NORMAL_DOMAIN_RENDER_NO_MAIN
#include "normal-domain-render.cpp"
#undef DOME_NORMAL_DOMAIN_RENDER_NO_MAIN
#include "dome/import-authoring.hpp"
#include "import-authoring-fixtures.hpp"
namespace {
import_fixture::Fixture role_fixture(int role,int coordinate){
 import_fixture::Fixture f;while(f.bin.size()%4)f.bin.push_back(0);
 const int accessor=f.values({0,0,0,1,1,1,1,0},2,"VEC2");f.j["meshes"][0]["primitives"][0]["attributes"]["TEXCOORD_"+std::to_string(coordinate)]=accessor;
 f.j["nodes"][0]["translation"]={0,0,0};f.j["nodes"][0]["rotation"]={0,0,0,1};f.j["nodes"][0]["scale"]={3,3,3};f.j["nodes"][1]["translation"]={-.14f,0,0};f.j["nodes"][2]["translation"]={.14f,0,0};
 auto &m=f.j["materials"][0];m.erase("extensions");m["alphaMode"]="OPAQUE";auto &pbr=m["pbrMetallicRoughness"];pbr.erase("baseColorTexture");pbr["baseColorFactor"]={.65f,.75f,.85f,1};pbr["metallicFactor"]=.7f;pbr["roughnessFactor"]=.6f;
 Image pixels{32,32,{}};for(int y=0;y<32;++y)for(int x=0;x<32;++x){if(role==3)pixels.rgba.insert(pixels.rgba.end(),{uint8_t(128+55*std::sin(x*.17f)),uint8_t(128+45*std::cos(y*.13f)),235,255});else pixels.rgba.insert(pixels.rgba.end(),{uint8_t(x*8),uint8_t(y*8),uint8_t(248-x*5),uint8_t(x<16?0:255)});}
 const auto png=pixels.png();while(f.bin.size()%4)f.bin.push_back(0);const auto view=f.j["bufferViews"].size();f.j["bufferViews"].push_back({{"buffer",0},{"byteOffset",f.bin.size()},{"byteLength",png.size()}});f.bin.insert(f.bin.end(),png.begin(),png.end());const auto image=f.j["images"].size();f.j["images"].push_back({{"bufferView",view},{"mimeType","image/png"}});const auto texture=f.j["textures"].size();f.j["textures"].push_back({{"source",image},{"sampler",0}});const auto info=nlohmann::json{{"index",texture},{"texCoord",coordinate}};
 if(role==0)pbr["baseColorTexture"]=info;else if(role==1)pbr["metallicRoughnessTexture"]=info;else if(role==2){m["emissiveTexture"]=info;m["emissiveFactor"]={.8f,.7f,.6f};}else if(role==3)m["normalTexture"]=info;else m["occlusionTexture"]=info;
 f.j["samplers"][0]={{"wrapS",33071},{"wrapT",33071},{"magFilter",9728},{"minFilter",9728}};return f;
}
Scene restore_raw(const Scene &scene){auto out=scene.snapshot();for(const auto &part:scene.objects())if(part.appearance){auto &o=out.object(part.id);auto b=std::make_shared<SourceMaterialBindings>(*o.material_bindings);const auto &source=*b->uv_origin;o.painted=source.paint->snapshot();o.appearance=source.appearance;o.deformation=source.deformation;b->sampling_reparameterized=false;o.material_bindings=b;}out.validate();return out;}
}
int main(int argc,char **argv)try{
 if(argc<2)return 2;const std::filesystem::path dir=argv[1];std::filesystem::create_directories(dir);NativeGraphics graphics;const std::string device=reinterpret_cast<const char*>(glGetString(GL_RENDERER));std::cout<<"GPU "<<device<<'\n';check(argc<3||std::string_view(argv[2])!="--hardware"||(device.find("Intel")!=std::string::npos&&device.find("Arc")!=std::string::npos&&device.find("A770")!=std::string::npos),"Independent per-role source UV consumer uses the exact requested device");Fbo fbo;fbo.bind();SurfaceRenderer renderer;prepared_normal_consumer::Renderer independent;
 for(int coordinate=2;coordinate<=7;++coordinate)for(int role=0;role<5;++role){auto fixture=role_fixture(role,coordinate);const auto stem="uv"+std::to_string(coordinate)+"-role"+std::to_string(role);const auto path=dir/(stem+"-source.glb");fixture.save(path);Scene before;before.active().visible=false;ImportAuthoringOptions options;options.paint_resolution=64;options.prepare_texture_atlas=true;const auto prepared=prepare_authoring_import(before,path,options);auto raw=restore_raw(prepared.scene);const auto raw_state=raw.encode();Document document;document.replace(raw);settled(document);document.shading(false);settled(document);const auto native=draw(renderer,document.frame().surface);
  prepared_normal_consumer::Model source(path);fbo.bind();independent.render(source.data);const auto input=read();const auto source_error=delta(native,input);check(source_error<=2,"Actual native original-source role UV matches an independently parsed input GLB");
  const auto output_path=dir/(stem+"-current.glb");document.export_asset(output_path,AssetExportFormat::Glb).get();prepared_normal_consumer::Model output(output_path);fbo.bind();independent.render(output.data);const auto current=read();const auto error=delta(native,current);std::cout<<"SOURCE_ROLE_UV set="<<coordinate<<" role="<<role<<" input_error="<<source_error<<" output_error="<<error<<'\n';check(error<=2,"Standard worker output preserves independent per-role UV arrays, images, samplers and current pixels");
  bool retained=true;for(size_t material=0;material<output.data->materials_count;++material){auto &m=output.data->materials[material];const cgltf_texture_view *view=role==0?&m.pbr_metallic_roughness.base_color_texture:role==1?&m.pbr_metallic_roughness.metallic_roughness_texture:role==2?&m.emissive_texture:role==3?&m.normal_texture:&m.occlusion_texture;retained&=view->texture&&view->texcoord==coordinate;}
  check(retained,"Every emitted material retains its actual source coordinate ID without renumbering onto UV1");
  for(size_t material=0;material<output.data->materials_count;++material){auto &m=output.data->materials[material];auto *view=role==0?&m.pbr_metallic_roughness.base_color_texture:role==1?&m.pbr_metallic_roughness.metallic_roughness_texture:role==2?&m.emissive_texture:role==3?&m.normal_texture:&m.occlusion_texture;view->texcoord=0;}
  fbo.bind();independent.render(output.data);const auto wrong=delta(current,read());std::cout<<"SOURCE_ROLE_WRONG_SET set="<<coordinate<<" role="<<role<<" sensitivity="<<wrong<<'\n';check(wrong>2,"Independent wrong-set sampling visibly fails for each PBR texture role");check(raw.encode()==raw_state,"Per-role export probes retain the complete original native authority exactly");
 }
 std::cout<<"PASS "<<checks<<" independently parsed all-role extra UV checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
