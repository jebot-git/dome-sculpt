// SPDX-License-Identifier: GPL-3.0-or-later
// Native cotangent FEM + mass-lumped visibility heat; independent per-bone PCG.
// Informed by pinned Blender meshlaplacian.cc, not a copied/runtime Blender
// solve.
#include "heat-binding.hpp"
#include "dome/mesh-processing.hpp"
#include "weight-surface-system.hpp"
#include <bit>
#include <map>
#include <numeric>
namespace dome::detail {
namespace {
using namespace weight_surface;
struct Segment {
  D a, b;
  uint32_t bone;
};
D closest(D p, const Segment &s) {
  auto d = sub(s.b, s.a);
  double length = dot(d, d),
         t = length ? std::clamp(dot(sub(p, s.a), d) / length, 0., 1.) : 0;
  D q = s.a;
  for (int k = 0; k < 3; ++k)
    q[k] += t * d[k];
  return q;
}
void seed(System &s, const Mesh &m, const Skeleton &rig,
          const HeatBindingOptions &o, Budget &budget,
          std::span<const HeatSourceSegment> sources,
          std::span<const uint8_t> eligible, bool all_visible = false) {
  auto world = rig.world();
  std::vector<Segment> segments;
  segments.reserve(sources.empty() ? rig.bones.size() : sources.size());
  if (sources.empty()) {
    for (uint32_t i = 0; i < rig.bones.size(); ++i)
      if (rig.bones[i].deform && (eligible.empty() || eligible[i])) {
        budget.step();
        const auto &w = world[i];
        D a{w[12], w[13], w[14]}, b = a;
        for (int k = 0; k < 3; ++k)
          b[k] += double(w[4 + k]) * rig.bones[i].length;
        segments.push_back({a, b, i});
      }
  } else
    for (auto &s : sources) {
      budget.step();
      segments.push_back({s.head, s.tail, s.bone});
    }
  if (segments.empty())
    throw std::invalid_argument("Bone heat needs a deforming source bone");
  SurfaceQuery query(m, [&] {
    budget.step();
    return false;
  });
  SurfaceQueryOptions qo;
  qo.cancelled = [&] {
    budget.step();
    return false;
  };
  D ray_direction{};
  size_t active_row = 0;
  auto facing = [&](uint32_t triangle) {
    budget.step(16);
    size_t at = size_t(triangle) * 3;
    auto a = point(m.positions[m.indices[at]]),
         b = point(m.positions[m.indices[at + 1]]),
         c = point(m.positions[m.indices[at + 2]]);
    auto normal = cross(sub(b, a), sub(c, a));
    double value = dot(normal, ray_direction), scale = 0;
    for (int k = 0; k < 3; ++k)
      scale += std::abs(normal[k] * ray_direction[k]);
    double error = 16 * std::numeric_limits<double>::epsilon() * scale;
    return value > error ? 1 : value < -error ? -1 : 0;
  };
  qo.triangle_filter = [&](uint32_t triangle) {
    budget.step();
    if (o.occlusion == HeatOcclusion::ConnectedSurface &&
        s.surface_parent[s.raw_to_row[m.indices[size_t(triangle) * 3]]] !=
            s.surface_parent[active_row])
      return false;
    if (o.visibility == HeatVisibility::AllFaces)
      return true;
    for (int k = 0; k < 3; ++k) {
      budget.step();
      if (s.raw_to_row[m.indices[size_t(triangle) * 3 + k]] == active_row)
        return false;
    }
    // Surface bone heat tests entry occluders, not closed-material path
    // membership. Exit faces are ignored as in the pinned heat reference;
    // ambiguous grazing remains conservative. Volume FMM is unaffected.
    return facing(triangle) <= 0;
  };
  struct Trace {
    size_t entry = 0, exit = 0, grazing = 0, own = 0, foreign = 0;
  };
  std::vector<Trace> trace(s.positions.size());
  struct Candidate {
    double distance = INFINITY, physical = 0;
    D target{};
    uint32_t bone = 0;
  };
  std::vector<Candidate> candidates;
  candidates.reserve(segments.size());
  std::vector<uint8_t> anchored(s.positions.size());
  const size_t bones = rig.bones.size();
  if (o.seed_rule == HeatSeedRule::ReferenceSegments)
    s.source_counts.assign(s.positions.size() * bones, 0);
  const double tie_epsilon =
      o.seed_rule == HeatSeedRule::ReferenceSegments ? 1e-4 : 1e-6;
  for (size_t i = 0; i < s.positions.size(); ++i) {
    budget.step();
    auto p = s.positions[i];
    active_row = i;
    candidates.clear();
    for (auto &source : segments) {
      budget.step();
      auto q = closest(p, source), d = sub(p, q);
      double length = norm(d);
      if (length > o.maximum_seed_distance)
        continue;
      double cosine = length ? dot(d, s.normals[i]) / length : 1;
      // The outward-normal reference presumes an oriented boundary containing
      // its bone. An explicitly selected surface domain does not infer inside
      // from cloth/lining orientation or penalize its genuinely nearer source.
      double distance =
          o.distance_rule == HeatDistanceRule::EuclideanSurface
              ? length
              : length / (.5 * (std::clamp(cosine, -1., 1.) + 1.001));
      candidates.push_back({distance, length, q, source.bone});
    }
    std::sort(candidates.begin(), candidates.end(),
              [&](const Candidate &a, const Candidate &b) {
                budget.step();
                return a.distance != b.distance ? a.distance < b.distance
                                                : a.bone < b.bone;
              });
    double nearest =
        candidates.empty() ? INFINITY : candidates.front().distance;
    size_t ties = 0;
    for (const auto &c : candidates) {
      budget.step();
      if (!all_visible && c.distance > nearest * (1 + tie_epsilon))
        break;
      auto d = sub(c.target, p);
      bool visible = true;
      if (c.physical > 0) {
        // Entry-face mode rejects the exact incident faces structurally. Do
        // not displace its origin: even a tiny bias can jump a genuine nearby
        // entry wall belonging to this same connected surface.
        double trim = o.visibility == HeatVisibility::EntryFaces
                          ? 0
                          : std::min(c.physical * 1e-5, 1e-5);
        D start = p;
        for (int k = 0; k < 3; ++k)
          start[k] += trim * d[k] / c.physical;
        ray_direction = d;
        auto hit = query.raycast(
            vec(start), vec(d), float(std::max(0., c.physical - 2 * trim)), qo);
        visible = !hit;
        if (hit) {
          int side = facing(hit->triangle);
          auto &record = trace[s.parent[i]];
          if (s.surface_parent
                  [s.raw_to_row[m.indices[size_t(hit->triangle) * 3]]] ==
              s.surface_parent[i])
            ++record.own;
          else
            ++record.foreign;
          if (side < 0)
            ++record.entry;
          else if (side > 0)
            ++record.exit;
          else
            ++record.grazing;
        }
      }
      if (!visible)
        continue;
      nearest = std::min(nearest, c.distance);
      auto mask = uint64_t(1) << (c.bone % 64);
      if (o.seed_rule == HeatSeedRule::ReferenceSegments) {
        // The reference counts source segments, then adds their independently
        // solved fields back to their owning bone. Aggregate the same linear
        // right-hand sides without inventing a second bone identity.
        ++s.source_counts[i * bones + c.bone];
        s.seeds[i][c.bone / 64] |= mask;
        ++ties;
      } else if (!(s.seeds[i][c.bone / 64] & mask)) {
        s.seeds[i][c.bone / 64] |= mask;
        ++ties;
      }
    }
    if (ties) {
      double distance = std::max(nearest, 1e-4);
      s.heat[i] = s.mass[i] * o.heat_strength * ties / (distance * distance);
      s.ties[i] = double(ties);
      if (!std::isfinite(s.heat[i]) || !(s.heat[i] > 0))
        throw std::runtime_error("Bone heat source strength overflow");
      s.diagonal[i] += s.heat[i];
      anchored[s.parent[i]] = 1;
    }
    if (!(s.diagonal[i] > 0) || !std::isfinite(s.diagonal[i]))
      throw std::runtime_error(
          "Bone heat has a nonpositive finite-element diagonal");
  }
  for (size_t i = 0; i < s.parent.size(); ++i) {
    budget.step();
    if (s.parent[i] == i && !anchored[i]) {
      size_t rows = 0;
      D lo{INFINITY, INFINITY, INFINITY}, hi{-INFINITY, -INFINITY, -INFINITY};
      for (size_t row = 0; row < s.parent.size(); ++row) {
        budget.step();
        if (s.parent[row] != i)
          continue;
        ++rows;
        for (int k = 0; k < 3; ++k) {
          lo[k] = std::min(lo[k], s.positions[row][k]);
          hi[k] = std::max(hi[k], s.positions[row][k]);
        }
      }
      const auto &t = trace[i];
      throw HeatSeedFailure(i, rows, t.entry, t.exit, t.grazing, t.own,
                            t.foreign, lo, hi);
    }
  }
}
struct Solve {
  size_t iterations = 0;
  double residual = 0, minimum = 0;
};
double reference_limit(double weight) {
  if (weight < .025)
    return 0;
  if (weight < .05)
    return (weight - .025) * 2;
  return weight;
}
Solve solve(const System &s, std::span<const double> rhs, std::span<double> x,
            const HeatBindingOptions &o, Budget &budget) {
  const size_t n = x.size();
  std::vector<double> r(rhs.begin(), rhs.end()), z(n), p(n), ap(n);
  std::fill(x.begin(), x.end(), 0);
  double scale = 0, rho = 0;
  budget.step(n * 3);
  for (size_t i = 0; i < n; ++i) {
    scale += rhs[i] * rhs[i];
    z[i] = r[i] / s.diagonal[i];
    p[i] = z[i];
    rho += r[i] * z[i];
  }
  if (!scale)
    return {};
  scale = std::sqrt(scale);
  Solve result;
  for (uint32_t iteration = 0; iteration < o.maximum_iterations; ++iteration) {
    budget.stop();
    s.multiply(p, ap, budget);
    double curvature = 0;
    budget.step(n);
    for (size_t i = 0; i < n; ++i)
      curvature += p[i] * ap[i];
    if (!(curvature > 0) || !std::isfinite(curvature) || !std::isfinite(rho))
      throw std::runtime_error("Bone heat PCG lost positive curvature");
    double alpha = rho / curvature, residual = 0;
    budget.step(n);
    for (size_t i = 0; i < n; ++i) {
      x[i] += alpha * p[i];
      r[i] -= alpha * ap[i];
      residual += r[i] * r[i];
    }
    result.iterations = iteration + 1;
    if (std::sqrt(residual) <= o.relative_tolerance * scale ||
        iteration + 1 == o.maximum_iterations) {
      s.multiply(x, ap, budget);
      residual = 0;
      budget.step(n);
      for (size_t i = 0; i < n; ++i) {
        r[i] = rhs[i] - ap[i];
        residual += r[i] * r[i];
      }
      result.residual = std::sqrt(residual) / scale;
      if (result.residual <= o.relative_tolerance) {
        for (double v : x) {
          budget.step();
          if (!std::isfinite(v))
            throw std::runtime_error("Bone heat produced a nonfinite field");
          result.minimum = std::min(result.minimum, v);
        }
        return result;
      }
      if (iteration + 1 == o.maximum_iterations)
        break;
      budget.step(n);
      rho = 0;
      for (size_t i = 0; i < n; ++i) {
        z[i] = r[i] / s.diagonal[i];
        p[i] = z[i];
        rho += r[i] * z[i];
      }
      continue;
    }
    double next = 0;
    budget.step(n);
    for (size_t i = 0; i < n; ++i) {
      z[i] = r[i] / s.diagonal[i];
      next += r[i] * z[i];
    }
    double beta = next / rho;
    budget.step(n);
    for (size_t i = 0; i < n; ++i)
      p[i] = z[i] + beta * p[i];
    rho = next;
  }
  throw std::runtime_error(
      "Bone heat PCG did not meet its measured residual bound");
}
struct BoundSolve {
  size_t sweeps = 0;
  double residual = 0;
};
// A separately selected convex energy, not a recovery path after an error.
// Standard FEM L is PSD; visible H anchors every component. Each coordinate
// update exactly minimizes .5*x'Ax-b'x over [0,1]. An unconstrained PCG result
// is only its initial guess. Acceptance requires the actual bound KKT residual.
BoundSolve solve_bounds(const System &s, std::span<const double> rhs,
                        std::span<double> x, const HeatBindingOptions &o,
                        Budget &budget) {
  budget.stop();
  const size_t n = x.size();
  std::vector<double> product(n);
  double scale = 0;
  budget.step(n);
  for (size_t i = 0; i < n; ++i) {
    if (!std::isfinite(x[i]))
      throw std::runtime_error("Bounded heat has a nonfinite initial field");
    x[i] = std::clamp(x[i], 0., 1.);
    scale += rhs[i] * rhs[i];
  }
  scale = scale ? std::sqrt(scale) : 1;
  BoundSolve result;
  auto inspect = [&] {
    s.multiply(x, product, budget);
    budget.step(n);
    double squared = 0, energy = 0;
    for (size_t i = 0; i < n; ++i) {
      double gradient = product[i] - rhs[i];
      double violation = x[i] == 0   ? std::min(gradient, 0.)
                         : x[i] == 1 ? std::max(gradient, 0.)
                                     : gradient;
      squared += violation * violation;
      energy += .5 * x[i] * product[i] - rhs[i] * x[i];
    }
    result.residual = std::sqrt(squared) / scale;
    if (!std::isfinite(result.residual) || !std::isfinite(energy))
      throw std::runtime_error("Bounded heat has nonfinite KKT/energy");
    return energy;
  };
  double energy = inspect();
  if (result.residual <= o.relative_tolerance)
    return result;
  for (uint32_t iteration = 0; iteration < o.maximum_iterations; ++iteration) {
    budget.stop();
    budget.step(n + s.columns.size());
    for (size_t i = 0; i < n; ++i) {
      double numerator = rhs[i];
      for (size_t j = s.starts[i]; j < s.starts[i + 1]; ++j)
        numerator -= s.values[j] * x[s.columns[j]];
      x[i] = std::clamp(numerator / s.diagonal[i], 0., 1.);
    }
    ++result.sweeps;
    if (iteration % 8 == 0 || iteration + 1 == o.maximum_iterations) {
      double next = inspect();
      if (next > energy + 1e-12 * std::max(1., std::abs(energy)))
        throw std::runtime_error("Bounded heat coordinate energy increased");
      energy = next;
      if (result.residual <= o.relative_tolerance)
        return result;
    }
  }
  throw std::runtime_error("Bounded heat did not meet its measured KKT bound");
}
} // namespace
SkinWeights heat_bind_surface(const Mesh &m, const Skeleton &rig,
                              const HeatBindingOptions &o,
                              const std::function<bool()> &cancelled,
                              HeatBindingReport *report,
                              std::span<const HeatSourceSegment> sources,
                              std::span<const uint8_t> eligible,
                              std::span<const uint32_t> row_groups) {
  Budget budget{0, o.maximum_work, cancelled};
  budget.stop();
  const size_t n = m.positions.size(), nt = m.indices.size() / 3;
  if ((o.operation != HeatOperator::FiniteElements &&
       o.operation != HeatOperator::PositiveGraph &&
       o.operation != HeatOperator::BoundedFiniteElements) ||
      (o.operation == HeatOperator::BoundedFiniteElements &&
       o.edges != HeatEdges::CotangentHalf) ||
      (o.visibility != HeatVisibility::AllFaces &&
       o.visibility != HeatVisibility::EntryFaces) ||
      (o.occlusion != HeatOcclusion::WholeMesh &&
       o.occlusion != HeatOcclusion::ConnectedSurface) ||
      (o.geometry_normals != HeatNormals::AreaWeighted &&
       o.geometry_normals != HeatNormals::UnitFaces) ||
      (o.mass != HeatMass::Barycentric && o.mass != HeatMass::MixedVoronoi) ||
      (o.edges != HeatEdges::CotangentHalf &&
       o.edges != HeatEdges::IncidentMean) ||
      (o.seed_rule != HeatSeedRule::DistinctBones &&
       o.seed_rule != HeatSeedRule::ReferenceSegments) ||
      (o.output != HeatOutput::Raw && o.output != HeatOutput::ReferenceLimit) ||
      (o.distance_rule != HeatDistanceRule::OutwardNormal &&
       o.distance_rule != HeatDistanceRule::EuclideanSurface) ||
      !o.maximum_work || o.maximum_work > 500'000'000 ||
      o.maximum_bytes < 4096 || o.maximum_bytes > 512u * 1024u * 1024u ||
      !o.maximum_iterations || o.maximum_iterations > 4096 ||
      !std::isfinite(o.maximum_seed_distance) || o.maximum_seed_distance <= 0 ||
      o.maximum_seed_distance > 10000 || !std::isfinite(o.heat_strength) ||
      o.heat_strength <= 0 || o.heat_strength > 100 ||
      !std::isfinite(o.relative_tolerance) || o.relative_tolerance < 1e-12 ||
      o.relative_tolerance > 1e-5 || !n || n > 2'000'000 || !nt ||
      nt > 2'000'000 || rig.bones.empty() || rig.bones.size() > 256)
    throw std::invalid_argument("Invalid bone heat input/resource bounds");
  size_t planned = 0;
  auto bytes = [&](size_t count, size_t stride) {
    if (count > (o.maximum_bytes - planned) / stride)
      throw std::length_error(
          "Bone heat memory budget exceeded before construction");
    planned += count * stride;
  };
  bytes(n, 576 + 2 * (sizeof(VertexWeights) +
                      size_t(o.limits.max_influences) * sizeof(Influence)));
  bytes(nt, 544);
  bytes(rig.bones.size() * rig.bones.size(), 64);
  bytes(rig.bones.size(), 512);
  if (o.seed_rule == HeatSeedRule::ReferenceSegments)
    bytes(n, rig.bones.size() * sizeof(uint16_t));
  if (o.operation == HeatOperator::BoundedFiniteElements)
    bytes(n, sizeof(double));
  bytes(1, 8192);
  budget.step(n + m.indices.size() + rig.bones.size() * rig.bones.size());
  budget.step(m.indices.size() *
              (4 * std::bit_width(m.indices.size() + 1) + 1));
  m.validate();
  rig.validate();
  normalize_weights({{0, 1}}, rig.bones.size(), {}, o.limits);
  if (!eligible.empty() && (eligible.size() != rig.bones.size() ||
                            std::any_of(eligible.begin(), eligible.end(),
                                        [](uint8_t b) { return b > 1; })))
    throw std::invalid_argument(
        "Bone heat source eligibility dimensions/values differ from exact "
        "joint identities");
  if (!sources.empty()) {
    if (sources.size() > rig.bones.size() * 2)
      throw std::length_error(
          "Bone heat source graph exceeds its bounded segment count");
    std::array<bool, 256> covered{};
    for (auto &source : sources) {
      budget.step();
      if (source.bone >= rig.bones.size() ||
          (!rig.bones[source.bone].deform ||
           (!eligible.empty() && !eligible[source.bone])))
        throw std::invalid_argument("Bone heat source graph has an "
                                    "invalid/nondeforming joint identity");
      for (double x : source.head)
        if (!std::isfinite(x))
          throw std::invalid_argument("Bone heat source head is nonfinite");
      for (double x : source.tail)
        if (!std::isfinite(x))
          throw std::invalid_argument("Bone heat source tail is nonfinite");
      covered[source.bone] = true;
    }
    for (size_t i = 0; i < rig.bones.size(); ++i)
      if (rig.bones[i].deform && (eligible.empty() || eligible[i]) &&
          !covered[i])
        throw std::invalid_argument(
            "Bone heat source graph omitted a deforming bone");
  }
  if (!row_groups.empty() && row_groups.size() != n)
    throw std::invalid_argument("Bone heat virtual row groups differ from the "
                                "exact current vertex owner");
  auto s = assemble(m, budget, o.operation, row_groups, o.geometry_normals,
                    o.mass, o.edges);
  const size_t rows = s.positions.size();
  seed(s, m, rig, o, budget, sources, eligible);
  HeatBindingReport proof;
  proof.vertices = n;
  proof.matrix_vertices = rows;
  proof.components = s.components;
  proof.planned_bytes = planned;
  proof.negative_edges = s.negative_edges;
  proof.removed_negative_stiffness = s.removed_negative_stiffness;
  std::vector<double> rhs(rows), field(rows), sum(rows);
  SkinWeights grouped;
  grouped.vertices.resize(rows);
  for (auto &row : grouped.vertices)
    row.reserve(o.limits.max_influences + 1);
  for (uint32_t bone = 0; bone < rig.bones.size(); ++bone)
    if (rig.bones[bone].deform && (eligible.empty() || eligible[bone])) {
      budget.step(rows);
      bool active = false;
      for (size_t i = 0; i < rows; ++i) {
        bool selected = s.seeds[i][bone / 64] & (uint64_t(1) << (bone % 64));
        const unsigned multiplicity =
            s.source_counts.empty()
                ? 1
                : s.source_counts[i * rig.bones.size() + bone];
        rhs[i] = selected ? multiplicity * s.heat[i] / s.ties[i] : 0;
        active |= selected;
      }
      if (!active)
        continue;
      ++proof.seeded_bones;
      auto solved = solve(s, rhs, field, o, budget);
      proof.iterations += solved.iterations;
      proof.maximum_relative_residual =
          std::max(proof.maximum_relative_residual, solved.residual);
      proof.minimum_unclamped_weight =
          std::min(proof.minimum_unclamped_weight, solved.minimum);
      if (o.operation == HeatOperator::BoundedFiniteElements) {
        auto bounded = solve_bounds(s, rhs, field, o, budget);
        proof.bound_sweeps += bounded.sweeps;
        proof.maximum_kkt_residual =
            std::max(proof.maximum_kkt_residual, bounded.residual);
      } else if (solved.minimum < -1e-7)
        throw HeatPositivityFailure(bone, solved.minimum);
      for (size_t i = 0; i < rows; ++i) {
        budget.step();
        double value = std::max(0., field[i]);
        sum[i] += value;
        if (o.output == HeatOutput::ReferenceLimit)
          value = reference_limit(value);
        if (value <= 0)
          continue;
        auto &row = grouped.vertices[i];
        row.push_back({bone, float(value)});
        std::sort(row.begin(), row.end(),
                  [](const Influence &a, const Influence &b) {
                    return a.weight != b.weight ? a.weight > b.weight
                                                : a.joint < b.joint;
                  });
        if (row.size() > o.limits.max_influences)
          row.pop_back();
      }
    }
  for (size_t i = 0; i < rows; ++i) {
    budget.step();
    proof.maximum_partition_error =
        std::max(proof.maximum_partition_error, std::abs(sum[i] - 1));
    grouped.vertices[i] = normalize_weights(std::move(grouped.vertices[i]),
                                            rig.bones.size(), {}, o.limits);
  }
  SkinWeights out;
  out.vertices.reserve(n);
  for (size_t i = 0; i < n; ++i) {
    budget.step();
    out.vertices.push_back(grouped.vertices[s.raw_to_row[i]]);
  }
  out.validate(n, rig.bones.size());
  budget.stop();
  proof.work = budget.used;
  if (report)
    *report = proof;
  return out;
}
} // namespace dome::detail
