// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_mtoon_suite
#include "mtoon-render.cpp"
#undef main
#include "import-skin-fixtures.hpp"
#include "source-color-consumer.hpp"
#include "dome/gltf-export.hpp"
int main(int argc,char**argv)try{
 if(argc<2)return 2;const std::filesystem::path directory=std::filesystem::absolute(argv[1]);std::filesystem::create_directories(directory/"native-home");setenv("HOME",(directory/"native-home").c_str(),1);setenv("XDG_STATE_HOME",(directory/"native-home").c_str(),1);setenv("XDG_CONFIG_HOME",(directory/"native-home").c_str(),1);
 NativeGraphics context;std::cout<<"GPU "<<glGetString(GL_RENDERER)<<"\nGL_VENDOR "<<glGetString(GL_VENDOR)<<std::endl;Fbo fbo;fbo.bind();SurfaceRenderer native;color_consumer::Renderer independent;
 cgltf_data empty{};independent.render(&empty);const auto blank=read();
 for(int mode=0;mode<3;++mode)for(bool unused:{false,true})for(float weight:{0.f,.65f}){
  auto f=mode==2?import_fixture::skinned(true):import_fixture::Fixture{};f.j["meshes"][0]["primitives"][0].erase("material");if(!unused){f.j.erase("materials");f.j.erase("extensionsUsed");}
  f.j["nodes"][1]["weights"]={weight};f.j["nodes"][2]["weights"]={weight*.5f};
  if(mode){f.j["nodes"][0]["scale"]={-1.7f,.8f,1.1f};f.j["nodes"][1]["rotation"]={0,0,std::sin(.31f),std::cos(.31f)};}
  auto path=directory/("default"+std::to_string(mode)+(unused?"-unused-":"-absent-")+std::to_string(weight)+".glb");f.save(path);ImportAuthoringOptions options;options.paint_resolution=64;auto prepared=prepare_authoring_import(Scene{},path,options);Document document;document.replace(prepared.scene);
  check(document.wait_mesh()&&document.frame().state->error.empty(),"Actual default-material source reaches native current-pose renderer");auto actual=draw(native,document.frame().surface);color_consumer::Model source(path);fbo.bind();independent.render(source.data);auto expected=read();int error=delta(actual,expected);
  std::cout<<"DEFAULT_PIXEL mode="<<mode<<" unused="<<unused<<" morph="<<weight<<" maxError="<<error<<std::endl;
  check(independent.triangles==4&&delta(expected,blank)>2,"Independent no-material consumer draws both real source instances");
  check(error<=2,"Native default factors, opaque COLOR alpha, current morph/skin and affine placement match independent raw-file consumer");
  if(unused){auto &primitive=source.data->meshes[0].primitives[0];primitive.material=source.data->materials;fbo.bind();independent.render(source.data);const int wrong=delta(expected,read());primitive.material=nullptr;std::cout<<"DEFAULT_WRONG_ZERO_PIXEL mode="<<mode<<" morph="<<weight<<" maxError="<<wrong<<std::endl;check(wrong>2,"Independent fixture rejects the visible appearance of incorrectly guessing authored material zero");}
  const auto unchanged=prepared.scene.encode();auto target=directory/("export-"+path.filename().string());document.export_asset(target,AssetExportFormat::Glb).get();color_consumer::Model exported(target);fbo.bind();independent.render(exported.data);const int exported_error=delta(expected,read());
  std::cout<<"DEFAULT_EXPORT_PIXEL mode="<<mode<<" unused="<<unused<<" morph="<<weight<<" maxError="<<exported_error<<std::endl;
  check(exported_error<=2,"Actual GLB default-material export matches the independent original-file appearance");
  check(prepared.scene.encode()==unchanged,"GLB export leaves immutable absent-material source and native state exact");
 }
 std::cout<<checks<<" default-material native/source pixel checks passed\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
