// SPDX-License-Identifier: GPL-3.0-or-later
// Read-only field diagnostic over genuinely unskinned repository fixtures.
// The saved ordinary-bind endpoint supplies only its exact target-local rig,
// pose/IBM/capsule transport; no skin values are passed into the candidate.
#include "visibility-heat-bind.hpp"
#include "humanoid-heat-sources.hpp"
#include "native-head-geometry.hpp"
#include "humanoid-heat-domains.hpp"
#include "native-anatomical-domain.hpp"
#include "dome/polygon-scene.hpp"
#include "dome/scene.hpp"
#include "dome/humanoid-creator.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
int main(int argc,char**argv)try{
 if(argc!=4||std::string(argv[3])!="source-domains")return 2;const std::filesystem::path input=argv[1],out=argv[2];std::filesystem::create_directories(out);auto scene=Scene::load(input);const auto before=scene.encode();std::vector<std::pair<uint64_t,SkinWeights>>weights;nlohmann::json records=nlohmann::json::array();size_t total=0;
 std::vector<uint64_t>ids;uint64_t reference=0;
 for(const auto&o:scene.objects())if(o.mesh_active&&o.painted&&o.deformation&&o.deformation->skeleton&&o.deformation->skin&&o.rig_source){ids.push_back(o.id);if(o.rig_source->deformation&&o.rig_source->deformation->skeleton){if(reference)throw std::runtime_error("Ambiguous original fitted reference capsule");reference=o.id;}}
 if(ids.size()!=19||!reference)throw std::runtime_error("Genuine complete fitted/unskinned source instance required");
 std::function<bool()>stop;detail::weight_surface::Budget whole{0,500'000'000,stop};
 auto anatomy=detail::source_humanoid_heat_domains(scene,ids,reference,whole,128u*1024u*1024u);total=whole.used;
 size_t output_bytes=65536;for(auto id:ids){const auto n=scene.object(id).painted->mesh().positions.size();const size_t bytes=n*(sizeof(VertexWeights)+4*sizeof(Influence)+64);if(bytes>256u*1024u*1024u-output_bytes)throw std::length_error("All staged source-head fields exceed workspace");output_bytes+=bytes;}
 for(const auto&o:scene.objects())if(o.mesh_active&&o.painted&&o.deformation&&o.deformation->skeleton&&o.deformation->skin&&o.rig_source){
  BindOptions options;options.maximum_work=whole.maximum-whole.used;std::cout<<"CANDIDATE object="<<o.id<<" vertices="<<o.painted->mesh().positions.size()<<std::endl;
  try{if(!o.deformation->humanoid)throw std::runtime_error("Actual current humanoid authority required");auto found=std::find_if(anatomy.parts.begin(),anatomy.parts.end(),[&](auto&p){return p.object==o.id;});if(found==anatomy.parts.end())throw std::runtime_error("Captured source head domain omitted actual part");detail::VisibilityGeodesicResult result;
   if(!found->fixed.empty()){if(found->mesh!=o.painted->mesh_owner()||found->fixed.size()!=o.painted->mesh().positions.size())throw std::runtime_error("Anatomical boundary mesh owner changed");const auto n=found->fixed.size();whole.step_items(n,16);result.weights.vertices.reserve(n);for(auto joint:found->fixed){if(joint>=o.deformation->skeleton->bones.size()||!o.deformation->skeleton->bones[joint].deform)throw std::runtime_error("Anatomical boundary joint is absent/nondeform");result.weights.vertices.push_back({{joint,1}});}result.weights.validate(n,o.deformation->skeleton->bones.size());result.workspace=output_bytes;}
   else{detail::weight_surface::Budget capture{0,whole.maximum-whole.used,stop};auto profile=detail::humanoid_heat_sources(*o.deformation->skeleton,*o.deformation->humanoid,capture);options.maximum_work-=capture.used;result=detail::visibility_heat_bind_object(o,*o.deformation->skeleton,options,{},256u*1024u*1024u-output_bytes,profile);result.work+=capture.used;whole.step(result.work);}
   total=whole.used;records.push_back({{"object",o.id},{"source_head_region",found->head},{"source_ocular_region",found->ocular},{"work",result.work},{"workspace",result.workspace},{"visible_seeds",result.visible_seeds},{"occluded_candidates",result.occluded_candidates},{"heat_iterations",result.heat_iterations},{"residual",result.heat_residual},{"partition_error",result.partition_error},{"negative_stiffness_edges",result.negative_stiffness_edges}});weights.emplace_back(o.id,std::move(result.weights));}
  catch(const std::exception&e){std::ofstream(out/"first-refusal.json")<<nlohmann::json{{"object",o.id},{"name",o.name},{"reason",e.what()},{"completed",records},{"work_of_completed_parts",total},{"input_exact",scene.encode()==before}}.dump(2);throw;}
 }
 if(weights.size()!=19||scene.encode()!=before)throw std::runtime_error("Diagnostic must stage all nineteen unchanged inputs before adoption");
 for(auto&[id,skin]:weights){const auto&object=scene.object(id);auto d=*object.deformation;d.skin=std::move(skin);scene.set_deformation(id,std::move(d));}
 scene.save(out/"bound.dsculpt");std::ofstream(out/"report.json")<<nlohmann::json{{"parts",weights.size()},{"per_target_work_limit",500'000'000},{"sum_work",total},{"source_head_analysis_work",anatomy.analysis_work},{"source_head_analysis_bytes",anatomy.analysis_bytes},{"reference",reference},{"staged_field_forecast",output_bytes},{"records",records},{"no_artist_weight_values_input",true},{"seed_policy","explicit exact-source head/convex ocular Dirichlet regions + role handles + nearest intrinsic surface heat"},{"transport_prerequisite","actual original ordinary bind target-local rig/pose/IBM and retained capsules; not a new public worker mode"}}.dump(2);std::cout<<"PASS private visibility candidate all19 fields staged and saved"<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
