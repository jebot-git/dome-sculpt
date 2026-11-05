// SPDX-License-Identifier: GPL-3.0-or-later
// Read-only field diagnostic over genuinely unskinned repository fixtures.
// The saved ordinary-bind endpoint supplies only its exact target-local rig,
// pose/IBM/capsule transport; no skin values are passed into the candidate.
#include "visibility-heat-bind.hpp"
#include "humanoid-heat-sources.hpp"
#include "native-head-geometry.hpp"
#include "native-anatomical-domain.hpp"
#include "dome/polygon-scene.hpp"
#include "dome/scene.hpp"
#include "dome/humanoid-creator.hpp"
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
using namespace dome;
int main(int argc,char**argv)try{
 if(argc!=4||std::string(argv[3])!="source-head")return 2;const std::filesystem::path input=argv[1],out=argv[2];std::filesystem::create_directories(out);auto scene=Scene::load(input);const auto before=scene.encode();std::vector<std::pair<uint64_t,SkinWeights>>weights;nlohmann::json records=nlohmann::json::array();size_t total=0;
 std::vector<uint64_t>ids;uint64_t reference=0;
 for(const auto&o:scene.objects())if(o.mesh_active&&o.painted&&o.deformation&&o.deformation->skeleton&&o.deformation->skin&&o.rig_source){ids.push_back(o.id);if(o.rig_source->deformation&&o.rig_source->deformation->skeleton){if(reference)throw std::runtime_error("Ambiguous original fitted reference capsule");reference=o.id;}}
 if(ids.size()!=19||!reference)throw std::runtime_error("Genuine complete fitted/unskinned source instance required");
 std::function<bool()>stop;detail::weight_surface::Budget whole{0,500'000'000,stop};
 auto domain=detail::native_anatomical_domain(scene,ids,whole);if(domain.objects.size()!=19)throw std::runtime_error("Complete actual current source domain changed");
 auto anatomy=detail::analyze_native_head_geometry(scene,reference,std::min(size_t(100'000'000),whole.maximum-whole.used),128u*1024u*1024u,stop,domain.objects);whole.step(anatomy.work);total=whole.used;
 size_t output_bytes=65536;for(auto id:ids){const auto n=scene.object(id).painted->mesh().positions.size();const size_t bytes=n*(sizeof(VertexWeights)+4*sizeof(Influence)+64);if(bytes>256u*1024u*1024u-output_bytes)throw std::length_error("All staged source-head fields exceed workspace");output_bytes+=bytes;}
 for(const auto&o:scene.objects())if(o.mesh_active&&o.painted&&o.deformation&&o.deformation->skeleton&&o.deformation->skin&&o.rig_source){
  BindOptions options;options.maximum_work=whole.maximum-whole.used;std::cout<<"CANDIDATE object="<<o.id<<" vertices="<<o.painted->mesh().positions.size()<<std::endl;
  try{if(!o.deformation->humanoid)throw std::runtime_error("Actual current humanoid authority required");auto found=std::find_if(anatomy.parts.begin(),anatomy.parts.end(),[&](auto&p){return p.object==o.id;});if(found==anatomy.parts.end())throw std::runtime_error("Captured source head domain omitted actual part");detail::VisibilityGeodesicResult result;
   if(found->head_region){const auto head=o.deformation->humanoid->joints.at("head");o.deformation->humanoid->validate(*o.deformation->skeleton,false);if(!o.deformation->skeleton->bones.at(head).deform)throw std::runtime_error("Cannot impose nondeform head boundary");const auto n=o.painted->mesh().positions.size();whole.step_items(n,16);result.weights.vertices.assign(n,VertexWeights{{head,1}});result.weights.validate(n,o.deformation->skeleton->bones.size());result.workspace=output_bytes;}
   else{detail::weight_surface::Budget capture{0,whole.maximum-whole.used,stop};auto profile=detail::humanoid_heat_sources(*o.deformation->skeleton,*o.deformation->humanoid,capture);options.maximum_work-=capture.used;result=detail::visibility_heat_bind_object(o,*o.deformation->skeleton,options,{},256u*1024u*1024u-output_bytes,profile);result.work+=capture.used;whole.step(result.work);}
   total=whole.used;records.push_back({{"object",o.id},{"source_head_region",found->head_region},{"work",result.work},{"workspace",result.workspace},{"visible_seeds",result.visible_seeds},{"occluded_candidates",result.occluded_candidates},{"heat_iterations",result.heat_iterations},{"residual",result.heat_residual},{"partition_error",result.partition_error},{"negative_stiffness_edges",result.negative_stiffness_edges}});weights.emplace_back(o.id,std::move(result.weights));}
  catch(const std::exception&e){std::ofstream(out/"first-refusal.json")<<nlohmann::json{{"object",o.id},{"name",o.name},{"reason",e.what()},{"completed",records},{"work_of_completed_parts",total},{"input_exact",scene.encode()==before}}.dump(2);throw;}
 }
 if(weights.size()!=19||scene.encode()!=before)throw std::runtime_error("Diagnostic must stage all nineteen unchanged inputs before adoption");
 for(auto&[id,skin]:weights){const auto&object=scene.object(id);auto d=*object.deformation;d.skin=std::move(skin);scene.set_deformation(id,std::move(d));}
 scene.save(out/"bound.dsculpt");std::ofstream(out/"report.json")<<nlohmann::json{{"parts",weights.size()},{"per_target_work_limit",500'000'000},{"sum_work",total},{"source_head_analysis_work",anatomy.work},{"source_head_analysis_bytes",anatomy.bytes},{"reference",reference},{"staged_field_forecast",output_bytes},{"records",records},{"no_artist_weight_values_input",true},{"seed_policy","explicit exact-source head Dirichlet regions + role handles + nearest intrinsic surface heat"},{"transport_prerequisite","actual original ordinary bind target-local rig/pose/IBM and retained capsules; not a new public worker mode"}}.dump(2);std::cout<<"PASS private visibility candidate all19 fields staged and saved"<<std::endl;return 0;
}catch(const std::exception&e){std::cerr<<"FAIL "<<e.what()<<std::endl;return 1;}
