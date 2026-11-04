// SPDX-License-Identifier: GPL-3.0-or-later
#define main existing_native_material_fixture
#include "mtoon-render.cpp"
#undef main
#include "prepared-normal-consumer.hpp"
#include "dome/document.hpp"
#include "dome/scene-file.hpp"
int main(int argc,char **argv)try{
 if(argc!=3)return 2;const std::filesystem::path source=argv[1],out=argv[2];std::filesystem::create_directories(out);
 NativeGraphics graphics;Fbo fbo;SurfaceRenderer native;native.set_frame_time(0);prepared_normal_consumer::Renderer consumer;
 std::cout<<"GPU "<<reinterpret_cast<const char*>(glGetString(GL_RENDERER))<<'\n';
 for(int variant=0;variant<4;++variant){const auto initial=load_scene_file(source/("fan-"+std::to_string(variant)+".glb")).scene;
  for(int pose=0;pose<2;++pose)for(float weight:{0.f,.35f,1.f}){
   auto scene=initial;auto &object=scene.active();if(object.deformation){auto d=*object.deformation;if(d.morphs)d.morph_weights={weight};
    if(d.skeleton){for(const auto &bone:d.skeleton->bones)d.pose.push_back(bone.rest);d.pose[0].rotation={0,0,std::sin(.075f),std::cos(.075f)};if(pose)d.pose[1].rotation={0,0,std::sin(.1f),std::cos(.1f)};}scene.set_deformation(object.id,d);}
   Document document;document.replace(scene);document.shading(false);
   check(document.wait_asset()&&document.wait_mesh()&&document.frame().state->error.empty(),"Actual native mirrored normal model publishes its current pose and morph");
   const auto file=out/("view-"+std::to_string(variant)+"-"+std::to_string(pose)+"-"+std::to_string(int(weight*100))+".glb");
   document.export_asset(file,AssetExportFormat::EditableProject256).get();prepared_normal_consumer::Model model(file);
   fbo.bind();auto expected=draw(native,document.frame().surface);fbo.bind();consumer.render(model.data);auto actual=read();const auto error=delta(expected,actual);
   size_t visible=0;for(size_t i=3;i<actual.size();i+=4)visible+=actual[i]>0&&expected[i]>0;
   std::cout<<"BAKED_TANGENT_IMAGE variant="<<variant<<" pose="<<pose<<" morph="<<weight<<" visible="<<visible<<" max="<<error<<'\n';
   check(visible>10,"Independent parsed normal-texture consumer and native view draw the genuine model");
   check(error<=4,"Independent normal-texture image matches the native baked field at the unchanged four-level bound");
  }
 }
 std::cout<<"PASS "<<checks<<" baked tangent image checks\n";return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
