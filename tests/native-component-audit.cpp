#include "geometry-weight-authoring.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
#include <bit>
using namespace dome;
int main(int argc,char**argv)try{
 if(argc!=3)throw std::runtime_error("Choose fitted input and audit file");auto scene=Scene::load(argv[1]);uint64_t ref=0;for(const auto&o:scene.objects())if(o.deformation&&o.deformation->skeleton)ref=o.id;
 const auto&r=scene.object(ref);const auto&rig=*r.deformation->skeleton;detail::weight_surface::Budget work{0,500000000,{}};
 auto profile=detail::declared_weight_profile(rig,work,detail::DeclaredRigGeometry::NativeBoneAxes);
 auto shift=AffineTransform{}.matrix();auto world=scene.world_matrix(ref);for(size_t c=0;c<3;++c)shift[12+c]=-world.matrix()[12+c];const AffineTransform origin(shift);const auto metric=origin*world;
 for(auto&s:profile.segments){auto p=[&](std::array<double,3>a){auto q=detail::skin_affine_point(metric,{float(a[0]),float(a[1]),float(a[2])});return std::array<double,3>{q.x,q.y,q.z};};s.head=p(s.head);s.tail=p(s.tail);}
 nlohmann::json result;result["sources"]=nlohmann::json::array();for(const auto&s:profile.segments)if(rig.bones[s.bone].name=="head"||rig.bones[s.bone].name.find("Eye")!=std::string::npos)result["sources"].push_back({{"bone",s.bone},{"name",rig.bones[s.bone].name},{"length",rig.bones[s.bone].length},{"head",s.head},{"tail",s.tail}});
 result["parts"]=nlohmann::json::array();for(const auto&o:scene.objects())if(o.mesh_active&&o.painted&&o.appearance){
  const std::function<bool()>never=[] {return false;};detail::SkinGeometryWork gw(500000000,never);auto mesh=detail::skin_world_mesh(o.painted->mesh(),origin*scene.world_matrix(o.id),gw);
  detail::HeatBindingOptions options;auto intent=detail::declared_weight_intent(o,{o.id,{}},profile,options,work,detail::DeclaredSupportPolicy::UniqueSourceWitnesses,mesh,r.deformation);auto masks=intent.masks(o);size_t used=0;auto parents=detail::heat_intent_components(mesh,{},used,500000000,{});
  std::map<uint32_t,std::vector<uint32_t>>parts;for(uint32_t v=0;v<parents.size();++v)parts[parents[v]].push_back(v);
  auto comp=nlohmann::json::array();for(const auto&[root,vertices]:parts){
   const auto m=masks[vertices.front()];std::vector<std::string>names;for(size_t j=0;j<rig.bones.size();++j)if(m[j/64]&(uint64_t(1)<<(j%64)))names.push_back(rig.bones[j].name);
   bool eye=std::any_of(names.begin(),names.end(),[](const auto&s){return s.find("Eye")!=std::string::npos;});if(!eye)continue;
   std::array<float,3>lo{INFINITY,INFINITY,INFINITY},hi{-INFINITY,-INFINITY,-INFINITY};for(auto v:vertices){const auto p=mesh.positions[v];const float a[]={p.x,p.y,p.z};for(size_t c=0;c<3;++c){lo[c]=std::min(lo[c],a[c]);hi[c]=std::max(hi[c],a[c]);}}
   using Key=std::array<uint32_t,3>;std::map<Key,uint32_t>canonical;std::vector<uint32_t>ids(mesh.positions.size());for(auto v:vertices){auto p=mesh.positions[v];Key k{std::bit_cast<uint32_t>(p.x==0?0.f:p.x),std::bit_cast<uint32_t>(p.y==0?0.f:p.y),std::bit_cast<uint32_t>(p.z==0?0.f:p.z)};auto[it,inserted]=canonical.try_emplace(k,canonical.size());ids[v]=it->second;}
   std::map<std::array<uint32_t,2>,std::array<int,2>>edges;size_t faces=0;for(size_t c=0;c<mesh.indices.size();c+=3){if(parents[mesh.indices[c]]!=root)continue;++faces;for(size_t j=0;j<3;++j){auto a=ids[mesh.indices[c+j]],b=ids[mesh.indices[c+(j+1)%3]];auto&e=edges[{std::min(a,b),std::max(a,b)}];++e[0];e[1]+=a<b?1:-1;}}
   size_t boundary=0,nonmanifold=0,orientation=0;for(const auto&[e,n]:edges){boundary+=n[0]==1;nonmanifold+=n[0]!=1&&n[0]!=2;orientation+=n[0]==2&&n[1]!=0;}
   comp.push_back({{"root",root},{"vertices",vertices.size()},{"faces",faces},{"mask",names},{"lo",lo},{"hi",hi},{"position_only_boundary_edges",boundary},{"position_only_nonmanifold_edges",nonmanifold},{"position_only_orientation_errors",orientation},{"diagnostic_only_no_topology_weld",true}});
  }
  if(!comp.empty()){std::string material;const auto&a=*o.appearance;if(a.material>=0&&size_t(a.material)<a.asset->materials.size())material=a.asset->materials[a.material].name;result["parts"].push_back({{"id",o.id},{"name",o.name},{"material_diagnostic_only",material},{"all_index_components",parts.size()},{"eye_components",comp}});}
 }
 std::ofstream(argv[2])<<result.dump(2);return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<std::endl;return 1;}
