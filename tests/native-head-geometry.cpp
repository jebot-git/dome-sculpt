// SPDX-License-Identifier: GPL-3.0-or-later
#include "native-head-geometry.hpp"
#include <fstream>
#include <iostream>
using namespace dome;
int main(int argc,char**argv)try{
 if(argc!=3)throw std::runtime_error("Choose actual fitted unskinned input and geometry report");
 auto scene=Scene::load(argv[1]);const auto before=scene.encode();uint64_t ref=0;for(const auto&o:scene.objects())if(o.deformation&&o.deformation->skeleton)ref=o.id;
 auto report=detail::analyze_native_head_geometry(scene,ref,100'000'000);
 nlohmann::json j;j["work"]=report.work;j["bytes"]=report.bytes;j["ambiguous"]=report.ambiguous;j["rejected"]=report.rejected;j["not_original_topology_or_skin_authority"]=true;j["parts"]=nlohmann::json::array();j["contacts"]=nlohmann::json::array();
 const auto&rig=*scene.object(ref).deformation->skeleton;
 for(const auto&p:report.parts){std::vector<std::string>roles;for(size_t i=0;i<rig.bones.size();++i)if(p.geometric_controls[i/64]&(uint64_t(1)<<(i%64)))roles.push_back(rig.bones[i].name);j["parts"].push_back({{"object",p.object},{"primary_witness",p.primary_witness},{"candidate_head_region",p.head_region},{"components",p.components},{"geometric_controls",roles}});}
 for(const auto&e:report.contacts)j["contacts"].push_back({{"first",e.first},{"second",e.second},{"edges",e.edges}});
 if(scene.encode()!=before)throw std::runtime_error("Readonly input changed");
 std::ofstream(argv[2])<<j.dump(2);std::cout<<"PASS candidate geometry analysis work="<<report.work<<" bytes="<<report.bytes<<'\n';
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
