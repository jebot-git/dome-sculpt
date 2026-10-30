// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/mesh.hpp"
#include <span>
namespace dome::detail {
inline bool uv_triangles_overlap(const Mesh &m,std::span<const std::array<float,2>> uv,uint32_t f,uint32_t g) {
  using D = std::array<double, 2>;
  auto point = [&](uint32_t face, int k) {
    auto p = uv[m.indices[face * 3 + k]];
    return D{p[0], p[1]};
  };
  auto cross = [](D a, D b, D c) {
    return (b[0] - a[0]) * (c[1] - a[1]) - (b[1] - a[1]) * (c[0] - a[0]);
  };
  std::vector<D> poly{point(f, 0), point(f, 1), point(f, 2)};
  D b[] = {point(g, 0), point(g, 1), point(g, 2)};
  double tolerance =
      std::max(1e-16, std::min(std::abs(cross(poly[0], poly[1], poly[2])),
                               std::abs(cross(b[0], b[1], b[2]))) *
                          1e-9);
  double sign = cross(b[0], b[1], b[2]) > 0 ? 1 : -1;
  for (int k = 0; k < 3 && !poly.empty(); ++k) {
    std::vector<D> out;
    for (size_t i = 0; i < poly.size(); ++i) {
      auto a = poly[i], c = poly[(i + 1) % poly.size()];
      double da = sign * cross(b[k], b[(k + 1) % 3], a),
             dc = sign * cross(b[k], b[(k + 1) % 3], c);
      if (da >= 0)
        out.push_back(a);
      if ((da < 0) != (dc < 0)) {
        double t = da / (da - dc);
        out.push_back({a[0] + (c[0] - a[0]) * t, a[1] + (c[1] - a[1]) * t});
      }
    }
    poly = std::move(out);
  }
  // Anchor the area at one intersection vertex. The absolute-coordinate
  // shoelace cancels O(1) products around translated UV charts and can label
  // a shared edge as positive area; keep the SAME overlap tolerance.
  double sum = 0;
  for (size_t i = 1; i+1 < poly.size(); ++i)
    sum += cross(poly[0],poly[i],poly[i+1]);
  return std::abs(sum) > tolerance;
}
// One shared analytic predicate for atlas repair and the editable UV workspace.
// Buckets only prune pairs; pixel/sample coverage never authorizes overlaps.
template<class Skip,class Visit>
bool visit_uv_overlaps(const Mesh &m,std::span<const std::array<float,2>> uv,
                       Skip skip,Visit visit,const std::function<bool()> &cancelled={},
                       size_t maximum_work=50'000'000) {
  if(m.indices.size()>6'000'000 || uv.size()!=m.positions.size())
    throw std::length_error("UV overlap surface budget exceeded");
  constexpr int grid=64;size_t work=0;
  auto step=[&]{if(work++>=maximum_work)throw std::length_error("UV overlap work budget exceeded");
    if((work&511)==0&&cancelled&&cancelled())throw std::runtime_error("UV overlap validation cancelled");};
  if(cancelled&&cancelled())throw std::runtime_error("UV overlap validation cancelled");
  std::vector<std::vector<uint32_t>> buckets(grid*grid);
  std::vector<uint32_t> seen(m.indices.size()/3,UINT32_MAX);
  for(uint32_t f=0;f<m.indices.size()/3;++f) {
    step();if(skip(f))continue;
    std::array<float,2> p[3];for(int k=0;k<3;++k)p[k]=uv[m.indices[f*3+k]];
    int x0=std::clamp(int(std::min({p[0][0],p[1][0],p[2][0]})*grid),0,grid-1);
    int x1=std::clamp(int(std::max({p[0][0],p[1][0],p[2][0]})*grid),0,grid-1);
    int y0=std::clamp(int(std::min({p[0][1],p[1][1],p[2][1]})*grid),0,grid-1);
    int y1=std::clamp(int(std::max({p[0][1],p[1][1],p[2][1]})*grid),0,grid-1);
    for(int y=y0;y<=y1&&!skip(f);++y)for(int x=x0;x<=x1&&!skip(f);++x) {
      step();auto &bucket=buckets[y*grid+x];
      for(auto g:bucket) {
        step();if(skip(f))break;if(skip(g)||seen[g]==f)continue;seen[g]=f;
        if(uv_triangles_overlap(m,uv,f,g)&&!visit(f,g))return false;
      }
      if(!skip(f))bucket.push_back(f);
    }
  }
  return true;
}
} // namespace dome::detail
