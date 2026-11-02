// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/rig-rest.hpp"
#include "rig-rest-transport.hpp"
#include <set>
#include <nlohmann/json.hpp>
namespace dome {
namespace {
void stop(const std::function<bool()> &cancelled){if(cancelled&&cancelled())throw std::runtime_error("Rest-rig editing cancelled");}
AffineTransform affine(const RigMatrix &m){return AffineTransform(m);}
float vector_length(AffineVector v){return float(std::sqrt(v[0]*v[0]+v[1]*v[1]+v[2]*v[2]));}
void matrix_close(const AffineTransform &a,const AffineTransform &b,float error,const char *message){for(size_t i=0;i<16;++i)if(std::abs(a.matrix()[i]-b.matrix()[i])>error)throw std::runtime_error(message);}
}
void RigRestOptions::validate()const{
 if(linked_objects.size()>256)throw std::length_error("Linked rig selection exceeds 256 objects");
 if(!std::isfinite(maximum_world_error)||maximum_world_error<=0||maximum_world_error>.01f)throw std::invalid_argument("Rest-rig world precision must be positive and at most 10 mm");
}
RigRestTransport transport_rig_rest(const Skeleton &source,std::span<const JointTransform> pose,const AffineTransform &world,std::span<const RigRestJointEdit> edits,RigRestOptions options,const std::function<bool()> &cancelled){
 stop(cancelled);options.validate();source.validate();if(!pose.empty())source.local_matrices(pose);
 const auto rest=source.world(),old_local=source.local_matrices();
 RigRestTransport result{source,std::vector<JointTransform>(pose.begin(),pose.end()),false};
 if(edits.empty())return result;if(edits.size()>source.bones.size())throw std::length_error("Rest joint edit count exceeded");
 auto desired=rest;std::set<uint32_t> seen;const auto inverse_world=world.inverse();
 for(const auto &edit:edits){stop(cancelled);if(edit.joint>=source.bones.size()||!seen.insert(edit.joint).second)throw std::invalid_argument("Invalid or repeated rest joint identity");
  if(edit.parent){if(*edit.parent< -1||*edit.parent>=int(source.bones.size()))throw std::invalid_argument("Invalid rest parent identity");result.skeleton.bones[edit.joint].parent=*edit.parent;}
  auto desired_world=world*affine(desired[edit.joint]);auto matrix=desired_world.matrix();
  if(edit.world_rotation_delta){auto q=*edit.world_rotation_delta;float norm=q.w*q.w+q.x*q.x+q.y*q.y+q.z*q.z;if(!std::isfinite(norm)||std::abs(norm-1)>1e-4f)throw std::invalid_argument("Rest joint rotation must be a unit quaternion");AffineTransform rotation(q.matrix());for(size_t column=0;column<3;++column){auto v=rotation.direction({matrix[column*4],matrix[column*4+1],matrix[column*4+2]});for(size_t row=0;row<3;++row)matrix[column*4+row]=v[row];}}
  if(edit.world_position){if(!finite(*edit.world_position))throw std::invalid_argument("Rest joint head must be finite");matrix[12]=edit.world_position->x;matrix[13]=edit.world_position->y;matrix[14]=edit.world_position->z;}
  if(edit.world_position||edit.world_rotation_delta)desired[edit.joint]=(inverse_world*AffineTransform(matrix)).float_matrix();
  if(edit.world_length){if(!std::isfinite(*edit.world_length)||*edit.world_length<=0||*edit.world_length>10000)throw std::invalid_argument("Rest joint length must be in (0,10000] world metres");const auto scale=vector_length((world*affine(desired[edit.joint])).direction({0,1,0}));if(!(scale>0))throw std::invalid_argument("Rest bone has a singular world axis");result.skeleton.bones[edit.joint].length=*edit.world_length/scale;}
 }
 // Validate the proposed graph before traversing it. Heads of unedited children
 // retain their original global positions and complete world linear basis.
 result.skeleton.validate();const AffineTransform space=source.space_matrix?affine(*source.space_matrix):AffineTransform{};
 for(size_t j=0;j<source.bones.size();++j){stop(cancelled);const auto parent=result.skeleton.bones[j].parent;const bool same_parent=parent==source.bones[j].parent;const bool parent_same=parent<0||desired[size_t(parent)]==rest[size_t(parent)];
  RigMatrix local=old_local[j];if(!same_parent||desired[j]!=rest[j]||!parent_same)local=((parent<0?space:affine(desired[size_t(parent)])).inverse()*affine(desired[j])).float_matrix();
  if(local!=old_local[j])result.skeleton.bones[j].rest_matrix=local;
 }
 const auto actual=result.skeleton.world();
 for(size_t j=0;j<rest.size();++j){stop(cancelled);matrix_close(world*affine(actual[j]),world*affine(desired[j]),options.maximum_world_error,"New rest hierarchy exceeds the explicit world reconstruction bound");}
 return detail::finish_rig_rest_transport(source,pose,std::move(result.skeleton),world,options,cancelled);
}
namespace detail {
RigRestTransport finish_rig_rest_transport(const Skeleton &source,std::span<const JointTransform> pose,Skeleton proposal,const AffineTransform &world,RigRestOptions options,const std::function<bool()> &cancelled){
 stop(cancelled);options.validate();source.validate();proposal.validate();
 if(source.bones.size()!=proposal.bones.size()||source.space_matrix!=proposal.space_matrix)throw std::invalid_argument("Rest completion requires exact aligned bone/bind-space identities");
 for(size_t j=0;j<source.bones.size();++j)if(source.bones[j].name!=proposal.bones[j].name)throw std::invalid_argument("Rest completion cannot infer a bone correspondence from replacement names");
 proposal.inverse_bind_override=source.inverse_bind_override;
 const auto rest=source.world(),current=source.world(pose),old_pose_local=source.local_matrices(pose),binds=source.inverse_bind();
 const AffineTransform space=source.space_matrix?affine(*source.space_matrix):AffineTransform{};
 RigRestTransport result{std::move(proposal),std::vector<JointTransform>(pose.begin(),pose.end()),false};
 const auto after_rest=result.skeleton.world();
 bool changed_rest=false;for(size_t j=0;j<rest.size();++j){stop(cancelled);changed_rest|=after_rest[j]!=rest[j];}
 if(changed_rest){result.skeleton.inverse_bind_override=binds;for(size_t j=0;j<rest.size();++j){stop(cancelled);if(after_rest[j]!=rest[j])(*result.skeleton.inverse_bind_override)[j]=(affine(after_rest[j]).inverse()*affine(rest[j])*affine(binds[j])).float_matrix();}}
 if(!pose.empty()){
  std::vector<RigMatrix> wanted;wanted.reserve(rest.size());for(size_t j=0;j<rest.size();++j){stop(cancelled);wanted.push_back(after_rest[j]==rest[j]?current[j]:current[j]==rest[j]?after_rest[j]:(affine(current[j])*affine(rest[j]).inverse()*affine(after_rest[j])).float_matrix());}
  const auto candidate=result.skeleton.local_matrices(result.pose);
  for(size_t j=0;j<rest.size();++j){stop(cancelled);const auto parent=result.skeleton.bones[j].parent;const bool same_parent=parent==source.bones[j].parent;const bool parent_same=parent<0||wanted[size_t(parent)]==current[size_t(parent)];
   RigMatrix local=old_pose_local[j];if(!same_parent||wanted[j]!=current[j]||!parent_same)local=((parent<0?space:affine(wanted[size_t(parent)])).inverse()*affine(wanted[j])).float_matrix();
   if(local!=candidate[j])result.pose[j].full_local_matrix=local;
  }
 }
 const auto before_skin=source.skin_matrices(pose),after_skin=result.skeleton.skin_matrices(result.pose);
 for(size_t j=0;j<rest.size();++j){stop(cancelled);matrix_close(world*affine(before_skin[j]),world*affine(after_skin[j]),options.maximum_world_error,"Rest fitting cannot preserve the current skin product within the explicit world bound");}
 result.skeleton.validate();result.skeleton.world(result.pose);
 result.changed=result.skeleton.encode()!=source.encode();
 if(!result.changed&&result.pose.size()==pose.size())for(size_t j=0;j<pose.size();++j)if(result.pose[j].full_local_matrix!=pose[j].full_local_matrix){result.changed=true;break;}
 stop(cancelled);return result;
}
} // namespace detail
} // namespace dome
