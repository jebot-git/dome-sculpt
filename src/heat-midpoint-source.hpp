// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "heat-source.hpp"
#include <numeric>
namespace dome::detail {
enum class HeatHandlePolicy { OriginalSegments, PrimaryMidpoints };
struct HeatHandleConstruction {
  std::vector<HeatSourceSegment> handles;
  size_t midpoint_sources = 0, work = 0, planned_bytes = 0;
};
// Algorithmic point handles, not fabricated original nodes or modified bones.
// Authenticated humanoid finite segments use their unique geometric centroid;
// original point handles and auxiliary surface controls stay unchanged. This
// policy is chosen before solving and never depends on fit/pose outcomes.
inline HeatHandleConstruction
make_heat_handles(std::span<const HeatSourceSegment> physical,
                  std::span<const uint32_t> primary, size_t bones,
                  HeatHandlePolicy policy, const HeatBindingOptions &options,
                  const std::function<bool()> &cancelled = {}) {
  if (cancelled && cancelled())
    throw std::runtime_error("Heat handle capture cancelled");
  if (!bones || bones > 256 || physical.empty() ||
      physical.size() > bones * 2 || primary.size() > bones ||
      (policy != HeatHandlePolicy::OriginalSegments &&
       policy != HeatHandlePolicy::PrimaryMidpoints))
    throw std::invalid_argument("Invalid bounded heat handle construction");
  const size_t work = bones + primary.size() + physical.size() * 16,
               bytes = physical.size() * sizeof(HeatSourceSegment) + 256;
  if (work > options.maximum_work || bytes > options.maximum_bytes)
    throw std::length_error(
        "Heat handles exceed preconstruction work/byte caps");
  std::array<bool, 256> roles{};
  for (auto joint : primary) {
    if (joint >= bones || roles[joint])
      throw std::invalid_argument(
          "Heat handle role identities are invalid or repeated");
    roles[joint] = true;
  }
  HeatHandleConstruction result;
  result.work = work;
  result.planned_bytes = bytes;
  result.handles.reserve(physical.size());
  for (auto source : physical) {
    if (cancelled && cancelled())
      throw std::runtime_error("Heat handle capture cancelled");
    if (source.bone >= bones)
      throw std::invalid_argument("Heat handle is outside the exact rig");
    for (double x : source.head)
      if (!std::isfinite(x))
        throw std::invalid_argument("Nonfinite source head");
    for (double x : source.tail)
      if (!std::isfinite(x))
        throw std::invalid_argument("Nonfinite source tail");
    if (policy == HeatHandlePolicy::PrimaryMidpoints && roles[source.bone] &&
        source.head != source.tail) {
      for (int k = 0; k < 3; ++k)
        source.head[k] = source.tail[k] =
            std::midpoint(source.head[k], source.tail[k]);
      ++result.midpoint_sources;
    }
    result.handles.push_back(source);
  }
  return result;
}
} // namespace dome::detail
