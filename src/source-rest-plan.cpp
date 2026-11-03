// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-rest-plan.hpp"
#include "import-skin.hpp"
#include <nlohmann/json.hpp>
#include <set>
namespace dome::detail {
namespace {
void stop(const std::function<bool()> &c){if(c&&c())throw std::runtime_error("Source rest planning cancelled");}
void require(bool value,const char *message){if(!value)throw std::invalid_argument(message);}
void close(const AffineTransform &a,const AffineTransform &b,double error,const char *message){for(size_t k=0;k<16;++k)if(std::abs(a.matrix()[k]-b.matrix()[k])>error)throw std::runtime_error(message);}
std::vector<RigMatrix> rest_world(const SourceRestGraph &g,const std::vector<int> &parents,std::vector<RigMatrix> &local,const std::function<bool()> &c){
 const auto &nodes=g.model->asset->nodes;std::vector<RigMatrix> world(nodes.size());std::vector<uint8_t> state(nodes.size()),changed(nodes.size());
 auto visit=[&](auto &&self,uint32_t node,size_t depth)->void{stop(c);require(depth<=256,"Source rest parent closure exceeds256 levels");if(state[node]==2)return;require(state[node]==0,"Source rest reparent would create a cycle");state[node]=1;const auto parent=parents.at(node);require(parent>=-1&&parent<int(nodes.size()),"Invalid original-node parent identity");if(parent>=0)self(self,uint32_t(parent),depth+1);
  changed[node]=parent!=nodes[node].parent||local[node]!=nodes[node].local||(parent>=0&&changed[size_t(parent)]);
  if(!changed[node])world[node]=nodes[node].world;
  else if(!g.pose_witnesses.contains(node)){world[node]=nodes[node].world;local[node]=parent<0?world[node]:(AffineTransform(world[size_t(parent)]).inverse()*AffineTransform(world[node])).float_matrix();}
  else world[node]=parent<0?local[node]:(AffineTransform(world[size_t(parent)])*AffineTransform(local[node])).float_matrix();state[node]=2;
 };
 for(size_t node=0;node<nodes.size();++node)visit(visit,uint32_t(node),1);return world;
}
std::vector<RigMatrix> pose_world(const std::vector<int> &parents,const std::vector<RigMatrix> &local,const std::vector<RigMatrix> &rest_local,const std::vector<RigMatrix> &rest_world,const std::function<bool()> &c){
 std::vector<RigMatrix> world(local.size());std::vector<uint8_t> state(local.size());auto visit=[&](auto&&self,size_t node,size_t depth)->void{stop(c);require(depth<=256,"Source pose ancestry exceeds256 levels");if(state[node]==2)return;require(state[node]==0,"Cyclic source pose graph");state[node]=1;const auto parent=parents[node];if(parent>=0)self(self,size_t(parent),depth+1);
  if(local[node]==rest_local[node]&&(parent<0||world[size_t(parent)]==rest_world[size_t(parent)]))world[node]=rest_world[node];else world[node]=parent<0?local[node]:(AffineTransform(world[size_t(parent)])*AffineTransform(local[node])).float_matrix();state[node]=2;
 };for(size_t node=0;node<local.size();++node)visit(visit,node,1);return world;
}
}
SourceRestPlan plan_source_rest(const SourceRestGraph &g,uint64_t selected,std::span<const RigRestJointEdit> edits,RigRestOptions options,const std::function<bool()> &cancelled,std::span<const SourceRestParentEdit> source_parents){
 stop(cancelled);options.validate();require(bool(g.model)&&!g.parts.empty(),"Missing captured source rig graph");require(options.linked,"Original-node REST edits require an explicit complete linked source instance");
 auto selected_part=std::find_if(g.parts.begin(),g.parts.end(),[&](const auto&p){return p.object==selected;});require(selected_part!=g.parts.end()&&selected_part->original_skin>=0,"Select an actual authored skin as the source REST reference");
 const auto &reference=*selected_part->deformation->skeleton;auto proposal=transport_rig_rest(reference,selected_part->deformation->pose,selected_part->world,edits,options,cancelled);SourceRestPlan plan;plan.parents=g.parents;plan.rest_local=g.rest_local;plan.rest_world=g.rest_world;std::set<uint32_t> changed_parents;for(const auto &edit:source_parents){require(changed_parents.insert(edit.node).second&&std::find(selected_part->joint_nodes.begin(),selected_part->joint_nodes.end(),edit.node)!=selected_part->joint_nodes.end(),"Original child is not an actual selected native joint identity");require(edit.parent>=-1&&(edit.parent<0||g.pose_witnesses.contains(uint32_t(edit.parent))),"Original parent is not an authenticated actual source joint");plan.parents.at(edit.node)=edit.parent;}if(!proposal.changed&&plan.parents==g.parents)return plan;
 // Prove shared MODEL-node pose authority; a single edited or conflicting view
 // is not selected as a replacement for another part's authored current pose.
 const auto shared_pose=source_rest_pose(g,cancelled);const auto &old_pose_local=shared_pose.local;const auto &old_pose_world=shared_pose.world;auto desired=g.rest_world;const auto proposal_world=proposal.skeleton.world();const auto mesh_to_model=AffineTransform(*reference.space_matrix).inverse();const auto model_to_world=selected_part->world*AffineTransform(*reference.space_matrix);
 std::map<uint32_t,float> lengths;
 for(const auto &edit:edits){stop(cancelled);const auto node=selected_part->joint_nodes.at(edit.joint);if(edit.parent&&changed_parents.contains(node))throw std::invalid_argument("One original child has two parent edit authorities");if(edit.parent)plan.parents[node]=*edit.parent<0?-1:int(selected_part->joint_nodes.at(size_t(*edit.parent)));if(edit.world_position||edit.world_rotation_delta)desired[node]=(mesh_to_model*AffineTransform(proposal_world.at(edit.joint))).float_matrix();if(edit.world_length)lengths[node]=proposal.skeleton.bones.at(edit.joint).length;}
 for(size_t node=0;node<desired.size();++node){stop(cancelled);const auto parent=plan.parents[node];if(parent!=g.parents[node]||desired[node]!=g.rest_world[node]||(parent>=0&&desired[size_t(parent)]!=g.rest_world[size_t(parent)]))plan.rest_local[node]=parent<0?desired[node]:(AffineTransform(desired[size_t(parent)]).inverse()*AffineTransform(desired[node])).float_matrix();}
 plan.rest_world=rest_world(g,plan.parents,plan.rest_local,cancelled);for(size_t node=0;node<desired.size();++node){stop(cancelled);close(model_to_world*AffineTransform(plan.rest_world[node]),model_to_world*AffineTransform(desired[node]),options.maximum_world_error,"Canonical source REST reconstruction exceeds the explicit WORLD bound");}
 std::vector<RigMatrix> wanted;wanted.reserve(desired.size());for(size_t node=0;node<desired.size();++node){stop(cancelled);wanted.push_back(old_pose_world[node]==g.rest_world[node]?plan.rest_world[node]:plan.rest_world[node]==g.rest_world[node]?old_pose_world[node]:(AffineTransform(old_pose_world[node])*AffineTransform(g.rest_world[node]).inverse()*AffineTransform(plan.rest_world[node])).float_matrix());}
 plan.pose_local=plan.rest_local;for(size_t node=0;node<wanted.size();++node){stop(cancelled);const auto parent=plan.parents[node];if(!g.pose_witnesses.contains(uint32_t(node)))continue;
  if(parent==g.parents[node]&&wanted[node]==old_pose_world[node]&&(parent<0||wanted[size_t(parent)]==old_pose_world[size_t(parent)]))plan.pose_local[node]=old_pose_local[node];
  else if(wanted[node]!=plan.rest_world[node]||(parent>=0&&wanted[size_t(parent)]!=plan.rest_world[size_t(parent)]))plan.pose_local[node]=parent<0?wanted[node]:(AffineTransform(wanted[size_t(parent)]).inverse()*AffineTransform(wanted[node])).float_matrix();}
 const auto actual_pose=pose_world(plan.parents,plan.pose_local,plan.rest_local,plan.rest_world,cancelled);for(size_t node=0;node<wanted.size();++node){stop(cancelled);close(model_to_world*AffineTransform(actual_pose[node]),model_to_world*AffineTransform(wanted[node]),options.maximum_world_error,"Source current-pose reconstruction exceeds its explicit WORLD bound");}
 for(const auto &part:g.parts){stop(cancelled);if(part.original_skin<0)continue;const auto &before=*part.deformation->skeleton;SourceRestSkinPlan result{part.object,before,part.deformation->pose,part.joint_nodes};std::set<uint32_t> required(result.source_nodes.begin(),result.source_nodes.end());
  auto ancestry=[&](uint32_t source,const auto &parents){size_t depth=0;for(int node=int(source);node>=0;node=parents.at(size_t(node))){stop(cancelled);require(++depth<=256,"Source native ancestor closure exceeds256 levels");required.insert(uint32_t(node));require(required.size()<=256,"Source reparent needs more than256 native ancestor bones; no joints were dropped");}};
  for(auto node:part.joint_nodes){ancestry(node,g.parents);ancestry(node,plan.parents);}for(auto node:required)if(std::find(result.source_nodes.begin(),result.source_nodes.end(),node)==result.source_nodes.end())result.source_nodes.push_back(node);
  std::map<uint32_t,uint32_t> slots;for(size_t slot=0;slot<result.source_nodes.size();++slot)slots.emplace(result.source_nodes[slot],uint32_t(slot));
  for(size_t slot=before.bones.size();slot<result.source_nodes.size();++slot){stop(cancelled);const auto source=result.source_nodes[slot];Bone bone;bone.name=imported_bone_name(g.model->asset->nodes[source],source);bone.rest=imported_node_reference(g.model->asset->nodes[source]);bone.deform=false;result.skeleton.bones.push_back(std::move(bone));}
  for(size_t slot=0;slot<result.source_nodes.size();++slot){const auto source=result.source_nodes[slot];auto &bone=result.skeleton.bones[slot];const auto parent=plan.parents[source];bone.parent=parent<0?-1:int(slots.at(uint32_t(parent)));bone.rest_matrix=plan.rest_local[source];if(auto length=lengths.find(source);length!=lengths.end())bone.length=length->second;}
  result.skeleton.inverse_bind_override->resize(result.skeleton.bones.size(),RigMatrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});
  const auto old_rest=before.world(),old_binds=before.inverse_bind(),old_skin=before.skin_matrices(part.deformation->pose);const auto next_rest=result.skeleton.world();std::vector<RigMatrix> binds;binds.reserve(result.source_nodes.size());
  for(size_t slot=0;slot<result.source_nodes.size();++slot){stop(cancelled);if(slot<before.bones.size())binds.push_back(next_rest[slot]==old_rest[slot]?old_binds[slot]:(AffineTransform(next_rest[slot]).inverse()*AffineTransform(old_rest[slot])*AffineTransform(old_binds[slot])).float_matrix());else binds.push_back(AffineTransform(next_rest[slot]).inverse().float_matrix());}
  result.skeleton.inverse_bind_override=std::move(binds);bool neutral=true;for(auto node:result.source_nodes)neutral&=plan.pose_local[node]==plan.rest_local[node];if(result.pose.empty()&&!neutral)for(const auto &bone:result.skeleton.bones)result.pose.push_back(bone.rest);else if(!result.pose.empty())for(size_t slot=result.pose.size();slot<result.skeleton.bones.size();++slot)result.pose.push_back(result.skeleton.bones[slot].rest);
  if(!result.pose.empty()){const auto initial=result.skeleton.local_matrices(result.pose);for(size_t slot=0;slot<result.source_nodes.size();++slot)if(initial[slot]!=plan.pose_local[result.source_nodes[slot]])result.pose[slot].full_local_matrix=plan.pose_local[result.source_nodes[slot]];}
  const auto current=result.skeleton.skin_matrices(result.pose);for(size_t slot=0;slot<before.bones.size();++slot){stop(cancelled);close(part.world*AffineTransform(old_skin[slot]),part.world*AffineTransform(current[slot]),options.maximum_world_error,"Per-part actual WORLD skin product could not be retained");}
  auto deformed=*part.deformation;deformed.skeleton=result.skeleton;deformed.pose=result.pose;deformed.validate(*part.mesh);plan.skins.push_back(std::move(result));
 }
 plan.changed=true;stop(cancelled);return plan;
}
} // namespace dome::detail
