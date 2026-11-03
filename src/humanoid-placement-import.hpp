// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "import-skin.hpp"
#include <bitset>
namespace dome::detail {
// Worker-only adapter for an authenticated original-node closure. Exported
// skins can declare unweighted ancestors as joint slots; declaration alone is
// not evidence that such external placement actually deforms any vertex.
// Raw source joint tables/IBMs and native slot IDs remain untouched.
inline void adopt_humanoid_placement_prefix(
    ImportedRig &rig,const ImportedAsset &asset,uint32_t mesh_node,
    const std::map<std::string,uint32_t> &human_nodes,const SkinWeights &weights,
    const std::function<bool()> &cancelled={}) {
  auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Humanoid placement adoption cancelled");};
  stop();rig.skeleton.validate();
  const auto hips=human_nodes.find("hips");
  if(hips==human_nodes.end())return;
  const auto at=std::find(rig.source_nodes.begin(),rig.source_nodes.end(),hips->second);
  if(at==rig.source_nodes.end())throw std::invalid_argument("Humanoid hips lacks actual source slot identity");
  std::bitset<256> prefix;bool reflected=false;
  for(int slot=rig.skeleton.bones[size_t(at-rig.source_nodes.begin())].parent;slot>=0;slot=rig.skeleton.bones[slot].parent){
    prefix.set(size_t(slot));const auto scale=rig.skeleton.bones[slot].rest.scale;
    reflected|=scale.x<0||scale.y<0||scale.z<0;
  }
  if(!reflected)return;
  if(rig.source_nodes.size()!=rig.skeleton.bones.size() ||
      !rig.skeleton.inverse_bind_override || !rig.skeleton.space_matrix ||
      *rig.skeleton.space_matrix!=imported_inverse(asset.nodes.at(mesh_node).world) ||
      asset.nodes.at(mesh_node).skin<0)
    throw std::invalid_argument("Humanoid placement needs exact source mesh/own-skin space authority");
  std::set<uint32_t> unique;
  for(auto source:rig.source_nodes)if(source>=asset.nodes.size()||!unique.insert(source).second)
    throw std::invalid_argument("Humanoid placement source slots are invalid or repeated");
  for(size_t slot=0;slot<rig.source_nodes.size();++slot)if(prefix.test(slot)){
    stop();const auto source=rig.source_nodes[slot];const auto &node=asset.nodes[source];const auto &bone=rig.skeleton.bones[slot];
    if(!bone.rest_matrix || *bone.rest_matrix!=node.local ||
        node.parent!=(bone.parent<0?-1:int(rig.source_nodes.at(size_t(bone.parent)))))
      throw std::invalid_argument("Humanoid external placement differs from exact original source closure");
    for(const auto &[role,id]:human_nodes)if(id==source)
      throw std::invalid_argument("A humanoid role cannot become external placement");
    (void)AffineTransform(node.local).inverse();
  }
  weights.validate(weights.vertices.size(),rig.skeleton.bones.size());
  size_t visits=0;
  for(const auto &vertex:weights.vertices)for(const auto &influence:vertex){
    if((++visits&1023)==0)stop();
    if(influence.weight!=0 && prefix.test(influence.joint))
      throw std::invalid_argument("Humanoid external source placement carries actual skin weights");
  }
  stop();
  for(size_t slot=0;slot<rig.source_nodes.size();++slot)if(prefix.test(slot))
    rig.skeleton.bones[slot].deform=false;
}
} // namespace dome::detail
