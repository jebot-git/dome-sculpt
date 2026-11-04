// SPDX-License-Identifier: GPL-3.0-or-later
#define main original_mtoon_fixture
#include "mtoon-render.cpp"
#undef main
#include "dome/document.hpp"
#include "baked-normal-field.hpp"
#include <fstream>
namespace {
void ready(Document &d){if(!d.wait_asset()||!d.wait_mesh()||!d.frame().state->error.empty())throw std::runtime_error(d.frame().state->error);}
Scene pose_scene(Scene scene,int pose,float weight){auto &o=scene.active();if(!o.deformation)return scene;auto d=*o.deformation;
 if(d.morphs)d.morph_weights.assign(d.morphs->targets.size(),weight);
 if(d.skeleton&&pose){d.pose.clear();for(auto &b:d.skeleton->bones)d.pose.push_back(b.rest);d.pose[0].scale=pose==1?Vec3{1.1f,.85f,.55f}:Vec3{.8f,1.05f,1.6f};const float a=pose==1?.2f:-.25f;d.pose[1].rotation={std::cos(a/2),0,std::sin(a/2),0};}
 scene.set_deformation(o.id,d);return scene;
}
void png(const std::filesystem::path&p,const std::vector<uint8_t>&v){const auto b=Image{96,96,v}.png();std::ofstream f(p,std::ios::binary);f.write(reinterpret_cast<const char*>(b.data()),b.size());}
}
int main(int argc,char**argv)try{
 if(argc!=3)return 2;const std::filesystem::path input=argv[1],out=argv[2];std::filesystem::create_directories(out);NativeGraphics gl;Fbo fbo;SurfaceRenderer renderer;unsigned failed=0,total=0;int maximum=0;
 for(int variant:{0,7}){
  auto raw=Scene::load(input/("original-"+std::to_string(variant)+".dsculpt"));raw.active().transform.scale=5;
  Document rest;rest.replace(raw);ready(rest);rest.convert_baked_normals();ready(rest);auto converted=rest.copy_scene();
  for(int pose=0;pose<3;++pose)for(float w:{0.f,.35f,1.f}){
   Document before,after;before.replace(pose_scene(raw,pose,w));after.replace(pose_scene(converted,pose,w));before.shading(false);after.shading(false);ready(before);ready(after);
   fbo.bind();auto expected=draw(renderer,before.frame().surface);fbo.bind();auto actual=draw(renderer,after.frame().surface);const auto error=delta(expected,actual);maximum=std::max(maximum,error);++total;if(error>4)++failed;
   std::cout<<"ORIGINAL_SOURCE_EQUIVALENCE variant="<<variant<<" pose="<<pose<<" weight="<<w<<" max="<<error<<" bound=4"<<std::endl;
   if(error>4){const auto name=std::to_string(variant)+"-"+std::to_string(pose)+"-"+std::to_string(int(w*100));png(out/(name+"-raw.png"),expected);png(out/(name+"-converted.png"),actual);}
  }
 }
 std::cout<<"SOURCE_EQUIVALENCE_SUMMARY total="<<total<<" failed="<<failed<<" maximum="<<maximum<<" unchanged_bound=4\n";return failed?1:0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 2;}
