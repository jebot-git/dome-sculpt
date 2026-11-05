// SPDX-License-Identifier: GPL-3.0-or-later
// Read-only field diagnostic over genuinely unskinned repository fixtures.
// The saved ordinary-bind endpoint supplies only its exact target-local rig,
// pose/IBM/capsule transport; no skin values are passed into the candidate.
#include "visibility-heat-bind.hpp"
#include "dome/scene.hpp"
#include "dome/humanoid-creator.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
int main(int argc,char**argv)try{
 if(argc!=3)return 2;const std::filesystem::path input=argv[1],out=argv[2];std::filesystem::create_directories(out);auto scene=Scene::load(input);const auto before=scene.encode();std::vector<std::pair<uint64_t,SkinWeights>>weights;nlohmann::json records=nlohmann::json::array();size_t total=0;
 for(const auto&o:scene.objects())if(o.mesh_active&&o.painted&&o.deformation&&o.deformation->skeleton&&o.deformation->skin&&o.rig_source){
  BindOptions options;options.maximum_work=500'000'000;std::cout<<"CANDIDATE object="<<o.id<<" vertices="<<o.painted->mesh().positions.size()<<std::endl;
  try{auto result=detail::visibility_heat_bind_object(o,*o.deformation->skeleton,options,{},256u*1024u*1024u);total+=result.work;records.push_back({{"object",o.id},{"work",result.work},{"workspace",result.workspace},{"visible_seeds",result.visible_seeds},{"occluded_candidates",result.occluded_candidates},{"heat_iterations",result.heat_iterations},{"residual",result.heat_residual},{"partition_error",result.partition_error},{"negative_stiffness_edges",result.negative_stiffness_edges}});weights.emplace_back(o.id,std::move(result.weights));}
  catch(const std::exception&e){std::ofstream(out/"first-refusal.json")<<nlohmann::json{{"object",o.id},{"name",o.name},{"reason",e.what()},{"completed",records},{"work_of_completed_parts",total},{"input_exact",scene.encode()==before}}.dump(2);throw;}
 }
 if(weights.size()!=19||scene.encode()!=before)throw std::runtime_error("Diagnostic must stage all nineteen unchanged inputs before adoption");
 for(auto&[id,skin]:weights){const auto&object=scene.object(id);auto d=*object.deformation;d.skin=std::move(skin);scene.set_deformation(id,std::move(d));}
 scene.save(out/"bound.dsculpt");std::ofstream(out/"report.json")<<nlohmann::json{{"parts",weights.size()},{"per_target_work_limit",500'000'000},{"sum_work",total},{"records",records},{"no_artist_weight_values_input",true},{"seed_policy","exact nearest intrinsic-surface screened positive heat"},{"transport_prerequisite","actual original ordinary bind target-local rig/pose/IBM and retained capsules; not a new public worker mode"}}.dump(2);std::cout<<"PASS private visibility candidate all19 fields staged and saved"<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
