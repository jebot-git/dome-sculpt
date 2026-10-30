// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/mesh.hpp"
#include <cmath>
#include <span>
namespace dome::detail {
// A derived, corner-split evaluation mesh. Original vertex/corner identities
// and raw attributes remain in the caller's immutable source primitive.
// glTF 2.0, Meshes/Morph Targets: absent NORMAL requires flat base and target
// normals; authored TANGENT and its target displacements are ignored.
inline Mesh source_flat_mesh(const Mesh &source, std::span<const Vec3> delta = {},
    const std::function<bool()> &cancelled = {}, size_t max_work = 64*1024*1024) {
  auto stop = [&] {
    if (cancelled && cancelled()) throw std::runtime_error("Flat source normals cancelled");
  };
  stop();
  source.validate();
  const auto corners = source.indices.size(), vertices = source.positions.size();
  if (!max_work || max_work > 256*1024*1024 || vertices > 2'000'000 ||
      corners > 6'000'000 || corners > max_work || vertices > max_work-corners ||
      (!delta.empty() && delta.size() != vertices))
    throw std::length_error("Flat source geometry exceeds identity or work limits");
  const size_t bytes_per_corner = 2*sizeof(Vec3)+sizeof(uint32_t)+
      (source.uv.empty()?0:sizeof(source.uv[0]))+(source.uv_pins.empty()?0:1)+
      (source.uv_seams.empty()?0:1);
  if (corners > 256*1024*1024/bytes_per_corner)
    throw std::length_error("Flat source geometry exceeds its memory budget");
  Mesh result;
  result.positions.reserve(corners);result.normals.reserve(corners);
  result.indices.reserve(corners);
  if (!source.uv.empty()) result.uv.reserve(corners);
  if (!source.uv_pins.empty()) result.uv_pins.reserve(corners);
  result.uv_seams = source.uv_seams;
  for (size_t c = 0; c < corners; c += 3) {
    if ((c & 1023) < 3) stop();
    Vec3 p[3];
    for (size_t k = 0; k < 3; ++k) {
      const auto v = source.indices[c+k];
      p[k] = source.positions[v];
      if (!delta.empty()) p[k] += delta[v];
      if (!finite(p[k])) throw std::invalid_argument("Flat source target POSITION is nonfinite");
    }
    // Use double differences/cross product; a square-metre epsilon would erase
    // valid small triangles. Normalize only after checking actual degeneracy.
    const double x=double(p[1].x)-p[0].x,y=double(p[1].y)-p[0].y,z=double(p[1].z)-p[0].z;
    const double u=double(p[2].x)-p[0].x,v=double(p[2].y)-p[0].y,w=double(p[2].z)-p[0].z;
    const double nx=y*w-z*v,ny=z*u-x*w,nz=x*v-y*u;
    const double length=std::hypot(nx,ny,nz);
    if (!std::isfinite(length) || length == 0)
      throw std::invalid_argument("Cannot derive a flat normal for a degenerate source target");
    const Vec3 normal{float(nx/length),float(ny/length),float(nz/length)};
    for (size_t k = 0; k < 3; ++k) {
      const auto source_vertex = source.indices[c+k];
      result.positions.push_back(p[k]);result.normals.push_back(normal);
      result.indices.push_back(uint32_t(c+k));
      if (!source.uv.empty()) result.uv.push_back(source.uv[source_vertex]);
      if (!source.uv_pins.empty()) result.uv_pins.push_back(source.uv_pins[source_vertex]);
    }
  }
  stop();
  return result;
}
} // namespace dome::detail
