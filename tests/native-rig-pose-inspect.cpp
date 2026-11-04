// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include "dome/rig-edit.hpp"
#include "dome/creator-archive.hpp"
#include "humanoid-sha256.hpp"
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
std::string hash(const nlohmann::json&j){const auto bytes=j.dump();return creator_detail::sha256(std::span(reinterpret_cast<const uint8_t*>(bytes.data()),bytes.size()));}
int main(int argc,char**argv)try{
 if(argc<3)return 2;auto s=Scene::load(argv[2]);std::vector<uint64_t>ids;for(const auto&o:s.objects())if(o.appearance&&o.deformation&&o.deformation->skin&&o.deformation->humanoid)ids.push_back(o.id);if(ids.size()!=19)throw std::invalid_argument("Authentic complete19 needed");
 if(std::string_view(argv[1])=="--prepare"){if(argc!=4)return 2;s.select(ids.front());s.save(argv[3]);return 0;}
 if(std::string_view(argv[1])=="--expected"){if(argc!=4)return 2;RigEdit e;e.linked_pose=false;e.joint=JointAdjustment{s.object(ids.front()).deformation->humanoid->joints.at("head"),{0,0,std::stof(argv[3])},{}};for(auto id:ids)edit_rig(s,id,e);}
 auto j=s.encode();auto value=nlohmann::json::array();for(auto&o:j.at("objects")){if(o.contains("deformation")&&!o.at("deformation").is_null()){auto&d=o.at("deformation");value.push_back({{"id",o.at("id")},{"pose",hash(d.at("pose"))}});d["pose"]=nlohmann::json::array();}}
 std::cout<<nlohmann::json{{"scene",hash(s.encode())},{"without_pose",hash(j)},{"target_ids",ids},{"head",s.object(ids.front()).deformation->humanoid->joints.at("head")},{"poses",value}}.dump()<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}
