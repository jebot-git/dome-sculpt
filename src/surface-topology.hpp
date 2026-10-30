// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/mesh.hpp"
#include <map>
#include <set>
#include <span>
#include <tuple>
namespace dome::surface_detail {
using Triangle = std::array<uint32_t, 3>;
inline std::array<double, 3> exact_cross(Vec3 a, Vec3 b, Vec3 c) {
  double x = double(b.x) - a.x, y = double(b.y) - a.y, z = double(b.z) - a.z;
  double u = double(c.x) - a.x, v = double(c.y) - a.y, w = double(c.z) - a.z;
  return {y * w - z * v, z * u - x * w, x * v - y * u};
}
inline bool zero(std::array<double, 3> n) {
  return n[0] == 0 && n[1] == 0 && n[2] == 0;
}
// A float-quantized zero-area triangle may have THREE distinct collinear
// positions. Dropping it leaves a T-junction on its longest boundary edge.
// Subdivide existing adjacent faces at that EXACT point instead: no coordinate
// movement, tolerance welding, area cutoff or deletion of a nonzero-area face.
inline void complete_collinear_surface(Mesh &mesh,
                                       std::span<const Triangle> collapsed,
                                       const std::function<bool()> &cancel = {},
                                       size_t work_limit = 16'000'000,
                                       size_t index_limit = 48'000'000,
                                       bool require_closed = true) {
  if (collapsed.empty())
    return;
  size_t work = 0;
  auto tick = [&] {
    if (++work > work_limit)
      throw std::length_error(
          "Surface collinear-edge repair work limit exceeded");
    if ((work & 255) == 0 && cancel && cancel())
      throw std::runtime_error("Meshing cancelled");
  };
  using Edge = std::pair<uint32_t, uint32_t>;
  auto edge = [](uint32_t a, uint32_t b) {
    return a < b ? Edge{a, b} : Edge{b, a};
  };
  std::map<Edge, std::set<uint32_t>> cuts;
  for (auto t : collapsed) {
    tick();
    int longest = 0;
    double maximum = -1;
    for (int i = 0; i < 3; ++i) {
      auto a = mesh.positions[t[i]], b = mesh.positions[t[(i + 1) % 3]];
      double x = double(a.x) - b.x, y = double(a.y) - b.y,
             z = double(a.z) - b.z;
      double squared = x * x + y * y + z * z;
      if (squared > maximum) {
        maximum = squared;
        longest = i;
      }
    }
    auto a = t[longest], b = t[(longest + 1) % 3], m = t[(longest + 2) % 3];
    if (a == b || m == a || m == b)
      continue;
    auto pa = mesh.positions[a], pb = mesh.positions[b], pm = mesh.positions[m];
    if (!zero(exact_cross(pa, pb, pm)))
      throw std::logic_error("Non-collinear surface repair request");
    if (pm.x < std::min(pa.x, pb.x) || pm.x > std::max(pa.x, pb.x) ||
        pm.y < std::min(pa.y, pb.y) || pm.y > std::max(pa.y, pb.y) ||
        pm.z < std::min(pa.z, pb.z) || pm.z > std::max(pa.z, pb.z))
      throw std::logic_error("Surface repair point is outside its edge");
    auto &points = cuts[edge(a, b)];
    points.insert(m);
    if (points.size() > 64)
      throw std::length_error("Surface edge subdivision limit exceeded");
  }
  if (cuts.empty())
    return;
  if (mesh.indices.size() > index_limit)
    throw std::length_error("Surface exact subdivision index limit exceeded");
  std::vector<uint32_t> output;
  output.reserve(mesh.indices.size());
  auto append = [&](std::span<const uint32_t> values) {
    if (values.size() > index_limit - output.size())
      throw std::length_error("Surface exact subdivision index limit exceeded");
    output.insert(output.end(), values.begin(), values.end());
  };
  std::set<Edge> used;
  for (size_t t = 0; t < mesh.indices.size(); t += 3) {
    tick();
    Triangle face{mesh.indices[t], mesh.indices[t + 1], mesh.indices[t + 2]};
    std::vector<uint32_t> polygon;
    bool changed = false;
    for (int i = 0; i < 3; ++i) {
      auto a = face[i], b = face[(i + 1) % 3];
      polygon.push_back(a);
      auto found = cuts.find(edge(a, b));
      if (found == cuts.end())
        continue;
      changed = true;
      used.insert(found->first);
      std::vector<uint32_t> points(found->second.begin(), found->second.end());
      auto pa = mesh.positions[a], pb = mesh.positions[b];
      int axis = 0;
      std::array<double, 3> direction{double(pb.x) - pa.x, double(pb.y) - pa.y,
                                      double(pb.z) - pa.z};
      if (std::abs(direction[1]) > std::abs(direction[axis]))
        axis = 1;
      if (std::abs(direction[2]) > std::abs(direction[axis]))
        axis = 2;
      auto coord = [&](uint32_t index) {
        auto p = mesh.positions[index];
        return std::array<float, 3>{p.x, p.y, p.z}[axis];
      };
      std::sort(points.begin(), points.end(), [&](auto x, auto y) {
        return direction[axis] > 0 ? coord(x) < coord(y) : coord(x) > coord(y);
      });
      polygon.insert(polygon.end(), points.begin(), points.end());
    }
    if (!changed) {
      append(face);
      continue;
    }
    const auto normal =
        exact_cross(mesh.positions[face[0]], mesh.positions[face[1]],
                    mesh.positions[face[2]]);
    // Convex planar polygon with exact collinear boundary vertices. Remove
    // nonzero ears while ensuring the remainder still has nonzero area. A fan
    // would discard collinear boundary vertices on either adjacent fan edge.
    while (polygon.size() > 3) {
      bool clipped = false;
      for (size_t i = 0; i < polygon.size(); ++i) {
        tick();
        auto a = polygon[(i + polygon.size() - 1) % polygon.size()],
             b = polygon[i], c = polygon[(i + 1) % polygon.size()];
        auto n = exact_cross(mesh.positions[a], mesh.positions[b],
                             mesh.positions[c]);
        if (n[0] * normal[0] + n[1] * normal[1] + n[2] * normal[2] <= 0)
          continue;
        bool nonzero_remainder = false;
        for (size_t j = 0; j < polygon.size(); ++j) {
          tick();
          if (j == i)
            continue;
          auto k = (j + 1) % polygon.size();
          if (k == i)
            k = (k + 1) % polygon.size();
          auto l = (k + 1) % polygon.size();
          if (l == i)
            l = (l + 1) % polygon.size();
          if (!zero(exact_cross(mesh.positions[polygon[j]],
                                mesh.positions[polygon[k]],
                                mesh.positions[polygon[l]]))) {
            nonzero_remainder = true;
            break;
          }
        }
        if (!nonzero_remainder)
          continue;
        append(Triangle{a, b, c});
        polygon.erase(polygon.begin() + i);
        clipped = true;
        break;
      }
      if (!clipped)
        throw std::runtime_error(
            "Surface exact collinear subdivision cannot preserve valid faces");
    }
    if (zero(exact_cross(mesh.positions[polygon[0]], mesh.positions[polygon[1]],
                         mesh.positions[polygon[2]])))
      throw std::runtime_error(
          "Surface collinear subdivision produced a zero-area face");
    append(polygon);
  }
  if (used.size() != cuts.size())
    throw std::runtime_error(
        "Surface collapsed edge has no adjacent face for exact subdivision");
  // Float quantization can reverse a tiny triangle's geometric cross product
  // even though its tetrahedron's topological orientation did not change.
  // Restore directed manifold adjacency, choosing each component's global sign
  // to preserve the greatest TOTAL original face area (sum over the component). No position/face is removed.
  struct HalfEdge {
    uint32_t a, b, face;
    uint8_t side, forward;
  };
  if (output.size() > index_limit || output.size() > 256 * 1024 * 1024 / 32)
    throw std::length_error("Surface exact orientation memory limit exceeded");
  std::vector<HalfEdge> halfedges;
  halfedges.reserve(output.size());
  const size_t faces = output.size() / 3;
  for (size_t f = 0; f < faces; ++f)
    for (uint8_t side = 0; side < 3; ++side) {
      tick();
      auto a = output[3 * f + side], b = output[3 * f + (side + 1) % 3];
      halfedges.push_back(
          {std::min(a, b), std::max(a, b), uint32_t(f), side, uint8_t(a < b)});
    }
  std::sort(halfedges.begin(), halfedges.end(), [&](auto a, auto b) {
    tick();
    return std::tie(a.a, a.b, a.face) < std::tie(b.a, b.b, b.face);
  });
  std::vector<std::array<uint32_t, 3>> adjacent(
      faces, {UINT32_MAX, UINT32_MAX, UINT32_MAX});
  for (size_t i = 0; i < halfedges.size();) {
    tick();
    size_t end = i + 1;
    while (end < halfedges.size() && halfedges[end].a == halfedges[i].a &&
           halfedges[end].b == halfedges[i].b)
      ++end;
    if (end - i == 1 && !require_closed) {
      i = end;
      continue;
    }
    if (end - i != 2)
      throw std::runtime_error(
          "Exact surface completion has an open or non-manifold edge; field "
          "precision/topology needs refinement");
    auto a = halfedges[i], b = halfedges[i + 1];
    uint32_t parity = a.forward == b.forward ? 0x80000000u : 0;
    adjacent[a.face][a.side] = b.face | parity;
    adjacent[b.face][b.side] = a.face | parity;
    i = end;
  }
  std::vector<int8_t> flip(faces, -1);
  std::vector<uint32_t> component;
  component.reserve(faces);
  for (size_t seed = 0; seed < faces; ++seed) {
    tick();
    if (flip[seed] >= 0)
      continue;
    component.clear();
    component.push_back(uint32_t(seed));
    flip[seed] = 0;
    double preserved = 0, reversed = 0;
    for (size_t head = 0; head < component.size(); ++head) {
      tick();
      auto f = component[head];
      auto n = exact_cross(mesh.positions[output[3 * f]],
                           mesh.positions[output[3 * f + 1]],
                           mesh.positions[output[3 * f + 2]]);
      double magnitude = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
      (flip[f] ? reversed : preserved) += magnitude;
      for (auto neighbor : adjacent[f]) {
        if (neighbor == UINT32_MAX)
          continue;
        auto other = neighbor & 0x7fffffffu;
        auto required = int8_t(flip[f] ^ int(neighbor >> 31));
        if (flip[other] < 0) {
          flip[other] = required;
          component.push_back(other);
        } else if (flip[other] != required)
          throw std::runtime_error(
              "Exact surface completion is not orientable");
      }
    }
    bool invert = reversed > preserved;
    for (auto f : component)
      if (bool(flip[f]) != invert)
        std::swap(output[3 * f + 1], output[3 * f + 2]);
  }
  mesh.indices = std::move(output);
}
} // namespace dome::surface_detail
