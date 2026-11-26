// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "vroid-shape-export.hpp"
#include <nlohmann/json.hpp>
namespace dome::detail {
class CreatorVrmAuthority {
  creator_detail::CreatorShapeExport shape_;
  std::vector<uint32_t> nodes_;
  bool nonlinear_physics_placement_ = false;
public:
  CreatorVrmAuthority(const std::vector<ObjectSurface> &, const CreatorAppearance &,
      const std::function<bool()> &);
  Vec3 point(Vec3 p) const { return shape_.point(p); }
  float scale() const { return shape_.scale(); }
  const std::vector<uint32_t> &joint_nodes() const { return nodes_; }
  nlohmann::json refit(const nlohmann::json &,const ImportedAsset &,
      const std::function<bool()> &) const;
};
bool has_creator_vrm_origin(const std::vector<ObjectSurface> &);
}
