// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/rig.hpp"

namespace dome {
class Scene;
struct SculptObject;
}
namespace dome::detail {
class SkinGeometryWork;

// UINT32_MAX means the donor joint has no authoritative receiver match.
std::vector<uint32_t> transfer_joint_map(const Scene &,const SculptObject &source,
                                         const SculptObject &target,SkinGeometryWork &);

SkinWeights transfer_weights_with_joint_map(const Mesh &source,const SkinWeights &,
    const Mesh &target,size_t source_bones,size_t target_bones,std::span<const uint32_t> map,
    float distance,WeightLimits,const std::function<bool()> &, size_t maximum_work);
} // namespace dome::detail
