// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/import-authoring.hpp"
#include "dome/polygon-scene.hpp"
#include "vrm-rigid-fixtures.hpp"
#include <iostream>
#include <unistd.h>
int main(int argc,char **argv)try{
 if(argc!=3)return 2;const std::string command=argv[1];
 if(command=="--inspect"){auto scene=dome::Scene::load(argv[2]);std::cout<<nlohmann::json{{"scene",scene.encode()},{"rig_source",bool(scene.active().rig_source)}}.dump();return 0;}
 if(command!="--ui-fixture")return 2;const auto home=std::filesystem::temp_directory_path()/("dome-source-rest-ui-"+std::to_string(getpid()));std::filesystem::create_directories(home);struct Cleanup{std::filesystem::path p;~Cleanup(){std::error_code ec;std::filesystem::remove_all(p,ec);}}cleanup{home};
 const auto original=std::filesystem::path(__FILE__).parent_path().parent_path()/"assets/humanoid/vroid/base-female.vrm";const auto variant=home/"placed.vrm",file=home/"source.vrm";vrm_source_fixture::source_variant(original,variant,2);vrm_source_fixture::with_rigid_accessories(variant,file);dome::ImportAuthoringOptions options;options.paint_resolution=64;auto imported=dome::prepare_authoring_import(dome::Scene{},file,options);imported.scene.select(imported.objects.front());imported.scene.save(argv[2]);return 0;
}catch(const std::exception &error){std::cerr<<error.what()<<'\n';return 1;}
