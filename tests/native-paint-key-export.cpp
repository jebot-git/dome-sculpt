// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/rig-edit.hpp"
#include "native-paint-frame-source.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
using namespace dome;
namespace {
size_t checks=0;
void check(bool value,const char*label){if(!value)throw std::runtime_error(label);++checks;std::cout<<"PASS "<<label<<std::endl;}
std::vector<uint8_t> bytes(const std::filesystem::path&p){std::ifstream f(p,std::ios::binary);return {std::istreambuf_iterator<char>(f),{}};}
}
int main(int argc,char**argv)try{
 if(argc!=3)return 2;const auto source=Scene::load(argv[1]);const std::filesystem::path out=argv[2];std::filesystem::create_directories(out);setenv("XDG_STATE_HOME",(out/"state").c_str(),1);setenv("XDG_CONFIG_HOME",(out/"config").c_str(),1);
 const auto binding=source.active().material_bindings;check(source.encode().at("version")==36&&binding&&binding->native_paint_frames&&binding->native_paint_sampling&&binding->normal_frames&&binding->key_derivation,"Actual public Create/Replace fixture owns separate native paint, current key frame and key derivation");binding->native_paint_frames->validate_binding(*binding,true);
 unsigned number=0;for(bool posed:{false,true})for(float weight:{0.f,.37f,1.f}){
  auto scene=source;RigEdit edit;edit.linked_pose=false;edit.morph=MorphWeightEdit{0,weight};if(posed)edit.joint=JointAdjustment{1,{0,0,25},{}};edit_rig(scene,scene.active().id,edit);const auto exact=scene.encode();Document doc;doc.replace(scene);check(doc.wait_asset(std::chrono::seconds(120))&&doc.wait_mesh(std::chrono::seconds(120))&&doc.frame().state->error.empty(),"Actual native current key/pose worker settles before export");
  auto dir=out/std::to_string(number++);std::filesystem::create_directories(dir);const auto file=dir/"key.glb";const std::string prior="prior current native key export";{std::ofstream f(file);f<<prior;}std::string reason;try{doc.export_asset(file,AssetExportFormat::Glb).get();}catch(const std::exception&e){reason=e.what();}std::cout<<"ORIGINAL_NATIVE36_EXPORT posed="<<posed<<" weight="<<weight<<" reason="<<reason<<std::endl;
  check(reason.find("Paint sampling export requires its own supported native colour/normal attachment")!=std::string::npos,"Actual current native36 key export exposes the missing frame/paint-domain adapter");const auto current=bytes(file);check(std::string(current.begin(),current.end())==prior&&doc.copy_scene().encode()==exact,"Original real key export refusal preserves exact old file and complete Scene");doc.save_async(dir/"following.dsculpt").get();check(Scene::load(dir/"following.dsculpt").encode()==exact,"Following native Save succeeds exactly after current key export refusal");
 }
 std::cout<<"PASS "<<checks<<" actual native36 key export counter checks"<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
