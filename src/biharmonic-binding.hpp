// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bounded-quadratic.hpp"
#include "heat-binding.hpp"
namespace dome::detail {
enum class BiharmonicProjection { VoronoiMinimum, IndependentComponent };
struct BiharmonicBindingOptions {
  BiharmonicProjection projection = BiharmonicProjection::VoronoiMinimum;
  HeatBindingOptions source;
  BoundedQuadraticOptions solve;
};
struct BiharmonicBindingReport {
  size_t vertices = 0, components = 0, fixed_rows = 0, source_bones = 0,
         active_iterations = 0, cg_iterations = 0, work = 0, planned_bytes = 0;
  double maximum_kkt_residual = 0, maximum_partition_error = 0;
};
// Explicit SURFACE-domain bounded biharmonic energy L' M^-1 L, not volumetric
// BBW. Handles are visible nearest-source minima projected onto each connected
// surface component. Coincident projected handles share normalized boundary
// values. Missing source/component handles refuse. Geometry/artist arrays are
// never repaired, welded or modified; authenticated row groups are optional.
SkinWeights bounded_biharmonic_surface(
    const Mesh &, const Skeleton &, const BiharmonicBindingOptions &,
    std::span<const HeatSourceSegment>, std::span<const uint8_t> eligible = {},
    std::span<const uint32_t> row_groups = {},
    const std::function<bool()> &cancelled = {},
    BiharmonicBindingReport *report = nullptr,
    std::span<const std::array<uint64_t, 4>> component_usage = {});
} // namespace dome::detail
