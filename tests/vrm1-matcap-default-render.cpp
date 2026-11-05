// SPDX-License-Identifier: GPL-3.0-or-later
#define main shared_mtoon_test_suite
#include "mtoon-render.cpp"
#undef main
#include "vrm1-expression-layer-consumer.hpp"
#include <fstream>
int main(int argc,char **argv)try{
 if(argc!=3)return 2;const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);NativeGraphics graphics;Fbo fbo;role_consumer::Model model(argv[1]);const auto exact=model.json;V1ExpressionLayerConsumer consumer;const Mat4 camera{5.27903413772583f,0,0,0,0,5.27903413772583f,0,0,0,0,5.27903413772583f,0,-2.639517307281494f,-.6387631297111511f,0,1};
 bool absent=false;for(const auto &m:model.json.at("materials"))if(m.contains("extensions")&&m.at("extensions").contains("VRMC_materials_mtoon"))absent|=!m.at("extensions").at("VRMC_materials_mtoon").contains("matcapFactor");check(absent,"Actual valid original source omits optional MatCap factor");
 for(int pose:{0,2})for(float weight:{0.f,.35f,1.f})for(int eye:{0,1}){consumer.pose=pose;consumer.weights={{"preset/happy",weight}};auto view=camera;if(eye)view[12]+=.008f;fbo.bind();const auto pixels=consumer.render(model,0,view);size_t coverage=0;for(size_t i=3;i<pixels.size();i+=4)coverage+=pixels[i]!=0;check(coverage>300,"Valid missing-factor source renders at all original pose/weight/eye states");const auto png=Image{96,96,pixels}.png();const auto name="p"+std::to_string(pose)+"-w"+std::to_string(weight)+"-e"+std::to_string(eye)+".png";std::ofstream f(out/name,std::ios::binary);f.write(reinterpret_cast<const char*>(png.data()),png.size());}check(model.json==exact,"Independent evaluation leaves actual raw source metadata exact");std::cout<<"PASS "<<checks<<" actual missing-factor compatibility checks\n";return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
