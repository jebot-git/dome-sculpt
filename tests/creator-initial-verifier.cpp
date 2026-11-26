// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/creation.hpp"
#include "dome/scene.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <iostream>
using namespace dome;
namespace {
size_t checks=0;
void check(bool yes,const char*why){if(!yes)throw std::runtime_error(why);++checks;std::cout<<"PASS "<<why<<'\n';}
Scene baseline(){
 Scene scene;Volume field(.03f);field.begin_stroke();Brush brush;brush.radius=.4f;brush.blend=0;field.stamp(brush);field.end_stroke();
 scene.active().volume=std::make_shared<Volume>(std::move(field));return scene;
}
HumanoidControls selected(){auto controls=humanoid_preset(HumanoidPreset::Anime,"vroid-beta-female");controls.height=1.9f;controls.build=.4f;controls.hair_recolor=humanoid_hair_recolor_value(controls);controls.hair_recolor->at(0)=.7f;return controls;}
void append(Scene&scene,const HumanoidControls&controls){CanvasTransform placement;placement.position.y=-controls.height*.5f;append_humanoid(scene,controls,"Humanoid",placement,256);}
void verify(std::string_view mode,const std::filesystem::path&file){
 check(mode=="accept"||mode=="cancel"||mode=="focus","Known initial creator public route");
 const auto actual=Scene::load(file);auto expected=baseline();
 if(mode=="accept"){
  check(actual.groups().size()==1&&bool(actual.groups().back().humanoid),"Initial public Apply creates one native avatar group");
  const auto&persisted=*actual.groups().back().humanoid;
  check(std::abs(persisted.height-1.9f)<1e-5f&&std::abs(persisted.build-.4f)<1e-5f&&persisted.hair_recolor&&std::abs(persisted.hair_recolor->at(0)-.7f)<1e-5f,"Original physical controller slider gates reach selected height/build/hair colour");
  auto controls=selected();controls.height=persisted.height;controls.build=persisted.build;controls.hair_recolor->at(0)=persisted.hair_recolor->at(0);
  check(controls.encode()==persisted.encode(),"Every other actual preset/body/face/hair/source recipe field matches the independent selected draft");append(expected,controls);
 }
 check(nlohmann::json::to_cbor(actual.encode())==nlohmann::json::to_cbor(expected.encode()),"Complete initial native bytes equal the independent seed/selected-avatar result without source or placement loss");
}
void self_test(const std::filesystem::path&root){
 std::filesystem::create_directories(root);
 for(const char*mode:{"accept","cancel","focus"}){auto scene=baseline();if(std::string_view(mode)=="accept")append(scene,selected());const auto path=root/(std::string(mode)+".dsculpt");scene.save(path);verify(mode,path);}
 auto altered=baseline();append(altered,selected());altered.active().material.color[0]=.123f;const auto path=root/"altered-material.dsculpt";altered.save(path);bool refused=false;try{verify("accept",path);}catch(const std::exception&){refused=true;}check(refused,"Independent verifier refuses altered native material bytes");
 altered=baseline();const auto initial=altered.active().id;append(altered,selected());altered.erase(initial);altered.save(root/"missing-initial.dsculpt");refused=false;try{verify("accept",root/"missing-initial.dsculpt");}catch(const std::exception&){refused=true;}check(refused,"Independent verifier refuses discarded pristine seed geometry");
}
}
int main(int argc,char**argv)try{
 std::cout<<std::unitbuf;
 if(argc==4&&std::string_view(argv[1])=="--verify-initial")verify(argv[2],argv[3]);
 else if(argc==3&&std::string_view(argv[1])=="--self-test")self_test(argv[2]);
 else throw std::runtime_error("usage: creator-initial-verifier --verify-initial MODE PROJECT | --self-test OUTPUT");
 std::cout<<"INITIAL VERIFIER CHECKS "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
