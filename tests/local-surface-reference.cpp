// Frozen differential oracle: 2c89e8d src/mesh.cpp extraction, unchanged.
// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/mesh.hpp"
#include <map>
#include <set>
namespace dome {
namespace {
// The input coordinates are floats, but a fixed square-metre area cutoff
// incorrectly deletes valid small faces and opens otherwise closed isosurfaces.
// Evaluate the actual cross product in double precision; only collinear or
// coincident positions are geometrically degenerate, independent of units.
std::array<double,3> face_cross(Vec3 a,Vec3 b,Vec3 c) {
  double x=double(b.x)-a.x,y=double(b.y)-a.y,z=double(b.z)-a.z;
  double u=double(c.x)-a.x,v=double(c.y)-a.y,w=double(c.z)-a.z;
  return {y*w-z*v,z*u-x*w,x*v-y*u};
}
bool zero_face(const std::array<double,3> &n) {
  return n[0]==0 && n[1]==0 && n[2]==0;
}
}
Mesh legacy_surface_mesh(const Volume &field, const std::function<bool()> &cancelled) {
  Mesh result;
  std::set<Int3> cells;
  static constexpr Int3 corner[] = {{0, 0, 0}, {1, 0, 0}, {1, 1, 0}, {0, 1, 0},
                                    {0, 0, 1}, {1, 0, 1}, {1, 1, 1}, {0, 1, 1}};
  static constexpr int tetra[][4] = {{0, 5, 1, 6}, {0, 1, 2, 6}, {0, 2, 3, 6},
                                     {0, 3, 7, 6}, {0, 7, 4, 6}, {0, 4, 5, 6}};
  size_t visited = 0;
  for (auto [p, d] : field.voxels()) {
    if ((++visited % 512) == 0 && cancelled && cancelled())
      throw std::runtime_error("Meshing cancelled");
    if (d < 0)
      for (auto c : corner)
        cells.insert(p - c);
  }
  using Edge = std::pair<Int3, Int3>;
  std::map<Edge, uint32_t> vertices;
  auto vertex = [&](Int3 a, Int3 b, float da, float db) {
    if (da == 0)
      b = a;
    else if (db == 0)
      a = b;
    Edge edge = a < b ? Edge{a, b} : Edge{b, a};
    auto found = vertices.find(edge);
    if (found != vertices.end())
      return found->second;
    Vec3 pa{a.x * field.voxel_size(), a.y * field.voxel_size(),
            a.z * field.voxel_size()},
        pb{b.x * field.voxel_size(), b.y * field.voxel_size(),
           b.z * field.voxel_size()};
    float t = da == db ? 0 : std::clamp(da / (da - db), 0.f, 1.f);
    Vec3 position = pa + (pb - pa) * t;
    uint32_t index = uint32_t(result.positions.size());
    vertices.emplace(edge, index);
    result.positions.push_back(position);
    result.normals.push_back(field.gradient(position));
    return index;
  };
  auto triangle = [&](uint32_t a, uint32_t b, uint32_t c, Vec3 outward) {
    if (a == b || b == c || a == c)
      return;
    auto n=face_cross(result.positions[a],result.positions[b],result.positions[c]);
    if (zero_face(n))
      return;
    if (n[0]*outward.x+n[1]*outward.y+n[2]*outward.z < 0)
      std::swap(b, c);
    result.indices.insert(result.indices.end(), {a, b, c});
  };
  for (auto cell : cells) {
    if ((++visited % 512) == 0 && cancelled && cancelled())
      throw std::runtime_error("Meshing cancelled");
    Int3 p[8];
    float d[8];
    for (int i = 0; i < 8; ++i) {
      p[i] = cell + corner[i];
      d[i] = field.at(p[i]);
    }
    for (auto &tet : tetra) {
      int inside[4], outside[4], ni = 0, no = 0;
      Vec3 in{}, out{};
      for (auto v : tet) {
        // Winding depends only on relative corner positions. Large grid
        // coordinates must not erase the centroid difference through rounding.
        Vec3 point{float(p[v].x-cell.x), float(p[v].y-cell.y), float(p[v].z-cell.z)};
        if (d[v] < 0) {
          inside[ni++] = v;
          in += point;
        } else {
          outside[no++] = v;
          out += point;
        }
      }
      if (!ni || !no)
        continue;
      Vec3 outward = out / float(no) - in / float(ni);
      auto edge = [&](int a, int b) { return vertex(p[a], p[b], d[a], d[b]); };
      if (ni == 1)
        triangle(edge(inside[0], outside[0]), edge(inside[0], outside[1]),
                 edge(inside[0], outside[2]), outward);
      else if (no == 1)
        triangle(edge(outside[0], inside[0]), edge(outside[0], inside[1]),
                 edge(outside[0], inside[2]), outward);
      else {
        uint32_t a = edge(inside[0], outside[0]),
                 b = edge(inside[0], outside[1]),
                 c = edge(inside[1], outside[1]),
                 e = edge(inside[1], outside[0]);
        triangle(a, b, c, outward);
        triangle(a, c, e, outward);
      }
    }
  }
  // Some exact-zero intersections can become unused after degenerate triangles
  // are discarded. Compact before handing topology to UV/rigging algorithms.
  std::vector<uint32_t> remap(result.positions.size(), UINT32_MAX);
  Mesh compact;
  for (auto &index : result.indices) {
    if (remap[index] == UINT32_MAX) {
      remap[index] = uint32_t(compact.positions.size());
      compact.positions.push_back(result.positions[index]);
      compact.normals.push_back(result.normals[index]);
    }
    compact.indices.push_back(remap[index]);
  }
  return compact;
}
} // namespace dome
