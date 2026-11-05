// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
int main(int argc,char**argv)try{if(argc!=4)return 2;auto scene=Scene::load(argv[1]);auto&o=scene.object(std::stoull(argv[2]));auto&d=*o.deformation;auto segments=d.skeleton->segments();nlohmann::json bones=nlohmann::json::array(),rows=nlohmann::json::array();for(size_t b=0;b<segments.size();++b){double sum=0;for(auto&r:d.skin->vertices)for(auto i:r)if(i.joint==b)sum+=i.weight;auto p=segments[b][0],q=segments[b][1];bones.push_back({{"joint",b},{"name",d.skeleton->bones[b].name},{"head",{p.x,p.y,p.z}},{"tail",{q.x,q.y,q.z}},{"weight_mass",sum}});}for(size_t v=0;v<o.painted->mesh().positions.size();++v){auto p=o.painted->mesh().positions[v];nlohmann::json influences=nlohmann::json::array();for(auto i:d.skin->vertices[v])influences.push_back({{"joint",i.joint},{"weight",i.weight}});rows.push_back({{"vertex",v},{"position",{p.x,p.y,p.z}},{"weights",influences}});}std::ofstream(argv[3])<<nlohmann::json{{"object",o.id},{"name",o.name},{"bones",bones},{"rows",rows}}.dump(2);return 0;}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}
