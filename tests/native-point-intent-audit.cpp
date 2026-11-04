// SPDX-License-Identifier: GPL-3.0-or-later
// Read-only geometry/control audit, no artist values or material-name policy.
#include "geometry-weight-authoring.hpp"
#include "weight-boundary-sewing.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
using namespace dome;
int main(int argc,char**argv)try{
 if(argc!=3)throw std::runtime_error("Choose genuine unskinned current input and diagnostic JSON");auto scene=Scene::load(argv[1]);auto before=scene.encode();uint64_t reference=0;for(const auto&o:scene.objects())if(o.deformation&&o.deformation->skeleton)reference=o.id;
 const auto&r=scene.object(reference);const auto&rig=*r.deformation->skeleton;std::function<bool()>no=[] {return false;};detail::weight_surface::Budget work{0,500000000,no};auto profile=detail::declared_weight_profile(rig,work,detail::DeclaredRigGeometry::NativeBoneAxes);auto world=scene.world_matrix(reference);auto s=AffineTransform{}.matrix();for(size_t k=0;k<3;++k)s[12+k]=-world.matrix()[12+k];const AffineTransform origin(s);
 for(auto&segment:profile.segments){auto f=[&](std::array<double,3> p){auto q=detail::skin_affine_point(origin*world,{float(p[0]),float(p[1]),float(p[2])});return std::array<double,3>{q.x,q.y,q.z};};segment.head=f(segment.head);segment.tail=f(segment.tail);}
 nlohmann::json result;result["parts"]=nlohmann::json::array();
 for(const auto&o:scene.objects())if(o.mesh_active&&o.painted&&o.appearance){
  const auto&m=o.painted->mesh();std::vector<std::vector<Vec3>>deltas;if(o.deformation&&o.deformation->morphs)for(const auto&t:o.deformation->morphs->targets)deltas.push_back(t.delta);
  auto sewing=detail::geometric_boundary_sewing(m,deltas,128u*1024u*1024u,work);detail::SkinGeometryWork gw(work.maximum-work.used,no);auto left=gw.remaining();auto metric=detail::skin_world_mesh(m,origin*scene.world_matrix(o.id),gw);work.step(left-gw.remaining());
  detail::DeclaredWeightPart part{o.id,{}};part.geometric_row_groups=sewing.groups;detail::HeatBindingOptions options;auto intent=detail::declared_weight_intent(o,part,profile,options,work,detail::DeclaredSupportPolicy::UniqueSourceWitnesses,metric,r.deformation);auto masks=intent.masks(o);size_t local=0;auto components=detail::heat_intent_components(m,sewing.groups,local,work.maximum-work.used,no);work.step(local);std::map<uint32_t,std::vector<uint32_t>>rows;for(uint32_t i=0;i<components.size();++i)rows[components[i]].push_back(i);
  auto cs=nlohmann::json::array();for(const auto&[root,vertices]:rows){std::vector<uint32_t>joints;for(uint32_t j=0;j<rig.bones.size();++j)if(masks[root][j/64]&(uint64_t(1)<<(j%64)))joints.push_back(j);auto roles=nlohmann::json::array();for(const auto&[role,joint]:r.deformation->humanoid->joints)if(std::find(joints.begin(),joints.end(),joint)!=joints.end())roles.push_back(role);std::array<double,3>lo{INFINITY,INFINITY,INFINITY},hi{-INFINITY,-INFINITY,-INFINITY};for(auto v:vertices){auto p=metric.positions[v];double xyz[]{p.x,p.y,p.z};for(size_t k=0;k<3;++k){lo[k]=std::min(lo[k],xyz[k]);hi[k]=std::max(hi[k],xyz[k]);}}
   cs.push_back({{"root",root},{"count",vertices.size()},{"joints",joints},{"standard_roles",roles},{"lo",lo},{"hi",hi}});
  }
  result["parts"].push_back({{"object",o.id},{"source_node",o.appearance->source_node},{"source_mesh",o.appearance->source_mesh},{"source_primitive",o.appearance->source_primitive},{"exact_edge_pairs",sewing.pairs},{"ambiguous_edge_groups",sewing.ambiguous},{"components",cs}});
 }
 if(scene.encode()!=before)throw std::runtime_error("Readonly source changed");result["work"]=work.used;result["artist_values_read"]=false;std::ofstream(argv[2])<<result.dump(2);std::cout<<"SAVED actual geometry/control audit work="<<work.used<<'\n';
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
