// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bounded-quadratic.hpp"
#include "heat-binding.hpp"
namespace dome::detail {
enum class BiharmonicProjection { VoronoiMinimum, IndependentComponent };
enum class BiharmonicPreconditioner {
  Diagonal,
  SquaredIncompleteLaplacian,
  SparseLDL
};
enum class BiharmonicInitialization {
  ComponentConstant,
  UnconstrainedWarmStart
};
enum class BiharmonicBoxSolver {
  PrimalFeasible,
  BlockActiveSet,
  CoupledDual,
  CoupledSquaredDual,
  CoupledProjectedDual
};
struct BiharmonicBindingOptions {
  BiharmonicBoxSolver box_solver = BiharmonicBoxSolver::PrimalFeasible;
  BiharmonicInitialization initialization =
      BiharmonicInitialization::ComponentConstant;
  BiharmonicPreconditioner preconditioner = BiharmonicPreconditioner::Diagonal;
  BiharmonicProjection projection = BiharmonicProjection::VoronoiMinimum;
  HeatBindingOptions source;
  BoundedQuadraticOptions solve;
};
struct BiharmonicBindingReport {
  size_t vertices = 0, components = 0, fixed_rows = 0, source_bones = 0,
         active_iterations = 0, cg_iterations = 0, warm_cg_iterations = 0,
         warm_clamps = 0, warm_outside = 0, sparse_nonzeros = 0,
         factor_nonzeros = 0, factorizations = 0, modified_pivots = 0,
         free_fill_peak = 0, last_free_vertices = 0, work = 0,
         planned_bytes = 0, dual_iterations = 0, dual_backtracks = 0,
         inverse_applications = 0, inverse_refinements = 0;
  double maximum_kkt_residual = 0, maximum_partition_error = 0,
         maximum_dual_kkt = 0, maximum_primal_kkt = 0,
         maximum_inverse_residual = 0, maximum_duality_gap = 0;
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
