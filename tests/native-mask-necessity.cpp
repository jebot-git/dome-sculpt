// SPDX-License-Identifier: GPL-3.0-or-later
// Necessary-condition control, not a solved field. It writes the unique
// admissible weights ONLY on rows whose declared mask has exactly one joint.
// Other rows retain the prior native result. Original artist values are absent.
#include "geometry-weight-authoring.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
using namespace dome;
int main(int argc,char**argv)try{
 if(argc!=4)throw std::runtime_error("Choose fitted unskinned input, prior native field and output directory");
 auto input=Scene::load(argv[1]);auto output=Scene::load(argv[2]);const auto before=input.encode();
 std::filesystem::path out=argv[3];std::filesystem::create_directories(out);
 uint64_t ref=0;for(const auto&o:input.objects())if(o.deformation&&o.deformation->skeleton)ref=o.id;
 const auto&r=input.object(ref);const auto&rig=*r.deformation->skeleton;
 const std::function<bool()> never=[]{return false;};detail::weight_surface::Budget work{0,500'000'000,never};
 auto profile=detail::declared_weight_profile(rig,work,detail::DeclaredRigGeometry::NativeBoneAxes);
 auto shift=AffineTransform{}.matrix();auto world=input.world_matrix(ref);for(size_t c=0;c<3;++c)shift[12+c]=-world.matrix()[12+c];const AffineTransform origin(shift);const auto metric=origin*world;
 for(auto&s:profile.segments){auto f=[&](std::array<double,3>a){auto p=detail::skin_affine_point(metric,{float(a[0]),float(a[1]),float(a[2])});return std::array<double,3>{p.x,p.y,p.z};};s.head=f(s.head);s.tail=f(s.tail);}
 nlohmann::json report;report["necessary_condition_only_not_a_solved_field"]=true;report["parts"]=nlohmann::json::array();
 for(const auto&o:input.objects())if(o.mesh_active&&o.painted&&o.appearance){
  if(!o.appearance->asset||!o.appearance->asset->skins.empty())throw std::runtime_error("Not genuinely unskinned");
  for(const auto&p:o.appearance->asset->primitives)if(!p.influences.empty())throw std::runtime_error("Artist influences forbidden");
  detail::SkinGeometryWork gw(500'000'000,never);auto mesh=detail::skin_world_mesh(o.painted->mesh(),origin*input.world_matrix(o.id),gw);
  detail::HeatBindingOptions options;auto intent=detail::declared_weight_intent(o,{o.id,{}},profile,options,work,detail::DeclaredSupportPolicy::UniqueSourceWitnesses,mesh,r.deformation);auto masks=intent.masks(o);
  auto d=*output.object(o.id).deformation;auto skin=*d.skin;size_t forced=0;std::map<uint32_t,size_t> joints;
  for(size_t i=0;i<masks.size();++i){size_t count=0;uint32_t joint=0;for(size_t j=0;j<rig.bones.size();++j)if(masks[i][j/64]&(uint64_t(1)<<(j%64))){++count;joint=j;}
   if(count==1){skin.vertices[i]={{joint,1.f}};++forced;++joints[joint];}}
  d.skin=std::move(skin);output.object(o.id).deformation=std::make_shared<const DeformationState>(std::move(d));
  auto fields=nlohmann::json::array();for(auto[j,n]:joints)fields.push_back({{"joint",j},{"role_diagnostic_only",rig.bones[j].name},{"rows",n}});
  report["parts"].push_back({{"object",o.id},{"rows",masks.size()},{"forced_rows",forced},{"every_row_uniquely_forced",forced==masks.size()},{"forced_joint_counts",fields}});
 }
 if(input.encode()!=before)throw std::runtime_error("Readonly input mutated");
 output.save(out/"necessary-condition-only.dsculpt");report["work"]=work.used;std::ofstream(out/"report.json")<<report.dump(2);
 std::cout<<"PASS readonly singleton-mask necessary condition, not a solved field\n";
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}
