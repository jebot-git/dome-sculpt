// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene-file.hpp"
#include "dome/native-authoring.hpp"
#include "dome/humanoid-creator.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
int main(int argc,char **argv)try{
 if(argc==4&&std::string_view(argv[1])=="--fixture"){
  auto scene=dome::Scene::load(argv[2]);std::filesystem::create_directories(std::filesystem::path(argv[3]).parent_path());scene.save(argv[3]);
  if(dome::Scene::load(argv[3]).encode()!=scene.encode())throw std::runtime_error("UI fixture restart differs");return 0;
 }
 if(argc!=2)return 2;auto scene=dome::Scene::load(argv[1]);bool avatar=false;
 for(const auto &object:scene.objects())if(object.appearance){if(!object.appearance->asset||object.appearance->asset->avatar_version!=dome::ImportedAvatarVersion::Vrm1)throw std::runtime_error("UI owner lost genuine V1 identity");avatar=true;}
 if(!avatar)throw std::runtime_error("UI owner has no actual V1 source");std::cout<<scene.encode().dump()<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
