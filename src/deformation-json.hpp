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
  if(deformation.at("version")!=2)throw std::invalid_argument("Exact pose-local matrix requires deformation version two");
  const auto &m=joint.at("full_local_matrix");if(!m.is_array()||m.size()!=16)throw std::invalid_argument("Invalid exact pose-local matrix dimensions");for(const auto &v:m)if(!v.is_number())throw std::invalid_argument("Invalid exact pose-local matrix component");entries+=16;found=true;
 }
 if(deformation.at("version")==2&&!found)throw std::invalid_argument("Deformation version two requires exact pose-local authority");
 return entries;
}
} // namespace dome::detail
