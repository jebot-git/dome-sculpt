// SPDX-License-Identifier: GPL-3.0-or-later
#include "skin-joint-transfer.hpp"
#include "skin-affine.hpp"
#include "source-rigid-pose.hpp"
#include "source-skin-link.hpp"

namespace dome::detail {
namespace {
struct JointIdentity {
  std::shared_ptr<const SourceNodePlacement> placement;
  std::span<const uint32_t> nodes;
};

JointIdentity joint_identity(const Scene &scene,const SculptObject &object) {
  const auto &shape=*object.deformation;
  if(!shape.source_skin_joints.empty())
    return {source_skin_link(scene.group_path(object.group),shape),shape.source_skin_joints};
  if(auto placement=source_rig_instance(scene,object))
    return {std::move(placement),object.appearance->source_joint_nodes};
  return {};
}

uint32_t mapped_ancestor(const Skeleton &rig,std::span<const uint32_t> mapping,uint32_t joint,
                         SkinGeometryWork &work) {
  for(int parent=rig.bones[joint].parent;parent>=0;parent=rig.bones[size_t(parent)].parent){
    work.visit();
    const auto mapped=mapping[size_t(parent)];
    if(mapped!=UINT32_MAX)return mapped;
  }
  return UINT32_MAX;
}
} // namespace

std::vector<uint32_t> transfer_joint_map(const Scene &scene,const SculptObject &source,
    const SculptObject &target,SkinGeometryWork &work) {
  const auto &donor=*source.deformation;
  const auto &receiver=*target.deformation;
  const auto &from=*donor.skeleton;
  const auto &to=*receiver.skeleton;
  std::vector<uint32_t> forward(from.bones.size(),UINT32_MAX),reverse(to.bones.size(),UINT32_MAX);
  const auto assign=[&](uint32_t a,uint32_t b){
    if((forward[a]!=UINT32_MAX&&forward[a]!=b)||(reverse[b]!=UINT32_MAX&&reverse[b]!=a))
      throw std::invalid_argument("Conflicting joint identities for weight transfer at "+from.bones[a].name);
    forward[a]=b;
    reverse[b]=a;
  };

  const auto donor_identity=joint_identity(scene,source),receiver_identity=joint_identity(scene,target);
  const bool shared_original=donor_identity.placement&&receiver_identity.placement&&
      same_source_skin_instance(*donor_identity.placement,*receiver_identity.placement);
  if(shared_original){
    if(donor_identity.nodes.size()!=from.bones.size()||receiver_identity.nodes.size()!=to.bones.size())
      throw std::invalid_argument("Weight transfer lost an original joint table");
    for(uint32_t a=0;a<from.bones.size();++a)
      for(uint32_t b=0;b<to.bones.size();++b){
        work.visit();
        if(donor_identity.nodes[a]==receiver_identity.nodes[b])assign(a,b);
      }
  }else{
    // An explicit humanoid role is stronger evidence than a file's joint
    // spelling. Other joints may use an exact name when it agrees with the
    // semantic map. Equal node numbers from unrelated files carry no meaning.
    std::vector<std::string_view> donor_roles(from.bones.size()),receiver_roles(to.bones.size());
    if(receiver.humanoid)for(const auto &[role,b]:receiver.humanoid->joints){
      work.visit();
      receiver_roles[b]=role;
    }
    if(donor.humanoid)for(const auto &[role,a]:donor.humanoid->joints){
      work.visit();
      donor_roles[a]=role;
      if(receiver.humanoid)
        if(auto match=receiver.humanoid->joints.find(role);match!=receiver.humanoid->joints.end())
          assign(a,match->second);
    }
    for(uint32_t a=0;a<from.bones.size();++a)
      for(uint32_t b=0;b<to.bones.size();++b){
        work.visit();
        if(from.bones[a].name!=to.bones[b].name)continue;
        if(!donor_roles[a].empty()&&!receiver_roles[b].empty()&&donor_roles[a]!=receiver_roles[b])
          throw std::invalid_argument("Conflicting humanoid roles for weight transfer at "+from.bones[a].name);
        assign(a,b);
      }
  }

  for(uint32_t a=0;a<from.bones.size();++a){
    work.visit();
    const auto b=forward[a];
    if(b==UINT32_MAX)continue;
    const auto donor_parent=mapped_ancestor(from,forward,a,work);
    const auto receiver_parent=mapped_ancestor(to,reverse,b,work);
    if(donor_parent!=(receiver_parent==UINT32_MAX?UINT32_MAX:forward[receiver_parent]))
      throw std::invalid_argument("Weight-transfer joint hierarchy differs at "+from.bones[a].name);
  }
  return forward;
}
} // namespace dome::detail
