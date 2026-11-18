// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/humanoid-creator.hpp"
namespace dome::creator_detail {
// Same configured Shaper and normalized skeleton as native creation. The
// embedded licensed bytes are verified before even consulting the cache.
class CreatorShapeExport {
  struct Impl;
  std::shared_ptr<const Impl> impl_;
public:
  CreatorShapeExport(const HumanoidControls &,
      std::shared_ptr<const std::vector<uint8_t>>,
      const std::function<bool()> &cancelled = {});
  Vec3 point(Vec3 original) const;
  CreatedHumanoid reference(const std::function<bool()> &cancelled = {}) const;
  const Skeleton &skeleton() const;
  std::vector<uint32_t> joint_nodes() const;
  float scale() const;
  float sphere_radius(Vec3 original_center, float original_radius,
      const std::function<bool()> &cancelled = {}) const;
};
}
