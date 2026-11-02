// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
int main(int argc,char **argv)try{
 if(argc!=3)return 2;
 if(std::string_view(argv[1])=="--inspect"){std::cout<<Scene::load(argv[2]).encode().dump()<<'\n';return 0;}
 if(std::string_view(argv[1])!="--fixture")return 2;
 Mesh m;m.positions={{-1,-1,0},{1,-1,0},{1,1,0},{-1,1,0}};m.normals.assign(4,{0,0,1});m.uv={{0,0},{1,0},{1,1},{0,1}};m.indices={0,1,2,0,2,3};
 Scene s;auto setup=[&](uint64_t id,ob::Vec3 position,std::array<float,4> color){auto &o=s.object(id);o.mesh_active=true;o.painted=std::make_shared<PaintedMesh>(m,64);o.transform.position=position;o.material.color=color;};
 setup(1,{10,4,0},{.08f,.55f,.95f,1});auto second=s.add("Other visible",Volume{});setup(second,{-20,4,0},{.95f,.28f,.05f,1});auto hidden=s.add("Hidden distant",Volume{});setup(hidden,{50,50,0},{1,0,0,1});s.object(hidden).visible=false;s.select(1);s.save(argv[2]);return 0;
}catch(const std::exception &e){std::cerr<<e.what()<<'\n';return 1;}
