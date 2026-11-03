// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/rig.hpp"
namespace dome::detail {
// Private open-surface PDE prototype. Raw vertex/topology identity is retained;
// no coordinate-only welding, authoring admission or legacy solver change.
struct HeatBindingOptions {
  WeightLimits limits;
  double maximum_seed_distance=.3,heat_strength=1,relative_tolerance=1e-9;
  size_t maximum_work=500'000'000,maximum_bytes=256u*1024u*1024u;
  uint32_t maximum_iterations=2048;
};
struct HeatBindingReport {
  size_t vertices=0,components=0,seeded_bones=0,iterations=0,work=0,planned_bytes=0;
  double maximum_relative_residual=0,minimum_unclamped_weight=0,
    maximum_partition_error=0;
};
SkinWeights heat_bind_surface(const Mesh&,const Skeleton&,const HeatBindingOptions&,
  const std::function<bool()>&cancelled={},HeatBindingReport*report=nullptr);
}
