// SPDX-License-Identifier: GPL-3.0-or-later
#include "visibility-heat-bind.hpp"
#include "humanoid-heat-sources.hpp"
#include <fstream>
#include <iostream>
#include <set>
#include <nlohmann/json.hpp>
using namespace dome;using namespace dome::detail;using namespace dome::detail::visibility_geodesic;
struct Captured{};
int main(int argc,char**argv)try{
 if(argc!=4)return 2;auto scene=Scene::load(argv[1]);const auto before=scene.encode();auto&o=scene.object(std::stoull(argv[2]));Mesh m;BindOptions options;options.maximum_work=500'000'000;
 try{bind_visibility_object(o,*o.deformation->skeleton,options,{},256u*1024u*1024u,[&](const Mesh&domain,const Skeleton&,BindOptions,const std::function<bool()>&,size_t)->VisibilityGeodesicResult{m=domain;throw Captured{};});}catch(const Captured&){}
 std::function<bool()>stop;weight_surface::Budget work{0,500'000'000,stop};std::vector<bone_shadow::Point>points;for(auto p:m.positions)points.push_back({p.x,p.y,p.z});auto query=std::make_shared<const ClosestRayVisibility>(points,m.indices,128u*1024u*1024u,work,true);nlohmann::json report{{"object",o.id},{"name_diagnostic_only",o.name},{"domain_vertices",m.positions.size()},{"domain_triangles",m.indices.size()/3}};
 try{ClosedMeshSourceDomain domain(query,128u*1024u*1024u,work);ClosestRayVisibility::Scratch scratch(*query);auto sources=humanoid_heat_sources(*o.deformation->skeleton,*o.deformation->humanoid,work);nlohmann::json components=nlohmann::json::array();std::set<uint32_t>seen;
 for(uint32_t row=0;row<m.positions.size();++row)if(domain.closed(row)&&seen.insert(domain.component(row)).second){nlohmann::json controls=nlohmann::json::array();for(const auto&s:sources){bone_shadow::Point a{float(s.head[0]),float(s.head[1]),float(s.head[2])},b{float(s.tail[0]),float(s.tail[1]),float(s.tail[2])};if(!domain.exterior(row,a,b,scratch,work))controls.push_back({{"joint",s.bone},{"name_diagnostic_only",o.deformation->skeleton->bones[s.bone].name},{"head",s.head},{"tail",s.tail}});}components.push_back({{"root",domain.component(row)},{"not_strictly_exterior_sources",controls}});}report["components"]=components;
 }catch(const ClosedEmbeddingFailure&e){report["embedding_failure"]={{"component",e.component},{"first",e.first},{"second",e.second},{"a",e.a},{"b",e.b},{"coincident",e.coincident},{"reason",e.what()}};}
 report["work"]=work.used;report["input_exact"]=scene.encode()==before;std::ofstream(argv[3])<<report.dump(2);std::cout<<report.dump(2)<<std::endl;return report["input_exact"]==true?0:1;
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}
