// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "dome/paint-sampling.hpp"

namespace dome::detail {
inline std::array<float,2> bake_paint_coordinates(const PaintSamplingDomain *domain,
    uint32_t triangle,std::array<float,3> bary,std::array<float,2> current) {
  if(!domain)return current;
  const auto &basis=*domain->basis();
  const auto &corners=domain->source_corners();
  if(size_t(triangle)>=corners.size()/3)
    throw std::out_of_range("Bake source triangle has no retained paint coordinates");
  std::array<double,2> uv{};
  for(size_t k=0;k<3;++k){
    const auto q=basis.uv[basis.indices[corners[size_t(triangle)*3+k]]];
    for(size_t axis=0;axis<2;++axis)uv[axis]+=double(q[axis])*bary[k];
  }
  return {float(uv[0]),float(uv[1])};
}
} // namespace dome::detail
