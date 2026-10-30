// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/humanoid-creator.hpp"
#include "dome/creation.hpp"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
namespace {
int checks=0;
void check(bool value,const char *text){if(!value)throw std::runtime_error(text);++checks;std::cout<<"PASS "<<text<<'\n';}
template<class F> bool rejects(F f){try{f();return false;}catch(const std::exception&){return true;}}
}
int main(int argc,char **argv)try{
 if(argc==3 && std::string(argv[1])=="--ui-fixture"){Scene scene;auto legacy=std::make_shared<const HumanoidControls>(humanoid_baseline_defaults("makehuman-hm08"));scene.active().group=scene.add_group("Saved legacy controls",{},0,legacy);scene.save(argv[2]);return 0;}
 if(argc==3 && std::string(argv[1])=="--inspect"){auto scene=Scene::load(argv[2]);auto groups=nlohmann::json::array();for(const auto &g:scene.groups())if(g.humanoid)groups.push_back({{"id",g.id},{"baseline",g.humanoid->baseline}});std::cout<<nlohmann::json{{"groups",groups},{"objects",scene.objects().size()}}.dump()<<'\n';return 0;}

 check(HumanoidControls{}.baseline=="vroid-beta-female","The default native humanoid is the verified CC0 VRoid female");
 for(const char *id:{"vroid-beta-female","vroid-beta-male"}){
  auto controls=humanoid_baseline_defaults(id);check(HumanoidControls::decode(controls.encode()).encode()==controls.encode(),"CC0 female/male controls retain exact versioned parameter identity");
  auto model=create_humanoid(controls);check(model.parts.size()==19 && model.parts[0].appearance,"Both offered baselines use all nineteen original CC0 VRoid source primitives");
  for(auto preset:{HumanoidPreset::Neutral,HumanoidPreset::Toon,HumanoidPreset::Anime}){auto styled=create_humanoid(humanoid_preset(preset,id));styled.validate();bool exact=true;for(size_t part=0;part<model.parts.size();++part)exact&=styled.parts[part].mesh.indices==model.parts[part].mesh.indices && styled.parts[part].mesh.uv==model.parts[part].mesh.uv;check(exact,"VRoid neutral/toon/anime controls preserve source topology and UV authority");}
 }
 auto legacy=humanoid_baseline_defaults("makehuman-hm08");auto old=legacy.encode();old["version"]=1;old.erase("baseline");
 check(HumanoidControls::decode(old).encode()==legacy.encode(),"Legacy schema1 MakeHuman creator metadata remains exactly decodable without bundled geometry");
 check(rejects([&]{create_humanoid(legacy);}),"Legacy MakeHuman recipes cannot recreate a removed default mesh");
 Scene retained;auto group=retained.add_group("Saved legacy recipe",{},0,std::make_shared<const HumanoidControls>(legacy));retained.active().group=group;auto before=retained.encode();
 check(rejects([&]{append_humanoid(retained,legacy);}) && retained.encode()==before,"Rejected legacy creation preserves every existing object and group exactly");
 check(Scene::decode(before).encode()==before,"Legacy MakeHuman group parameters survive native Scene roundtrip without migration or replacement");
 auto invalid=HumanoidControls{};invalid.height=100;check(rejects([&]{create_humanoid(invalid);}),"Creator height validation remains bounded");
 check(rejects([&]{create_humanoid({},[]{return true;});}),"CC0 model creation retains cancellation before publication");
 check(rejects([&]{humanoid_preset(static_cast<HumanoidPreset>(999));}),"Unknown creator presets remain rejected");
 if(const char *path=std::getenv("DOME_LEGACY_HUMANOID_SCENE")){
  auto saved=Scene::load(path);auto exact=saved.encode();size_t parts=0;for(const auto &o:saved.objects())if(o.group){++parts;check(o.painted && o.deformation && o.deformation->skeleton->bones.size()==163,"Actual pre-removal MakeHuman project retains its original mesh and 163-joint rig");}
  check(parts==5 && saved.group(saved.active().group).humanoid->baseline=="makehuman-hm08","Actual pre-removal native project retains its five original parts and creator identity");
  auto output=std::filesystem::path(path).parent_path()/"vroid-only-legacy-reopened.dsculpt";saved.save(output);check(Scene::load(output).encode()==exact,"Actual pre-removal MakeHuman project re-saves and restarts byte-equivalent semantic data");
  auto exported=std::filesystem::path(path).parent_path()/"vroid-only-legacy-reopened.glb";export_glb(saved.surfaces(),exported);check(std::filesystem::file_size(exported)>100000,"Actual pre-removal mesh/paint/rig exports without the removed generator");
 }
 std::cout<<"VRoid-only humanoid checks "<<checks<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
