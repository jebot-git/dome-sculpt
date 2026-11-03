// SPDX-License-Identifier: GPL-3.0-or-later
// Shared native positive surface assembly. Extracted unchanged from the pinned
// heat research assembly; alternative operators remain private controls.
#pragma once
#include "dome/mesh-processing.hpp"
#include "heat-binding.hpp"
#include <bit>
#include <map>
#include <numeric>
namespace dome::detail::weight_surface {
using D = std::array<double, 3>;
inline D point(Vec3 p) { return {p.x, p.y, p.z}; }
inline D sub(D a, D b) {
  for (int i = 0; i < 3; ++i)
    a[i] -= b[i];
  return a;
}
inline double dot(D a, D b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
inline D cross(D a, D b) {
  return {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
          a[0] * b[1] - a[1] * b[0]};
}
inline double norm(D a) { return std::sqrt(dot(a, a)); }
inline Vec3 vec(D a) { return {float(a[0]), float(a[1]), float(a[2])}; }
struct Budget {
  size_t used = 0, maximum;
  const std::function<bool()> &cancel;
  void step(size_t n = 1) {
    if (n > maximum - used)
      throw std::length_error("Bone heat work budget exceeded");
    used += n;
    if ((used & 1023) < n)
      stop();
  }
  void stop() const {
    if (cancel && cancel())
      throw std::runtime_error("Bone heat cancelled");
  }
};
struct System {
  std::vector<size_t> starts;
  std::vector<uint32_t> columns, parent, raw_to_row, surface_parent;
  std::vector<D> positions;
  std::vector<double> values, diagonal, mass, heat, ties;
  std::vector<D> normals;
  std::vector<std::array<uint64_t, 4>> seeds;
  std::vector<uint16_t> source_counts;
  size_t components = 0, negative_edges = 0;
  double removed_negative_stiffness = 0;
  void multiply(std::span<const double> x, std::span<double> out,
                Budget &budget) const {
    budget.step(diagonal.size() + columns.size());
    for (size_t i = 0; i < diagonal.size(); ++i) {
      double y = diagonal[i] * x[i];
      for (size_t j = starts[i]; j < starts[i + 1]; ++j)
        y += values[j] * x[columns[j]];
      out[i] = y;
    }
  }
};
inline System assemble(const Mesh &m, Budget &budget, HeatOperator operation,
                       std::span<const uint32_t> row_groups,
                       HeatNormals geometry_normals = HeatNormals::AreaWeighted,
                       HeatMass mass = HeatMass::Barycentric,
                       HeatEdges edge_policy = HeatEdges::CotangentHalf) {
  System s;
  const size_t raw = m.positions.size();
  s.raw_to_row.resize(raw);
  s.positions.reserve(raw);
  std::vector<uint32_t> lookup(raw, UINT32_MAX);
  for (uint32_t i = 0; i < raw; ++i) {
    budget.step();
    uint32_t key = row_groups.empty() ? i : row_groups[i];
    if (key >= raw)
      throw std::invalid_argument(
          "Bone heat virtual row group is outside the current owner");
    auto p = point(m.positions[i]);
    auto &row = lookup[key];
    if (row == UINT32_MAX) {
      row = uint32_t(s.positions.size());
      s.positions.push_back(p);
    } else if (s.positions[row] != p)
      throw std::invalid_argument(
          "Bone heat virtual row merged different exact metric positions");
    s.raw_to_row[i] = row;
  }
  const size_t n = s.positions.size();
  s.diagonal.assign(n, 0);
  s.mass.assign(n, 0);
  s.normals.resize(n);
  s.heat.assign(n, 0);
  s.ties.assign(n, 0);
  s.seeds.resize(n);
  s.parent.resize(n);
  std::iota(s.parent.begin(), s.parent.end(), 0);
  auto root = [&](uint32_t x) {
    while (x != s.parent[x]) {
      budget.step();
      s.parent[x] = s.parent[s.parent[x]];
      x = s.parent[x];
    }
    return x;
  };
  struct Edge {
    double weight = 0;
    size_t faces = 0;
  };
  std::map<std::array<uint32_t, 2>, Edge> edges;
  for (size_t t = 0; t < m.indices.size(); t += 3) {
    budget.step();
    std::array<uint32_t, 3> ids{s.raw_to_row[m.indices[t]],
                                s.raw_to_row[m.indices[t + 1]],
                                s.raw_to_row[m.indices[t + 2]]};
    std::array<D, 3> p{s.positions[ids[0]], s.positions[ids[1]],
                       s.positions[ids[2]]};
    auto normal = cross(sub(p[1], p[0]), sub(p[2], p[0]));
    double area2 = norm(normal);
    if (!(area2 > 0) || !std::isfinite(area2))
      throw std::invalid_argument(
          "Bone heat requires finite nondegenerate triangles");
    std::array<double, 3> cotangent;
    int obtuse = -1;
    for (int k = 0; k < 3; ++k) {
      budget.step(8);
      cotangent[k] =
          dot(sub(p[(k + 1) % 3], p[k]), sub(p[(k + 2) % 3], p[k])) / area2;
      if (cotangent[k] < 0)
        obtuse = k;
    }
    for (int k = 0; k < 3; ++k) {
      budget.step();
      auto id = ids[k];
      double vertex_area = area2 / 6;
      if (mass == HeatMass::MixedVoronoi) {
        budget.step(12);
        if (obtuse >= 0)
          vertex_area = area2 * (k == obtuse ? .25 : .125);
        else {
          auto a = sub(p[(k + 1) % 3], p[k]);
          auto b = sub(p[(k + 2) % 3], p[k]);
          vertex_area = .125 * (dot(a, a) * cotangent[(k + 2) % 3] +
                                dot(b, b) * cotangent[(k + 1) % 3]);
        }
      }
      if (!(vertex_area > 0) || !std::isfinite(vertex_area))
        throw std::invalid_argument(
            "Bone heat requires finite positive vertex area");
      s.mass[id] += vertex_area;
      for (int a = 0; a < 3; ++a) {
        budget.step();
        s.normals[id][a] += geometry_normals == HeatNormals::UnitFaces
                                ? normal[a] / area2
                                : normal[a];
      }
      auto a = ids[(k + 1) % 3], b = ids[(k + 2) % 3];
      if (a == b)
        throw std::invalid_argument("Bone heat triangle repeats a vertex");
      s.parent[root(a)] = root(b);
      double weight = .5 * cotangent[k];
      if (!std::isfinite(weight))
        throw std::runtime_error("Bone heat stiffness overflow");
      auto &edge = edges[{std::min(a, b), std::max(a, b)}];
      edge.weight += weight;
      ++edge.faces;
    }
  }
  s.surface_parent.resize(n);
  for (size_t i = 0; i < n; ++i) {
    budget.step();
    s.surface_parent[i] = root(uint32_t(i));
  }
  s.starts.assign(n + 1, 0);
  if (operation == HeatOperator::PositiveGraph) {
    budget.step(n);
    std::iota(s.parent.begin(), s.parent.end(), 0);
  }
  for (auto &[edge, value] : edges) {
    budget.step();
    if (edge_policy == HeatEdges::IncidentMean)
      value.weight *= 2. / double(value.faces);
    double w = value.weight;
    if (!std::isfinite(w))
      throw std::runtime_error("Bone heat edge stiffness overflow");
    if (operation == HeatOperator::PositiveGraph) {
      if (w < 0) {
        ++s.negative_edges;
        s.removed_negative_stiffness -= w;
      }
      if (w <= 0)
        continue;
      s.parent[root(edge[0])] = root(edge[1]);
    }
    ++s.starts[edge[0] + 1];
    ++s.starts[edge[1] + 1];
    s.diagonal[edge[0]] += w;
    s.diagonal[edge[1]] += w;
  }
  for (size_t i = 0; i < n; ++i) {
    budget.step();
    s.parent[i] = root(uint32_t(i));
    if (s.parent[i] == i)
      ++s.components;
    if (!(s.mass[i] > 0))
      throw std::invalid_argument(
          "Bone heat has an unreferenced or zero-area vertex");
    double length = norm(s.normals[i]);
    if (length)
      for (auto &x : s.normals[i])
        x /= length;
    s.starts[i + 1] += s.starts[i];
  }
  s.columns.resize(s.starts.back());
  s.values.resize(s.starts.back());
  auto cursor = s.starts;
  for (auto [edge, value] : edges) {
    budget.step();
    double w = value.weight;
    if (operation == HeatOperator::PositiveGraph && w <= 0)
      continue;
    auto a = cursor[edge[0]]++, b = cursor[edge[1]]++;
    s.columns[a] = edge[1];
    s.values[a] = -w;
    s.columns[b] = edge[0];
    s.values[b] = -w;
  }
  return s;
}
} // namespace dome::detail::weight_surface
