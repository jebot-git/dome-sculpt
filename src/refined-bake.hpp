// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "key-mirror-refinement.hpp"

namespace dome::detail {
inline bool same_bake_order(const Mesh &a,const Mesh &b){
  return a.positions.size()==b.positions.size()&&a.indices==b.indices;
}
struct RefinedBakeMap {
  std::shared_ptr<const Mesh> original;
  std::vector<MirrorSurfacePoint> corners;
};
std::optional<RefinedBakeMap> replay_refined_bake(const SculptObject &target,
    const SculptObject &dependency,bool cage,const std::function<bool()> &cancelled);
}
