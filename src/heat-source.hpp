// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "heat-binding.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/import.hpp"
#include <map>
namespace dome::detail {
// Private imported-source producer. No rig axis, topology or source ID is
// inferred from a display name. Current joint heads retain the actual metric;
// the unchanged original parent graph authenticates the endpoint identities.
inline std::vector<HeatSourceSegment> imported_heat_segments(const Skeleton&rig,
  const CreatorAppearance&source,const std::function<bool()>&cancelled={}){
 if(!source.asset||source.source_joint_nodes.size()!=rig.bones.size())throw std::invalid_argument("Imported bone heat needs complete exact source joint identities");
 rig.validate();const auto&nodes=source.asset->nodes;std::map<uint32_t,uint32_t> native;
 for(uint32_t i=0;i<rig.bones.size();++i){if(cancelled&&cancelled())throw std::runtime_error("Bone heat source capture cancelled");auto node=source.source_joint_nodes[i];if(node>=nodes.size()||!native.emplace(node,i).second)throw std::invalid_argument("Imported bone heat joint map is invalid or duplicated");}
 for(uint32_t i=0;i<rig.bones.size();++i){int parent=nodes[source.source_joint_nodes[i]].parent,actual=-1;size_t depth=0;while(parent>=0){if(cancelled&&cancelled())throw std::runtime_error("Bone heat source capture cancelled");if(size_t(parent)>=nodes.size()||++depth>nodes.size())throw std::invalid_argument("Imported bone heat source parent graph is invalid");auto at=native.find(uint32_t(parent));if(at!=native.end()){actual=int(at->second);break;}parent=nodes[size_t(parent)].parent;}if(actual!=rig.bones[i].parent)throw std::invalid_argument("Imported bone heat requires an authenticated current/source parent correspondence");}
 auto world=rig.world();auto head=[&](size_t i){return std::array<double,3>{world[i][12],world[i][13],world[i][14]};};std::vector<HeatSourceSegment>out;out.reserve(rig.bones.size()*2);
 for(uint32_t i=0;i<rig.bones.size();++i)if(rig.bones[i].deform){bool child=false;for(uint32_t j=0;j<rig.bones.size();++j){if(cancelled&&cancelled())throw std::runtime_error("Bone heat source capture cancelled");if(rig.bones[j].parent==int(i)){out.push_back({head(i),head(j),i});child=true;}}if(!child)out.push_back({head(i),head(i),i});}
 return out;
}
}
