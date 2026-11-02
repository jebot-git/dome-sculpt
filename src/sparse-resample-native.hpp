// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "sparse-resample-grid.hpp"
// Compile native reference callers with contraction disabled.
namespace dome::sparse_resample {
inline double native_sample(const sparse_resample::Grid &grid,sparse_resample::Parameters p,sparse_resample::Point q) {
  using namespace sparse_resample;
  const double source=std::bit_cast<double>(p.source),target=std::bit_cast<double>(p.target),background=std::bit_cast<double>(p.background);
  const double v[]={double(q.x)*target/source,double(q.y)*target/source,double(q.z)*target/source};
  const Point lo{int32_t(std::floor(v[0])),int32_t(std::floor(v[1])),int32_t(std::floor(v[2]))};
  const double f[]={v[0]-lo.x,v[1]-lo.y,v[2]-lo.z};double value=background;
  for(int z=0;z<2;++z)for(int y=0;y<2;++y)for(int x=0;x<2;++x)
    if(const auto *cell=find(grid.cells().data(),grid.mask(),{lo.x+x,lo.y+y,lo.z+z}))
      value+=(std::bit_cast<double>(cell->value)-background)*(x?f[0]:1-f[0])*(y?f[1]:1-f[1])*(z?f[2]:1-f[2]);
  return value;
}
} // namespace dome::sparse_resample
