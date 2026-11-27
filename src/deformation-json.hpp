// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <nlohmann/json.hpp>
namespace dome::detail {
// Count and structurally validate the new bounded numeric channel before any
// authoring arrays are allocated. Outer Scene feature admission is separate.
inline size_t encoded_pose_matrix_entries(const nlohmann::json &deformation){
 if(deformation.is_null())return 0;
 const auto &pose=deformation.at("pose");if(!pose.is_array()||pose.size()>256)throw std::invalid_argument("Invalid pose matrix table dimensions");
 size_t entries=0;bool found=false;
 for(const auto &joint:pose){if(!joint.is_object())throw std::invalid_argument("Invalid pose joint record");if(!joint.contains("full_local_matrix"))continue;
  if(deformation.at("version")!=2&&deformation.at("version")!=3)throw std::invalid_argument("Exact pose-local matrix requires deformation version two or three");
  const auto &m=joint.at("full_local_matrix");if(!m.is_array()||m.size()!=16)throw std::invalid_argument("Invalid exact pose-local matrix dimensions");for(const auto &v:m)if(!v.is_number())throw std::invalid_argument("Invalid exact pose-local matrix component");entries+=16;found=true;
 }
 if(deformation.at("version")==2&&!found)throw std::invalid_argument("Deformation version two requires exact pose-local authority");
 return entries;
}
inline size_t encoded_source_skin_entries(const nlohmann::json &deformation){
 if(deformation.is_null())return 0;
 const bool linked=deformation.at("version")==3;
 for(const auto *key:{"source_skin_instance","source_skin_node","source_skin_joints"})
  if(linked!=deformation.contains(key))throw std::invalid_argument("Incomplete source skin link");
 if(!linked)return 0;
 const auto &instance=deformation.at("source_skin_instance"), &node_id=deformation.at("source_skin_node");
 if(!instance.is_number_integer()||instance<=0||!node_id.is_number_integer()||node_id<0||node_id>=100000)
  throw std::invalid_argument("Invalid source skin instance or mesh node");
 const auto &nodes=deformation.at("source_skin_joints");
 if(!nodes.is_array()||nodes.empty()||nodes.size()>256)throw std::invalid_argument("Invalid source skin joint table dimensions");
 for(const auto &node:nodes)
   if(!node.is_number_integer()||node<0||node>=100000)throw std::invalid_argument("Invalid source skin joint identity");
 return nodes.size();
}
} // namespace dome::detail
