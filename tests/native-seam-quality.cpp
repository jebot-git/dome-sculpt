// SPDX-License-Identifier: GPL-3.0-or-later
// Separate AFTER-SOLVE seam oracle. Original artist weights select expected
// continuity pairs only here, never in the producer or its energy.
#include "cgltf.h"
#include "dome/scene.hpp"
#include "dome/humanoid-creator.hpp"
#include <fstream>
#include <iostream>
#include <map>
#include <bit>
#include <nlohmann/json.hpp>
using namespace dome;
int main(int argc,char**argv)try{
 if(argc!=6)throw std::runtime_error("Choose completed field, original artist GLB, global poses, head-eye poses and report");
 auto scene=Scene::load(argv[1]);auto before=scene.encode();
 cgltf_options options{};cgltf_data*raw=nullptr;if(cgltf_parse_file(&options,argv[2],&raw)!=cgltf_result_success)throw std::runtime_error("Original independent parse");
 std::unique_ptr<cgltf_data,decltype(&cgltf_free)>owner(raw,cgltf_free);
 if(cgltf_load_buffers(&options,raw,argv[2])!=cgltf_result_success||cgltf_validate(raw)!=cgltf_result_success)throw std::runtime_error("Original independent buffers");
 struct Vertex{uint64_t object;uint32_t vertex;};
 using Key=std::tuple<uint64_t,int,int,std::array<float,3>,std::array<float,3>,std::vector<uint64_t>,std::vector<uint32_t>>;
 std::map<Key,std::vector<Vertex>>groups;
 for(const auto&o:scene.objects())if(o.mesh_active&&o.painted&&o.appearance&&o.deformation&&o.deformation->skin){
  const auto&a=*o.appearance;const auto&m=o.painted->mesh();if(a.source_node<0||size_t(a.source_node)>=raw->nodes_count||a.source_primitive<0||a.source_vertices.size()!=m.positions.size())throw std::runtime_error("Current original correspondence");
  auto&node=raw->nodes[a.source_node];if(!node.mesh||!node.skin||size_t(a.source_primitive)>=node.mesh->primitives_count)throw std::runtime_error("Actual original mesh/skin");const auto&primitive=node.mesh->primitives[a.source_primitive];
  std::map<std::array<uint32_t,2>,size_t>edges;for(size_t c=0;c<m.indices.size();c+=3)for(size_t k=0;k<3;++k){auto x=m.indices[c+k],y=m.indices[c+(k+1)%3];++edges[{std::min(x,y),std::max(x,y)}];}
  std::vector<bool>boundary(m.positions.size());for(const auto&[e,count]:edges)if(count==1)boundary[e[0]]=boundary[e[1]]=true;
  auto world=scene.world_matrix(o.id);
  for(uint32_t v=0;v<m.positions.size();++v)if(boundary[v]){
   std::vector<uint64_t>weights;auto original=a.source_vertices[v];
   for(int set=0;set<8;++set){auto ja=cgltf_find_accessor(&primitive,cgltf_attribute_type_joints,set),wa=cgltf_find_accessor(&primitive,cgltf_attribute_type_weights,set);if(!ja&&!wa)break;if(!ja||!wa)throw std::runtime_error("Original weight-set pairing");unsigned joints[4]{};float values[4]{};if(!cgltf_accessor_read_uint(ja,original,joints,4)||!cgltf_accessor_read_float(wa,original,values,4))throw std::runtime_error("Original independent row");for(size_t k=0;k<4;++k)if(values[k]){if(joints[k]>=node.skin->joints_count)throw std::runtime_error("Original joint outside skin");weights.push_back((uint64_t(joints[k])<<32)|std::bit_cast<uint32_t>(values[k]));}}
   std::sort(weights.begin(),weights.end());if(weights.empty())throw std::runtime_error("Original weighted row missing");
   float original_position[3]{};if(!cgltf_accessor_read_float(cgltf_find_accessor(&primitive,cgltf_attribute_type_position,0),original,original_position,3))throw std::runtime_error("Original rest position");
   std::vector<uint32_t>morph;
   if(o.deformation->morphs){if(a.source_morph_targets.size()!=o.deformation->morphs->targets.size())throw std::runtime_error("Current morph identities");for(size_t t=0;t<a.source_morph_targets.size();++t){auto p=o.deformation->morphs->targets[t].delta[v];if(p.x==0&&p.y==0&&p.z==0)continue;morph.insert(morph.end(),{a.source_morph_targets[t],std::bit_cast<uint32_t>(p.x==0?0.f:p.x),std::bit_cast<uint32_t>(p.y==0?0.f:p.y),std::bit_cast<uint32_t>(p.z==0?0.f:p.z)});}}
   auto p=m.positions[v];auto q=world.point({p.x,p.y,p.z});std::array<float,3>current{float(q[0]),float(q[1]),float(q[2])};std::array<float,3>original_point{original_position[0],original_position[1],original_position[2]};for(auto&f:current)if(f==0)f=0;for(auto&f:original_point)if(f==0)f=0;
   groups[{o.group,a.source_node,a.source_mesh,current,original_point,std::move(weights),std::move(morph)}].push_back({o.id,v});
  }
 }
 std::vector<std::vector<Vertex>>seams;size_t pairs=0;for(auto&[key,g]:groups)if(g.size()>1){(void)key;pairs+=g.size()*(g.size()-1)/2;seams.push_back(std::move(g));}
 if(!pairs)throw std::runtime_error("No authentic expected-continuity pairs");
 nlohmann::json records=nlohmann::json::array();size_t failed=0;double maximum=0;
 for(int family=0;family<2;++family)for(int pose=0;pose<(family?14:5);++pose){
  const auto path=std::filesystem::path(argv[family?4:3])/("pose-"+std::to_string(pose)+".dsculpt");auto posed=Scene::load(path);std::map<uint64_t,std::vector<std::array<double,3>>>points;
  for(const auto&o:posed.objects())if(o.mesh_active&&o.painted&&o.deformation&&o.deformation->skin){auto mesh=o.deformation->preview(o.painted->mesh());auto world=posed.world_matrix(o.id);for(auto p:mesh.positions)points[o.id].push_back(world.point({p.x,p.y,p.z}));}
  double worst=0;uint64_t oa=0,ob=0;uint32_t va=0,vb=0;size_t bad=0;
  for(const auto&g:seams)for(size_t a=0;a<g.size();++a)for(size_t b=a+1;b<g.size();++b){auto x=points.at(g[a].object).at(g[a].vertex),y=points.at(g[b].object).at(g[b].vertex);double d=std::hypot(x[0]-y[0],x[1]-y[1],x[2]-y[2]);if(d>worst){worst=d;oa=g[a].object;ob=g[b].object;va=g[a].vertex;vb=g[b].vertex;}bad+=d>3e-5;}
  failed+=bad>0;maximum=std::max(maximum,worst);records.push_back({{"family",family?"head-eye":"global"},{"pose",pose},{"pairs",pairs},{"failed_pairs",bad},{"maximum_m",worst},{"worst_objects",std::array{oa,ob}},{"worst_vertices",std::array{va,vb}}});
 }
 if(scene.encode()!=before)throw std::runtime_error("Read-only field mutated");
 std::ofstream(argv[5])<<nlohmann::json{{"artist_values_after_solve_only",true},{"seam_threshold_m",3e-5},{"groups",seams.size()},{"pairs",pairs},{"poses",records.size()},{"failed_poses",failed},{"maximum_m",maximum},{"records",records}}.dump(2);
 std::cout<<"RESULT continuity pairs="<<pairs<<" failedPoses="<<failed<<" max="<<maximum<<'\n';return failed?1:0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
