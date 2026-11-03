// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "heat-midpoint-source.hpp"
namespace dome::detail {
// Explicit geometric construction: axial handles retain their authenticated
// spatial segments; appendicular primary handles use their unique midpoint.
// Auxiliary sources retain their exact input segments. No pose, skin value,
// surface fit or display name participates in this classification.
inline HeatHandleConstruction
make_axial_heat_handles(std::span<const HeatSourceSegment> physical,
                        const Skeleton &rig, const HumanoidMapping &mapping,
                        const HeatBindingOptions &options,
                        const std::function<bool()> &cancelled = {}) {
  if (cancelled && cancelled())
    throw std::runtime_error("Axial heat source capture cancelled");
  const size_t bones = rig.bones.size();
  if (!bones || bones > 256 || mapping.joints.size() > 55)
    throw std::invalid_argument("Invalid bounded axial heat role domain");
  const size_t work = bones * bones * 4 + bones * 16 + 256;
  const size_t bytes = bones * 256 + 256 * sizeof(uint32_t);
  if (work >= options.maximum_work || bytes >= options.maximum_bytes)
    throw std::length_error("Axial heat role capture exceeds work/byte caps");
  mapping.validate(rig, false);
  std::array<uint32_t, 256> midpoint{};
  size_t count = 0;
  for (const auto &[role, joint] : mapping.joints) {
    if (cancelled && cancelled())
      throw std::runtime_error("Axial heat source capture cancelled");
    const bool axial = role == "hips" || role == "spine" || role == "chest" ||
                       role == "upperChest" || role == "neck" || role == "head";
    if (!axial)
      midpoint[count++] = joint;
  }
  auto remaining = options;
  remaining.maximum_work -= work;
  remaining.maximum_bytes -= bytes;
  auto result = make_heat_handles(physical, {midpoint.data(), count}, bones,
                                  HeatHandlePolicy::PrimaryMidpoints, remaining,
                                  cancelled);
  result.work += work;
  result.planned_bytes += bytes;
  return result;
}
} // namespace dome::detail
