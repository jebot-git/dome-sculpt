// SPDX-License-Identifier: GPL-3.0-or-later
#include "native-head-geometry.hpp"
#include "rotary-cap-geometry.hpp"
#include <fstream>
#include <iostream>
using namespace dome;
int main(int argc,char**argv)try{
 if(argc!=3)throw std::runtime_error("Choose fitted unskinned scene and descriptor report");
 auto scene=Scene::load(argv[1]);auto before=scene.encode();uint64_t ref=0;for(const auto&o:scene.objects())if(o.deformation&&o.deformation->skeleton)ref=o.id;
 auto head=detail::analyze_native_head_geometry(scene,ref);const auto&r=scene.object(ref);const auto&roles=r.deformation->humanoid->joints;const auto segments=r.deformation->skeleton->segments();
 detail::HeatJointMask eyes{};for(auto role:{"leftEye","rightEye"}){auto j=roles.at(role);eyes[j/64]|=uint64_t(1)<<(j%64);}
 const std::function<bool()>never=[]{return false;};detail::weight_surface::Budget work{head.work,500'000'000,never};
 nlohmann::json report;report["components"]=nlohmann::json::array();report["descriptor_only_no_field_admission"]=true;
 for(const auto&part:head.parts){
  bool ocular=!part.head_region&&part.geometric_controls!=detail::HeatJointMask{};
  for(size_t k=0;k<4;++k)ocular&=(part.geometric_controls[k]&~eyes[k])==0;
  if(!ocular)continue;
  const auto&o=scene.object(part.object);detail::SkinGeometryWork geometry(work.maximum-work.used,never);auto available=geometry.remaining();auto mesh=detail::skin_world_mesh(o.painted->mesh(),scene.world_matrix(o.id),geometry);work.step(available-geometry.remaining());
  size_t local=0;auto parents=detail::heat_intent_components(mesh,{},local,work.maximum-work.used,never);work.step(local);
  std::map<uint32_t,std::vector<uint32_t>>components;for(uint32_t i=0;i<parents.size();++i){work.step(16);components[parents[i]].push_back(i);}
  for(const auto&[root,vertices]:components){
   if(vertices.size()<10)throw std::runtime_error("Ocular component is underdetermined for a quadric");
   Mesh subset;std::vector<uint32_t>map(mesh.positions.size(),UINT32_MAX);
   for(auto i:vertices){work.step(8);map[i]=subset.positions.size();subset.positions.push_back(mesh.positions[i]);}
   for(size_t c=0;c<mesh.indices.size();c+=3){work.step(8);if(parents[mesh.indices[c]]!=root)continue;for(size_t k=0;k<3;++k)subset.indices.push_back(map[mesh.indices[c+k]]);}
   auto fit=detail::analyze_rotary_cap(subset,128u*1024u*1024u,work);
   auto distances=nlohmann::json::object();
   if(fit.ellipsoid)for(auto role:{"head","leftEye","rightEye"}){
    auto s=segments.at(roles.at(role));auto matrix=scene.world_matrix(ref);auto a=matrix.point({s[0].x,s[0].y,s[0].z});auto b=std::string_view(role)=="head"?matrix.point({s[1].x,s[1].y,s[1].z}):a;
    double rr=0,rd=0,dd=0;for(size_t k=0;k<3;++k){auto delta=b[k]-a[k];rr+=(fit.center[k]-a[k])*(fit.center[k]-a[k]);rd+=(fit.center[k]-a[k])*delta;dd+=delta*delta;}
    double t=dd?std::clamp(rd/dd,0.,1.):0,squared=0;for(size_t k=0;k<3;++k){double d=fit.center[k]-(a[k]+t*(b[k]-a[k]));squared+=d*d;}distances[role]=std::sqrt(squared);
   }
   report["components"].push_back({{"object",o.id},{"root",root},{"vertices",vertices.size()},{"disk",fit.disk},{"convex",fit.convex_boundary},{"negative_turns",fit.negative_turns},{"boundary",fit.boundary},{"positive_ellipsoid_fit",fit.ellipsoid},{"rank_ratio",fit.rank_ratio},{"singular",fit.singular},{"maximum_polynomial_residual",fit.maximum_residual},{"center",fit.center},{"radii",fit.radii},{"eigenvalues",fit.eigenvalues},{"source_distance_m",distances}});
  }
 }
 if(scene.encode()!=before)throw std::runtime_error("Readonly source changed");
 report["work"]=work.used;std::ofstream(argv[2])<<report.dump(2);std::cout<<"PASS read-only cap audit components="<<report["components"].size()<<" work="<<work.used<<'\n';
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
