// SPDX-License-Identifier: GPL-3.0-or-later
#include "current-color-target-fixture.hpp"
#include "dome/document.hpp"
#include <iostream>
int main(int argc,char**argv)try{
 if(argc!=3)return 2;
 if(std::string_view(argv[1])=="--inspect"){
  auto scene=dome::Scene::load(argv[2]);auto value=scene.encode();auto isolated=nlohmann::json::array();
  for(const auto &object:scene.objects())isolated.push_back(current_color_bake_fixture::source_only(scene.snapshot(),object.id));
  value["isolated_objects"]=std::move(isolated);std::cout<<value.dump()<<'\n';return 0;
 }
 if(std::string_view(argv[1])!="--ui-fixture")return 2;
 auto source=current_color_target_fixture::fixture(true,256);
 auto &other=source.object(source.objects().back().id);auto mesh=other.painted->mesh();for(size_t i=0;i<mesh.positions.size();++i){auto p=mesh.positions[i];mesh.normals[i]=dome::normalized(mesh.normals[i]+dome::Vec3{.19f*p.y,.23f*p.x,.11f*p.z});}other.painted=std::make_shared<dome::PaintedMesh>(mesh,256);
 dome::Document document;document.replace(source);dome::ObjectBakeSettings settings;settings.prepare_colors=true;settings.ao_samples=0;document.bake_from_object(other.id,settings);
 if(!document.wait_asset()||!document.wait_mesh()||!document.frame().state->error.empty())throw std::runtime_error("GUI fixture Bake: "+document.frame().state->error);
 document.save(argv[2]);return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
