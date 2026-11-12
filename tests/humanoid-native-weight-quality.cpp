// SPDX-License-Identifier: GPL-3.0-or-later
// Real retained-source authoring; no artist influence payload enters solvers.
#include "humanoid-weight-reference.hpp"
#define main established_anatomical_source_fixture
#include "anatomical-fixture.cpp"
#undef main
#include "dome/skin-authoring.hpp"
#include <chrono>
namespace {
struct PrimitiveIdentity {size_t offset=0,vertices=0;int mesh=-1;};
std::map<std::pair<int,int>,PrimitiveIdentity> primitive_identity(const std::filesystem::path &path){
 cgltf_options options{};cgltf_data *raw=nullptr;if(cgltf_parse_file(&options,path.c_str(),&raw)!=cgltf_result_success)throw std::runtime_error("Source identity parse failed");std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data(raw,cgltf_free);
 std::map<std::pair<int,int>,PrimitiveIdentity> result;size_t offset=0;
 for(size_t n=0;n<raw->nodes_count;++n)if(auto *mesh=raw->nodes[n].mesh)for(size_t p=0;p<mesh->primitives_count;++p){const auto *position=cgltf_find_accessor(&mesh->primitives[p],cgltf_attribute_type_position,0);if(!position)throw std::runtime_error("Source identity POSITION missing");result[{int(n),int(p)}]={offset,position->count,int(mesh-raw->meshes)};offset+=position->count;}
 return result;
}
struct Agreement {size_t vertices=0,dominant_matches=0;double l1=0,shared_mass=0;};
Json agreement_json(const Agreement &m){return {{"vertices",m.vertices},{"mean_l1",m.vertices?m.l1/m.vertices:0},{"mean_reference_mass_retained",m.vertices?m.shared_mass/m.vertices:0},{"dominant_match_fraction",m.vertices?double(m.dominant_matches)/m.vertices:0}};}
Json compare_native(const Scene &scene,std::span<const uint64_t> objects,const HumanoidMapping &mapping,const std::filesystem::path &original){
 const auto reference=artist_rows(original,mapping);const auto identity=primitive_identity(original);std::map<size_t,std::string> roles;for(auto &[role,joint]:mapping.joints)roles.emplace(joint,role);
 Agreement whole;std::map<std::string,Agreement> by_role;size_t excluded=0,total=0;double unmapped=0;
 for(auto id:objects){const auto &o=scene.object(id);if(!o.appearance||!o.deformation||!o.deformation->skin||!o.deformation->skeleton)throw std::runtime_error("Missing actual accepted native source skin");const auto &a=*o.appearance;const auto primitive=identity.at({a.source_node,a.source_primitive});if(a.source_mesh!=primitive.mesh||a.source_vertices.size()!=o.painted->mesh().positions.size())throw std::runtime_error("Native/source primitive identity differs");
  const auto &mesh=o.painted->mesh();const auto &skin=*o.deformation->skin;skin.validate(mesh.positions.size(),o.deformation->skeleton->bones.size());std::set<uint32_t> used(mesh.indices.begin(),mesh.indices.end());
  for(auto v:used){if(a.source_vertices.at(v)>=primitive.vertices)throw std::runtime_error("Native source vertex outside exact primitive");const auto &row=reference.at(primitive.offset+a.source_vertices[v]);++total;unmapped+=row.unmapped;if(row.unmapped>1e-5){++excluded;continue;}auto dominant=std::max_element(row.weights.begin(),row.weights.end(),[](auto &x,auto &y){return x.second<y.second;});if(dominant==row.weights.end())throw std::runtime_error("Empty mapped artist row");
   std::map<size_t,double> predicted;for(auto influence:skin.vertices.at(v))predicted[influence.joint]+=influence.weight;const auto selected=std::max_element(predicted.begin(),predicted.end(),[](auto &x,auto &y){return x.second<y.second;});double l1=0,shared=0;for(auto [joint,value]:row.weights){l1+=std::abs(value-predicted[joint]);shared+=std::min(value,predicted[joint]);}for(auto [joint,value]:predicted)if(!row.weights.contains(joint))l1+=value;
   for(auto *m:{&whole,&by_role[roles.at(dominant->first)]}){++m->vertices;m->l1+=l1;m->shared_mass+=shared;m->dominant_matches+=selected!=predicted.end()&&selected->first==dominant->first;}
  }
 }
 if(!whole.vertices)throw std::runtime_error("No native source vertices matched exact artist role identities");Json breakdown=Json::object();for(auto &[role,m]:by_role)breakdown[role]=agreement_json(m);
 return {{"referenced_native_vertices",total},{"unmapped_reference_vertices_excluded",excluded},{"mean_unmapped_mass_all_referenced",unmapped/total},{"mapped_only",agreement_json(whole)},{"by_reference_dominant_humanoid_role",breakdown}};
}
Json native_quality(const std::filesystem::path &input,const std::filesystem::path &out){
 std::filesystem::create_directories(out);const auto metadata=static_source(input,out/"unskinned.glb");ImportLimits limits;limits.mode=ImportMode::PreserveGltf;const auto raw=import_mesh(out/"unskinned.glb",limits);require(raw.skins.empty(),"No artist source skin may enter solver");for(auto &p:raw.primitives)require(p.influences.empty(),"No artist influence values may enter solver");
 ImportAuthoringOptions choices;choices.paint_resolution=64;auto imported=prepare_authoring_import(Scene{},out/"unskinned.glb",choices);auto &base=imported.scene;uint64_t reference=0;size_t largest=0;for(auto id:imported.objects)if(base.object(id).painted->mesh().positions.size()>largest){reference=id;largest=base.object(id).painted->mesh().positions.size();}require(reference!=0,"Native reference required");
 HumanLandmarks landmarks;landmarks.left={-1,0,0};landmarks.front={0,0,-1};const auto inverse=base.world_matrix(reference).inverse();for(const auto &entry:metadata.at("extensions").at("VRM").at("humanoid").at("humanBones")){auto role=entry.at("bone").get<std::string>();if(role.ends_with("ThumbProximal"))role.replace(role.size()-8,8,"Metacarpal");else if(role.ends_with("ThumbIntermediate"))role.replace(role.size()-12,12,"Proximal");const auto node=entry.at("node").get<size_t>();require(node<raw.nodes.size(),"Actual original role node required");const auto &m=raw.nodes[node].world;const auto p=inverse.point({m[12],m[13],m[14]});landmarks.points.emplace(role,Vec3{float(p[0]),float(p[1]),float(p[2])});}
 RigFitOptions fit;fit.bind=false;author_humanoid_fit(base,reference,landmarks,fit);const auto mapping=*base.object(reference).deformation->humanoid;const auto before=base.encode();Json methods=Json::array();
 for(auto [mode,label]:{std::pair{SkinAuthoringMode::Automatic,"legacy_automatic"},std::pair{SkinAuthoringMode::SurfaceHeat,"surface_heat"},std::pair{SkinAuthoringMode::Anatomical,"anatomical"}}){
  Scene scene=base;SkinAuthoringOptions options;options.mode=mode;options.reference=reference;const auto start=std::chrono::steady_clock::now();Json method{{"method",label}};
  try{if(mode==SkinAuthoringMode::Automatic){for(auto id:imported.objects)author_skin(scene,id,options);}else if(mode==SkinAuthoringMode::SurfaceHeat)author_surface_heat_skin(scene,imported.objects,options);else author_anatomical_skin(scene,imported.objects,options);method["agreement"]=compare_native(scene,imported.objects,mapping,input);method["status"]="measured";}
  catch(const std::exception &e){method["status"]="refused";method["error"]=e.what();}
  require(base.encode()==before,"Quality runs must preserve exact unweighted native source input");method["seconds"]=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();methods.push_back(std::move(method));std::ofstream(out/"progress.json")<<methods.dump(2)<<'\n';
 }
 return {{"asset",input.string()},{"parts",imported.objects.size()},{"reference",reference},{"bones",base.object(reference).deformation->skeleton->bones.size()},{"artist_weights_in_solver_source",false},{"methods",methods}};
}
}
int main(int argc,char **argv)try{
 if(argc!=3)throw std::invalid_argument("Usage: humanoid-native-weight-quality ASSET_DIRECTORY OUTPUT_DIRECTORY");const std::filesystem::path directory=argv[2];std::filesystem::create_directories(directory);Json result{{"scope","Descriptive native source workflow quality measurement; exact source node/primitive/accessor correspondence, artist influence payload physically removed before fitting and all solver methods. Refusals retained; no universal visual-quality threshold."},{"assets",Json::array()}};
 for(auto *sex:{"female","male"}){result["assets"].push_back(native_quality(std::filesystem::path(argv[1])/"vroid"/(std::string("base-")+sex+".vrm"),directory/sex));std::ofstream(directory/"quality.json")<<result.dump(2)<<'\n';}
 std::cout<<result.dump(2)<<'\n';return 0;
}catch(const std::exception &e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
