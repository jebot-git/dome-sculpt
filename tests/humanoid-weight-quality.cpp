// SPDX-License-Identifier: GPL-3.0-or-later
// Read-only quality measurement. Artist weights are independent references,
// never solver inputs; unmapped source controls are reported, not guessed.
#define main established_anatomical_fit_fixture
#include "rig-fit-anatomical.cpp"
#undef main
#include <cmath>
#include <map>
namespace {
using Json=nlohmann::json;
struct ArtistRow {std::map<size_t,double> weights;double unmapped=0;};
std::vector<ArtistRow> artist_rows(const std::filesystem::path &path,const HumanoidMapping &mapping){
 cgltf_options options{};cgltf_data *raw=nullptr;
 if(cgltf_parse_file(&options,path.c_str(),&raw)!=cgltf_result_success)throw std::runtime_error("Artist parse failed");
 std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data(raw,cgltf_free);
 if(cgltf_load_buffers(&options,raw,path.c_str())!=cgltf_result_success||cgltf_validate(raw)!=cgltf_result_success)throw std::runtime_error("Artist source invalid");
 std::ifstream file(path,std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});uint32_t length=0;memcpy(&length,bytes.data()+12,4);
 auto json=Json::parse(bytes.data()+20,bytes.data()+20+length);std::map<const cgltf_node*,size_t> roles;
 for(const auto &entry:json.at("extensions").at("VRM").at("humanoid").at("humanBones")){
  std::string role=entry.at("bone");if(role.ends_with("ThumbProximal"))role.replace(role.size()-8,8,"Metacarpal");else if(role.ends_with("ThumbIntermediate"))role.replace(role.size()-12,12,"Proximal");
  auto found=mapping.joints.find(role);if(found==mapping.joints.end())continue;
  size_t node=entry.at("node");if(node>=raw->nodes_count||!roles.emplace(raw->nodes+node,found->second).second)throw std::runtime_error("Artist role identity invalid");
 }
 std::vector<ArtistRow> result;
 for(size_t node=0;node<raw->nodes_count;++node)if(auto *mesh=raw->nodes[node].mesh){
  auto *skin=raw->nodes[node].skin;
  for(size_t primitive=0;primitive<mesh->primitives_count;++primitive){auto &p=mesh->primitives[primitive];auto *position=cgltf_find_accessor(&p,cgltf_attribute_type_position,0);if(!position)throw std::runtime_error("Artist POSITION missing");
   const auto start=result.size();result.resize(start+position->count);
   for(size_t vertex=0;vertex<position->count;++vertex){auto &row=result[start+vertex];double total=0;
    for(size_t set=0;set<2;++set){auto *joints=cgltf_find_accessor(&p,cgltf_attribute_type_joints,int(set));auto *weights=cgltf_find_accessor(&p,cgltf_attribute_type_weights,int(set));
     if(!joints&&!weights)continue;if(!joints||!weights||!skin||joints->count!=position->count||weights->count!=position->count)throw std::runtime_error("Artist influence domain invalid");
     cgltf_uint indices[4]{};float values[4]{};if(!cgltf_accessor_read_uint(joints,vertex,indices,4)||!cgltf_accessor_read_float(weights,vertex,values,4))throw std::runtime_error("Artist influence read failed");
     for(size_t i=0;i<4;++i){const double value=values[i];if(!std::isfinite(value)||value<0||indices[i]>=skin->joints_count)throw std::runtime_error("Artist influence invalid");if(value==0)continue;total+=value;auto found=roles.find(skin->joints[indices[i]]);if(found==roles.end())row.unmapped+=value;else row.weights[found->second]+=value;}
    }
    if(total==0){row.unmapped=1;continue;}if(std::abs(total-1)>.005)throw std::runtime_error("Artist weights not normalized");
    // Quantized source accessor sums may differ from1. Normalize only the
    // independent reference; never pass it to automatic_bind or fit_humanoid.
    row.unmapped/=total;for(auto &[joint,value]:row.weights){(void)joint;value/=total;}
   }
  }
 }
 return result;
}
Json measure(const std::filesystem::path &path){
 auto source=read_original(path);HumanFitOptions options;options.maximum_surface_distance=.3f;auto fitted=fit_humanoid(source.mesh,source.landmarks,options);auto predicted=automatic_bind(source.mesh,fitted.skeleton);predicted.validate(source.mesh.positions.size(),fitted.skeleton.bones.size());
 const auto reference=artist_rows(path,fitted.mapping);if(reference.size()!=source.mesh.positions.size())throw std::runtime_error("Reference/current vertex order differs");
 std::map<size_t,std::string> names;for(const auto &[role,joint]:fitted.mapping.joints)names.emplace(joint,role);
 struct Metric{size_t vertices=0,dominant_matches=0;double l1=0,reference_mass_retained=0;};Metric whole;std::map<std::string,Metric> roles;size_t unmapped=0;double unmapped_mass=0;
 const std::set<uint32_t> used(source.mesh.indices.begin(),source.mesh.indices.end());
 for(auto vertex:used){const auto &row=reference[vertex];unmapped_mass+=row.unmapped;if(row.unmapped>1e-5){++unmapped;continue;}
  const auto dominant=std::max_element(row.weights.begin(),row.weights.end(),[](const auto&a,const auto&b){return a.second<b.second;});if(dominant==row.weights.end())throw std::runtime_error("Mapped reference has no positive joint");
  std::map<size_t,double> current;for(auto weight:predicted.vertices[vertex])current[weight.joint]+=weight.weight;
  auto selected=std::max_element(current.begin(),current.end(),[](const auto&a,const auto&b){return a.second<b.second;});
  double l1=0,retained=0;for(auto [joint,value]:row.weights){l1+=std::abs(value-current[joint]);retained+=std::min(value,current[joint]);}for(auto [joint,value]:current)if(!row.weights.contains(joint))l1+=value;
  for(auto *metric:{&whole,&roles[names.at(dominant->first)]}){++metric->vertices;metric->l1+=l1;metric->reference_mass_retained+=retained;metric->dominant_matches+=selected!=current.end()&&selected->first==dominant->first;}
 }
 if(!whole.vertices)throw std::runtime_error("No exact mapped artist reference vertices available");
 auto encode=[](const Metric&m){return Json{{"vertices",m.vertices},{"mean_l1",m.vertices?m.l1/m.vertices:0},{"mean_reference_mass_retained",m.vertices?m.reference_mass_retained/m.vertices:0},{"dominant_match_fraction",m.vertices?double(m.dominant_matches)/m.vertices:0}};};
 Json by_role=Json::object();for(auto &[role,metric]:roles)by_role[role]=encode(metric);
 return {{"asset",path.string()},{"all_position_vertices",source.mesh.positions.size()},{"referenced_vertices",used.size()},{"triangles",source.mesh.indices.size()/3},{"unmapped_reference_vertices_excluded",unmapped},{"mean_unmapped_mass_all_referenced",unmapped_mass/used.size()},{"mapped_only",encode(whole)},{"by_reference_dominant_humanoid_role",by_role}};
}
}
int main(int argc,char **argv)try{
 if(argc!=3)throw std::invalid_argument("Usage: humanoid-weight-quality ASSET_DIRECTORY OUTPUT.json");
 Json result{{"scope","Read-only default automatic_bind comparison to artist weights on exact mapped humanoid joints. Unmapped positive controls are excluded and reported, not guessed. This is not a universal quality acceptance threshold or Anatomical/SurfaceHeat solver qualification."},{"artist_weights_solver_inputs",false},{"assets",Json::array()}};
 for(const auto *sex:{"female","male"})result["assets"].push_back(measure(std::filesystem::path(argv[1])/"vroid"/(std::string("base-")+sex+".vrm")));
 std::ofstream output(argv[2]);output<<result.dump(2)<<'\n';if(!output)throw std::runtime_error("Quality report write failed");std::cout<<result.dump(2)<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<'\n';return 1;}
