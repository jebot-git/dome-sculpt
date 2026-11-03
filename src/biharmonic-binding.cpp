// SPDX-License-Identifier: GPL-3.0-or-later
// Private research TU reuses the independently checked native FEM/visibility
// assembly. No copied libigl implementation/runtime. Operator and constrained
// energy informed by pinned libigl bbw/harmonic and Jacobson et al.2011.
#include "biharmonic-binding.hpp"
#include "biharmonic-sparse-ldl.hpp"
#include "block-bounded-quadratic.hpp"
#include "fixed-quadratic.hpp"
#include "heat-binding.cpp"
#include "squared-laplacian-preconditioner.hpp"
#include <optional>
namespace dome::detail {
namespace {
struct BiharmonicOperator {
  const System &l;
  std::vector<double> diagonal, temporary;
  double mass_scale = 0;
  std::optional<SquaredLaplacianPreconditioner> incomplete;
  std::optional<BiharmonicSparseLDL> full;
  bool wants_full = false, restrict_full = false, lagged_full = false;
  unsigned free_solves = 0;
  std::vector<uint8_t> base_status;
  size_t full_bytes = 0;
  BiharmonicOperator(const System &system, Budget &budget,
                     std::span<const uint8_t> fixed = {}, bool squared = false,
                     size_t maximum_bytes = 0, bool sparse = false,
                     bool restricted = false, bool lagged = false)
      : l(system), diagonal(l.diagonal.size()), temporary(l.diagonal.size()) {
    budget.step(diagonal.size() * 2 + l.values.size());
    for (double mass : l.mass)
      mass_scale += mass;
    mass_scale /= diagonal.size();
    if (!(mass_scale > 0) || !std::isfinite(mass_scale))
      throw std::runtime_error("Invalid biharmonic mass scale");
    for (size_t i = 0; i < diagonal.size(); ++i) {
      double q = l.diagonal[i] * l.diagonal[i] / l.mass[i];
      for (size_t j = l.starts[i]; j < l.starts[i + 1]; ++j)
        q += l.values[j] * l.values[j] / l.mass[l.columns[j]];
      diagonal[i] = mass_scale * q;
      if (!(diagonal[i] > 0) || !std::isfinite(diagonal[i]))
        throw std::runtime_error("Invalid biharmonic quadratic diagonal");
    }
    wants_full = sparse;
    restrict_full = restricted || lagged;
    lagged_full = lagged;
    if (restrict_full) {
      budget.step(fixed.size());
      base_status.assign(fixed.begin(), fixed.end());
    }
    full_bytes = maximum_bytes;
    if (squared)
      incomplete.emplace(l, fixed, maximum_bytes, budget);
  }
  // Refresh only BETWEEN fixed-mask CG solves. Fixed cadence is an explicit
  // preconditioner strategy, never a retry after a failure or a quality switch.
  void begin_free_solve(std::span<const uint8_t> status, Budget &budget) {
    if (!lagged_full)
      return;
    if (free_solves++ % 8 == 0) {
      budget.step(status.size());
      base_status.assign(status.begin(), status.end());
    }
  }
  void precondition(std::span<const double> r, std::span<double> z,
                    std::span<const uint8_t> status, Budget &budget) {
    if (wants_full && !full)
      full.emplace(l, full_bytes, budget);
    if (full) {
      if (restrict_full)
        full->apply_restricted(r, z, base_status, status, budget, lagged_full);
      else
        full->apply(r, z, status, budget);
    } else if (incomplete)
      incomplete->apply(r, z, status, budget);
    else {
      budget.step(r.size());
      for (size_t i = 0; i < r.size(); ++i)
        z[i] = status[i] ? 0 : r[i] / diagonal[i];
    }
  }
  void apply(std::span<const double> x, std::span<double> out, Budget &budget) {
    l.multiply(x, temporary, budget);
    budget.step(temporary.size());
    for (size_t i = 0; i < temporary.size(); ++i)
      temporary[i] *= mass_scale / l.mass[i];
    l.multiply(temporary, out, budget);
  }
};
} // namespace
SkinWeights bounded_biharmonic_surface(
    const Mesh &mesh, const Skeleton &rig,
    const BiharmonicBindingOptions &options,
    std::span<const HeatSourceSegment> sources,
    std::span<const uint8_t> eligible, std::span<const uint32_t> row_groups,
    const std::function<bool()> &cancelled, BiharmonicBindingReport *report,
    std::span<const std::array<uint64_t, 4>> component_usage) {
  const auto &o = options.source;
  const bool block = options.box_solver == BiharmonicBoxSolver::BlockActiveSet;
  if (options.box_solver != BiharmonicBoxSolver::PrimalFeasible && !block)
    throw std::invalid_argument("Unknown biharmonic box solver");
  const bool warm = options.initialization ==
                    BiharmonicInitialization::UnconstrainedWarmStart;
  if (options.initialization != BiharmonicInitialization::ComponentConstant &&
      !warm)
    throw std::invalid_argument("Unknown biharmonic initialization");
  const bool squared = options.preconditioner ==
                       BiharmonicPreconditioner::SquaredIncompleteLaplacian;
  const bool lagged =
      options.preconditioner == BiharmonicPreconditioner::LaggedSparseLDL;
  const bool restricted =
      lagged ||
      options.preconditioner == BiharmonicPreconditioner::RestrictedSparseLDL;
  const bool sparse = restricted || options.preconditioner ==
                                        BiharmonicPreconditioner::SparseLDL;
  if (options.preconditioner != BiharmonicPreconditioner::Diagonal &&
      !squared && !sparse)
    throw std::invalid_argument("Unknown biharmonic preconditioner");
  const bool independent =
      options.projection == BiharmonicProjection::IndependentComponent;
  Budget budget{0, o.maximum_work, cancelled};
  budget.stop();
  const size_t n = mesh.positions.size(), b = rig.bones.size(),
               corners = mesh.indices.size();
  if (!n || n > 2'000'000 || !corners || corners > 12'000'000 || corners % 3 ||
      !b || b > 256 || o.limits.max_influences < 1 ||
      o.limits.max_influences > 32 || !std::isfinite(o.limits.prune) ||
      o.limits.prune < 0 || o.limits.prune > 1 || sources.empty() ||
      sources.size() > b * 2 || !o.maximum_work ||
      o.maximum_work > 500'000'000 || o.maximum_bytes < 4096 ||
      o.maximum_bytes > 512u * 1024u * 1024u ||
      !(o.maximum_seed_distance > 0) ||
      !std::isfinite(o.maximum_seed_distance) ||
      o.distance_rule != HeatDistanceRule::EuclideanSurface ||
      o.seed_rule != HeatSeedRule::ReferenceSegments ||
      o.visibility != HeatVisibility::EntryFaces ||
      o.occlusion != HeatOcclusion::ConnectedSurface ||
      (!eligible.empty() && eligible.size() != b) ||
      (!row_groups.empty() && row_groups.size() != n) ||
      (options.projection != BiharmonicProjection::VoronoiMinimum &&
       !independent) ||
      (independent && component_usage.size() != n) ||
      (!independent && !component_usage.empty()))
    throw std::invalid_argument(
        "Invalid explicit surface-biharmonic domain/options");
  // Conservative peak: assembly map/CSR, two row-by-bone projection tables,
  // optional source multiplicities, QP scratch, both grouped/raw weight owners.
  const size_t precondition_bytes = squared ? n * 128 + corners * 48 + 8192 : 0;
  const size_t planned = n * (1024 + 32 * o.limits.max_influences + b * 18) +
                         corners * 160 + b * b * 64 + 16384 +
                         precondition_bytes + (restricted ? n : 0);
  if (planned > o.maximum_bytes || n > o.maximum_work / (sources.size() + 1))
    throw std::length_error(
        "Biharmonic construction exceeds byte/work admission");
  budget.step(n + corners + b * b);
  mesh.validate();
  rig.validate();
  normalize_weights({{0, 1}}, b, {}, o.limits);
  std::array<bool, 256> covered{};
  for (uint8_t value : eligible)
    if (value > 1)
      throw std::invalid_argument("Invalid eligible source bit");
  for (const auto &source : sources) {
    budget.step(16);
    if (source.bone >= b || !rig.bones[source.bone].deform ||
        (!eligible.empty() && !eligible[source.bone]))
      throw std::invalid_argument("Invalid biharmonic source joint identity");
    for (double x : source.head)
      if (!std::isfinite(x))
        throw std::invalid_argument("Nonfinite source head");
    for (double x : source.tail)
      if (!std::isfinite(x))
        throw std::invalid_argument("Nonfinite source tail");
    covered[source.bone] = true;
  }
  for (size_t i = 0; i < b; ++i)
    if (rig.bones[i].deform && (eligible.empty() || eligible[i]) && !covered[i])
      throw std::invalid_argument(
          "Biharmonic input omitted an eligible source");
  auto s = assemble(mesh, budget, HeatOperator::FiniteElements, row_groups,
                    HeatNormals::UnitFaces, HeatMass::MixedVoronoi,
                    HeatEdges::CotangentHalf);
  auto diagonal = s.diagonal;
  seed(s, mesh, rig, o, budget, sources, eligible, independent);
  s.diagonal.swap(diagonal); // Restore exact L, not floating subtraction of H.
  const size_t rows = s.positions.size();
  std::vector<std::array<uint64_t, 4>> domains(rows);
  std::vector<uint8_t> assigned(rows);
  if (independent)
    for (size_t raw = 0; raw < n; ++raw) {
      budget.step(8);
      auto row = s.raw_to_row[raw], root = s.parent[row];
      const auto mask = component_usage[raw];
      if (mask == std::array<uint64_t, 4>{} ||
          (assigned[root] && domains[root] != mask))
        throw std::invalid_argument(
            "Component intent is empty or varies inside a component");
      assigned[root] = 1;
      domains[root] = mask;
      for (size_t joint = 0; joint < 256; ++joint) {
        budget.step();
        if ((mask[joint / 64] & (uint64_t(1) << (joint % 64))) &&
            (joint >= b || !covered[joint]))
          throw std::invalid_argument(
              "Component intent names absent source joint");
      }
    }
  std::vector<double> distances(rows * b, INFINITY),
      minimum(rows * b, INFINITY);
  for (size_t i = 0; i < rows; ++i)
    for (const auto &source : sources) {
      budget.step(16);
      if (independent && !(domains[s.parent[i]][source.bone / 64] &
                           (uint64_t(1) << (source.bone % 64))))
        continue;
      if (!(s.seeds[i][source.bone / 64] & (uint64_t(1) << (source.bone % 64))))
        continue;
      double distance =
          norm(sub(s.positions[i],
                   closest(s.positions[i],
                           Segment{source.head, source.tail, source.bone})));
      auto &here = distances[i * b + source.bone];
      here = std::min(here, distance);
      auto &global = minimum[size_t(s.parent[i]) * b + source.bone];
      global = std::min(global, distance);
    }
  std::vector<uint8_t> fixed(rows);
  std::vector<std::array<uint64_t, 4>> anchors(rows);
  std::array<bool, 256> anchored{};
  BiharmonicBindingReport proof;
  proof.vertices = rows;
  proof.components = s.components;
  for (size_t i = 0; i < rows; ++i)
    for (size_t joint = 0; joint < b; ++joint) {
      budget.step();
      double d = distances[i * b + joint],
             best = minimum[size_t(s.parent[i]) * b + joint];
      if (std::isfinite(d) && d <= best * (1 + 1e-4)) {
        anchors[i][joint / 64] |= uint64_t(1) << (joint % 64);
        anchored[joint] = true;
        fixed[i] = 1;
      }
    }
  if (independent)
    for (size_t row = 0; row < rows; ++row)
      if (s.parent[row] == row)
        for (size_t joint = 0; joint < b; ++joint)
          if (domains[row][joint / 64] & (uint64_t(1) << (joint % 64))) {
            budget.step();
            if (!std::isfinite(minimum[row * b + joint]))
              throw std::runtime_error(
                  "Declared component source lacks a visible projection");
          }
  for (size_t i = 0; i < b; ++i)
    if (covered[i]) {
      if (!anchored[i])
        throw std::runtime_error(
            "Eligible biharmonic source lacks a visible projected handle");
      ++proof.source_bones;
    }
  proof.fixed_rows = std::count(fixed.begin(), fixed.end(), uint8_t(1));
  if (!proof.fixed_rows)
    throw std::runtime_error("Biharmonic surface has no projected constraints");
  distances.clear();
  minimum.clear();
  diagonal.clear(); // Peak remains accounted.
  BiharmonicOperator q(s, budget, fixed, squared,
                       sparse ? o.maximum_bytes - planned : precondition_bytes,
                       sparse, restricted, lagged);
  auto controls = options.solve;
  controls.maximum_bytes = std::min(controls.maximum_bytes, o.maximum_bytes);
  std::vector<double> values(rows), field(rows), sum(rows), counts(rows),
      initial(rows);
  for (size_t i = 0; i < rows; ++i)
    if (fixed[i])
      ++counts[s.parent[i]];
  SkinWeights grouped;
  grouped.vertices.resize(rows);
  for (auto &row : grouped.vertices)
    row.reserve(o.limits.max_influences + 1);
  for (uint32_t joint = 0; joint < b; ++joint)
    if (covered[joint]) {
      budget.stop();
      std::fill(values.begin(), values.end(), 0);
      std::fill(initial.begin(), initial.end(), 0);
      for (size_t i = 0; i < rows; ++i)
        if (fixed[i]) {
          budget.step(8);
          size_t count = 0;
          for (auto mask : anchors[i])
            count += std::popcount(mask);
          values[i] = (anchors[i][joint / 64] & (uint64_t(1) << (joint % 64)))
                          ? 1. / count
                          : 0;
          initial[s.parent[i]] += values[i];
        }
      for (size_t i = 0; i < rows; ++i) {
        budget.step();
        if (!(counts[s.parent[i]] > 0))
          throw std::runtime_error(
              "Biharmonic component lacks fixed constraints");
        field[i] = initial[s.parent[i]] / counts[s.parent[i]];
      }
      if (warm) {
        auto initialized =
            solve_fixed_quadratic(q, fixed, values, field, controls, budget);
        proof.warm_cg_iterations += initialized.cg_iterations;
        budget.step(rows);
        for (double &value : field) {
          proof.warm_outside += value < 0 || value > 1;
          if (!block) {
            proof.warm_clamps += value < 0 || value > 1;
            value = std::clamp(value, 0., 1.);
          }
        }
      }
      auto solved = block ? solve_block_bounded_quadratic(
                                q, fixed, values, field, controls, budget)
                          : solve_bounded_quadratic(q, fixed, values, field,
                                                    controls, budget);
      proof.active_iterations += solved.active_iterations;
      proof.cg_iterations += solved.cg_iterations;
      proof.maximum_kkt_residual =
          std::max(proof.maximum_kkt_residual, solved.kkt_residual);
      for (size_t i = 0; i < rows; ++i) {
        budget.step(8);
        double value = field[i];
        sum[i] += value;
        if (value <= 0)
          continue;
        auto &row = grouped.vertices[i];
        row.push_back({joint, float(value)});
        std::sort(row.begin(), row.end(), [](auto a, auto b) {
          return a.weight != b.weight ? a.weight > b.weight : a.joint < b.joint;
        });
        if (row.size() > o.limits.max_influences)
          row.pop_back();
      }
    }
  for (size_t i = 0; i < rows; ++i) {
    budget.step();
    proof.maximum_partition_error =
        std::max(proof.maximum_partition_error, std::abs(sum[i] - 1));
    grouped.vertices[i] =
        normalize_weights(std::move(grouped.vertices[i]), b, {}, o.limits);
  }
  SkinWeights out;
  out.vertices.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    budget.step(o.limits.max_influences);
    out.vertices.push_back(grouped.vertices[s.raw_to_row[i]]);
  }
  out.validate(n, b);
  budget.stop();
  proof.work = budget.used;
  proof.planned_bytes = planned + (q.full ? q.full->planned_bytes : 0);
  if (q.full) {
    proof.sparse_nonzeros = q.full->matrix_nonzeros;
    proof.factor_nonzeros = q.full->factor_nonzeros;
    proof.factorizations = q.full->factorizations;
    proof.modified_pivots = q.full->modified_pivots;
    proof.free_fill_peak = q.full->peak_free_fill;
    proof.last_free_vertices = q.full->last_free_vertices;
  }
  if (report)
    *report = proof;
  return out;
}
} // namespace dome::detail
