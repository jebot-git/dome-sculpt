// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/rig.hpp"
namespace dome::detail {
enum class HeatOperator {
  FiniteElements,
  PositiveGraph,
  BoundedFiniteElements
};
enum class HeatVisibility { AllFaces, EntryFaces };
enum class HeatOcclusion { WholeMesh, ConnectedSurface };
enum class HeatNormals { AreaWeighted, UnitFaces };
enum class HeatMass { Barycentric, MixedVoronoi };
enum class HeatEdges { CotangentHalf, IncidentMean };
enum class HeatSeedRule { DistinctBones, ReferenceSegments };
enum class HeatOutput { Raw, ReferenceLimit };
// Private open-surface PDE prototype. Raw vertex/topology identity is retained;
// no coordinate-only welding, authoring admission or legacy solver change.
struct HeatBindingOptions {
  HeatOperator operation = HeatOperator::FiniteElements;
  HeatVisibility visibility = HeatVisibility::AllFaces;
  HeatOcclusion occlusion = HeatOcclusion::WholeMesh;
  HeatNormals geometry_normals = HeatNormals::AreaWeighted;
  HeatMass mass = HeatMass::Barycentric;
  HeatEdges edges = HeatEdges::CotangentHalf;
  HeatSeedRule seed_rule = HeatSeedRule::DistinctBones;
  HeatOutput output = HeatOutput::Raw;
  WeightLimits limits;
  double maximum_seed_distance = .3, heat_strength = 1,
         relative_tolerance = 1e-9;
  size_t maximum_work = 500'000'000, maximum_bytes = 256u * 1024u * 1024u;
  uint32_t maximum_iterations = 2048;
};
struct HeatBindingReport {
  size_t vertices = 0, components = 0, seeded_bones = 0, iterations = 0,
         work = 0, planned_bytes = 0;
  size_t negative_edges = 0, matrix_vertices = 0;
  size_t bound_sweeps = 0;
  double removed_negative_stiffness = 0;
  double maximum_relative_residual = 0, minimum_unclamped_weight = 0,
         maximum_partition_error = 0;
  double maximum_kkt_residual = 0;
};
// Caller-authenticated actual metric source geometry. Imported raw joint axes
// must not be silently interpreted as authored native bone-Y tail authority.
struct HeatSourceSegment {
  std::array<double, 3> head{}, tail{};
  uint32_t bone = 0;
};
struct HeatPositivityFailure : std::runtime_error {
  uint32_t joint;
  double minimum;
  HeatPositivityFailure(uint32_t j, double m)
      : std::runtime_error("Bone heat FEM field violates its independently "
                           "checked positivity bound"),
        joint(j), minimum(m) {}
};
struct HeatSeedFailure : std::runtime_error {
  size_t component, rows, entry_hits, exit_hits, grazing_hits, own_hits,
      foreign_hits;
  std::array<double, 3> lower, upper;
  HeatSeedFailure(size_t id, size_t r, size_t e, size_t x, size_t g, size_t own,
                  size_t foreign, std::array<double, 3> lo,
                  std::array<double, 3> hi)
      : std::runtime_error("Bone heat has a disconnected component without a "
                           "visible deform-bone seed"),
        component(id), rows(r), entry_hits(e), exit_hits(x), grazing_hits(g),
        own_hits(own), foreign_hits(foreign), lower(lo), upper(hi) {}
};
SkinWeights heat_bind_surface(const Mesh &, const Skeleton &,
                              const HeatBindingOptions &,
                              const std::function<bool()> &cancelled = {},
                              HeatBindingReport *report = nullptr,
                              std::span<const HeatSourceSegment> sources = {},
                              std::span<const uint8_t> eligible = {},
                              std::span<const uint32_t> row_groups = {});
} // namespace dome::detail
