// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/avatar.hpp"
#include "dome/affine.hpp"
#include <bitset>
namespace dome::detail {
// Exact source placement above hips, never an editable humanoid/deforming
// joint. Owning DeformationState must additionally exclude actual influences.
inline std::bitset<256> humanoid_placement_prefix(const Skeleton &s,
                                                const HumanoidMapping &mapping) {
  std::bitset<256> result;
  if (!s.space_matrix || !s.inverse_bind_override || s.bones.size() > result.size()) return result;
  const auto hips=mapping.joints.find("hips");
  if(hips==mapping.joints.end() || hips->second>=s.bones.size())return result;
  bool reflected_reference=false;
  for(int at=s.bones[hips->second].parent;at>=0;at=s.bones[at].parent){
    if(size_t(at)>=s.bones.size() || result.test(size_t(at)))return {};
    const auto &bone=s.bones[at];
    if(bone.deform || !bone.rest_matrix)return {};
    reflected_reference|=bone.rest.scale.x<0 || bone.rest.scale.y<0 || bone.rest.scale.z<0;
    result.set(size_t(at));
  }
  // Ordinary positive prefix/weight behavior is unchanged. This representation
  // is needed only for a reflected exact source placement, not as a new rule
  // for every legacy nondeforming ancestor.
  if(!reflected_reference)return {};
  (void)AffineTransform(*s.space_matrix).inverse();
  for(size_t at=0;at<s.bones.size();++at)if(result.test(at))
    (void)AffineTransform(*s.bones[at].rest_matrix).inverse();
  return result;
}
} // namespace dome::detail
