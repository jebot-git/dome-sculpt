// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native-anatomical-domain.hpp"
#include "geometry-weight-authoring.hpp"
namespace dome::detail {
struct NativeRigPoseScope {
  uint64_t group=0;
  std::shared_ptr<const ImportedAsset> asset;
  std::shared_ptr<const std::vector<uint8_t>> bytes;
  std::vector<uint64_t> objects;
  bool contains(uint64_t id)const{return std::find(objects.begin(),objects.end(),id)!=objects.end();}
};
// Pose membership only. This authenticates actual source instance/group and
// current native joint authority, never paint/normal/topology transfer. Original
// source skin views retain their separate source-rig mapping path. No creator
// control, method label or merely retained Origin grants shared rig authority.
inline std::optional<NativeRigPoseScope> native_rig_pose_scope(
 const Scene&scene,const SculptObject&source,weight_surface::Budget&work){
 if(!source.group||!source.appearance||!source.appearance->asset||
    !source.appearance->asset->skins.empty()||!source.appearance->source_joint_nodes.empty())return {};
 const auto&a=*source.appearance;
 auto domain=native_anatomical_domain(scene,std::span<const uint64_t>(&source.id,1),work,256);
 for(auto id:domain.objects){work.step(128);const auto&o=scene.object(id);const auto&v=*o.appearance;
  if(!v.source_joint_nodes.empty()||v.source_node<0||size_t(v.source_node)>=v.asset->nodes.size()||
     v.source_mesh<0||size_t(v.source_mesh)>=v.asset->meshes.size()||v.source_primitive<0||
     size_t(v.source_primitive)>=v.asset->meshes[size_t(v.source_mesh)].primitives.size())
    throw std::invalid_argument("Linked native source part lacks its actual node/mesh/primitive identity");
  const auto&node=v.asset->nodes[size_t(v.source_node)];
  const auto raw=v.asset->meshes[size_t(v.source_mesh)].primitives[size_t(v.source_primitive)];
  if(node.mesh!=v.source_mesh||node.skin>=0||raw>=v.asset->primitives.size()||
     v.asset->primitives[raw].source_mesh!=v.source_mesh||v.asset->primitives[raw].source_primitive!=v.source_primitive)
    throw std::invalid_argument("Linked native source part borrowed another node or primitive");
 }
 return NativeRigPoseScope{domain.group,a.asset,a.original_bytes,std::move(domain.objects)};
}
inline void require_native_pose_rig(const Scene&scene,const SculptObject&source,
 const SculptObject&target,bool reset_pose,weight_surface::Budget&work){
 const auto&a=*source.deformation;const auto&b=*target.deformation;
 if(bool(a.humanoid)!=bool(b.humanoid)||(a.humanoid&&a.humanoid->joints!=b.humanoid->joints))
  throw std::invalid_argument("Linked native source parts have different authored joint roles");
 const auto sw=scene.world_matrix(source.id),tw=scene.world_matrix(target.id);
 const bool same=sw.matrix()==tw.matrix();
 // Exact canonical native reframe in either direction accepts genuine absent
 // native identity space versus its explicit generated identity. No tolerance,
 // names-only matching or normalization of stored authority is introduced.
 const auto change=same?AffineTransform{}:tw.inverse()*sw;
 bool match=geometry_reference_rig_matches(*a.skeleton,*b.skeleton,change,same,work,2u*1024u*1024u);
 if(!match)match=geometry_reference_rig_matches(*b.skeleton,*a.skeleton,
   same?AffineTransform{}:sw.inverse()*tw,same,work,2u*1024u*1024u);
 if(!match)throw std::invalid_argument("Linked native source parts have incompatible rest, bind or placement authority");
 if(!reset_pose){
   work.step_items(a.skeleton->bones.size()+b.skeleton->bones.size()+1,8192);
   if(a.skeleton->local_matrices(a.pose)!=b.skeleton->local_matrices(b.pose))
     throw std::invalid_argument("Linked native source parts have unsynchronised poses; reset or resolve them explicitly");
 }
}
} // namespace dome::detail
