// SPDX-License-Identifier: GPL-3.0-or-later
// Read-only geometric counterfactual. No source influence values or artist BIN.
#include "bone-shadow-query.hpp"
#include "skin-affine.hpp"
#include "weight-surface-system.hpp"
#include "dome/scene.hpp"
#include "dome/humanoid-creator.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
int main(int argc,char**argv)try{
 if(argc!=3)throw std::runtime_error("Choose genuine fitted unskinned Scene and report");
 auto scene=Scene::load(argv[1]);uint64_t reference=0;
 for(const auto&o:scene.objects())if(o.deformation&&o.deformation->skeleton)reference=o.id;
 const auto&r=scene.object(reference);if(!r.deformation->humanoid)throw std::runtime_error("Actual current roles required");
 std::vector<detail::bone_shadow::Point>points;std::vector<uint32_t>indices,owners;std::vector<uint64_t>parts;
 const std::function<bool()>cancel=[] {return false;};detail::weight_surface::Budget work{0,500'000'000,cancel};
 for(const auto&o:scene.objects())if(o.mesh_active&&o.painted&&o.appearance){
  if(!o.appearance->asset||!o.appearance->asset->skins.empty())throw std::runtime_error("Not genuinely unskinned");
  for(const auto&p:o.appearance->asset->primitives)if(!p.influences.empty())throw std::runtime_error("Artist weights forbidden");
  auto w=scene.world_matrix(o.id);const auto&m=o.painted->mesh();uint32_t offset=points.size(),owner=parts.size();parts.push_back(o.id);
  for(auto p:m.positions){work.step(16);auto q=w.point({p.x,p.y,p.z});points.push_back({float(q[0]),float(q[1]),float(q[2])});owners.push_back(owner);}
  for(auto i:m.indices){work.step();indices.push_back(offset+i);}
 }
 detail::BoneShadowQuery query(points,indices,256u*1024u*1024u,work);
 detail::BoneShadowQuery::Scratch scratch(query);detail::BoneQueryReport report;auto segs=r.deformation->skeleton->segments();
 nlohmann::json result;result["source_weights_absent"]=true;result["policy"]="Existing strict WholeGroup point visibility; ambiguity recorded, never skipped or accepted";result["eyes"]=nlohmann::json::array();
 for(auto role:{"leftEye","rightEye"}){
  auto joint=r.deformation->humanoid->joints.at(role);auto p=segs.at(joint)[0];auto q=scene.world_matrix(reference).point({p.x,p.y,p.z});detail::bone_shadow::Point pivot{float(q[0]),float(q[1]),float(q[2])};
  auto rows=nlohmann::json::array();for(auto id:parts)rows.push_back({{"object",id},{"visible",0},{"blocked",0},{"ambiguous",0}});
  for(uint32_t row=0;row<points.size();++row){auto&item=rows[owners[row]];try{
   auto v=query.integrate(row,pivot,pivot,1,0,1,scratch,work,report,detail::BoneWallDomain::WholeGroup,true,true,detail::BoneOriginPolicy::StrictOwnership);
   auto key=v>0?"visible":"blocked";item[key]=item[key].get<size_t>()+1;
  }catch(const detail::BoneQueryAmbiguity&e){item["ambiguous"]=item["ambiguous"].get<size_t>()+1;auto key="kind"+std::to_string(int(e.kind));item[key]=item.value(key,size_t(0))+1;}}
  result["eyes"].push_back({{"role",role},{"pivot",pivot},{"parts",rows}});
 }
 result["work"]=work.used;result["bytes"]=query.planned_bytes;std::ofstream(argv[2])<<result.dump(2);std::cout<<"PASS read-only strict wall audit work="<<work.used<<'\n';
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
