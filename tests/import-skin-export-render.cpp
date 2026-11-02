// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_mtoon_suite
#include "mtoon-render.cpp"
#undef main
#include "import-skin-fixtures.hpp"
#include "source-color-consumer.hpp"
#include "dome/gltf-export.hpp"
int main(int argc,char**argv)try{
 if(argc<2)return 2;const std::filesystem::path directory=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(directory/"native-home");setenv("HOME",(directory/"native-home").c_str(),1);setenv("XDG_CONFIG_HOME",(directory/"native-home").c_str(),1);setenv("XDG_STATE_HOME",(directory/"native-home").c_str(),1);
 NativeGraphics context;std::cout<<"GPU "<<glGetString(GL_RENDERER)<<"\nGL_VENDOR "<<glGetString(GL_VENDOR)<<std::endl;Fbo fbo;fbo.bind();SurfaceRenderer native;color_consumer::Renderer independent;cgltf_data empty{};independent.render(&empty);const auto blank=read();
 for(bool reflected:{false,true})for(bool authored:{false,true})for(bool double_sided:{false,true})for(float weight:{0.f,.65f}){
  auto f=import_fixture::skinned(authored);f.j["nodes"][0]["scale"]={reflected?-1.7f:1.7f,.8f,1.1f};f.j["nodes"][1]["rotation"]={0,0,std::sin(.31f),std::cos(.31f)};f.j["nodes"][3]["scale"]={1.2f,.9f,1.1f};f.j["nodes"][4]["scale"]={.7f,1.3f,.8f};f.j["nodes"][3]["rotation"]={0,0,std::sin(.15f),std::cos(.15f)};f.j["nodes"][1]["weights"]={weight};f.j["nodes"][2]["weights"]={weight*.5f};
  auto &material=f.j["materials"][0];material.erase("extensions");material["doubleSided"]=double_sided;material["alphaMode"]="OPAQUE";
  const Image bump{1,1,{230,153,204,255}};const auto png=bump.png();while(f.bin.size()%4)f.bin.push_back(0);const int view=int(f.j["bufferViews"].size());f.j["bufferViews"].push_back({{"buffer",0},{"byteOffset",f.bin.size()},{"byteLength",png.size()}});f.bin.insert(f.bin.end(),png.begin(),png.end());f.j["images"].push_back({{"mimeType","image/png"},{"bufferView",view}});f.j["textures"].push_back({{"source",1},{"sampler",0}});material["normalTexture"]={{"index",1},{"scale",.8f}};
  const auto tag=std::to_string(reflected)+"-"+std::to_string(authored)+"-"+std::to_string(double_sided)+"-"+std::to_string(weight);const auto path=directory/("source-"+tag+".glb");f.save(path);ImportAuthoringOptions options;options.paint_resolution=64;auto prepared=prepare_authoring_import(Scene{},path,options);auto duplicate=prepared.scene.duplicate(prepared.objects.front());prepared.scene.object(duplicate).name="Shared rig duplicate";Document document;document.replace(prepared.scene);
  check(document.wait_mesh()&&document.frame().state->error.empty(),"Actual authored-material skin reaches native renderer");auto actual=draw(native,document.frame().surface);color_consumer::Model source(path);fbo.bind();independent.render(source.data);const auto expected=read();const int native_error=delta(actual,expected);check(independent.triangles==4&&delta(expected,blank)>2,"Independent source draws both nonempty skinned instances");
  std::cout<<"SKIN_NATIVE_PIXEL case="<<tag<<" maxError="<<native_error<<std::endl;check(native_error<=2,"Native reflected/single-sided/mapped/morphed skin matches independent source");
  const auto before=prepared.scene.encode();const auto target=directory/("export-"+tag+".glb");document.export_asset(target,AssetExportFormat::Glb).get();color_consumer::Model exported(target);fbo.bind();independent.render(exported.data);const int exported_error=delta(expected,read());std::cout<<"SKIN_EXPORT_PIXEL case="<<tag<<" maxError="<<exported_error<<std::endl;
  check(exported_error<=2,"Actual GLB export preserves skinned mesh-node culling and tangent handedness");
  size_t surfaces=0;for(size_t n=0;n<exported.data->nodes_count;++n)if(exported.data->nodes[n].mesh){std::array<float,16> world{};cgltf_node_transform_world(exported.data->nodes+n,world.data());check(consumer_frame::handedness(world)==(reflected?-1.f:1.f),"Exported mesh global determinant keeps original reflected front-face authority");++surfaces;}
  check(exported.data->skins_count==2,"Duplicated part shares its actual rig without duplicating joint/bind state");check(surfaces==3&&prepared.scene.encode()==before&&document.copy_scene().encode()==before,"Worker export retains separate/shared-rig instances and exact source/native state");
 }
 std::cout<<checks<<" skinned surface export/handedness pixel checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
