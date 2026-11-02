// SPDX-License-Identifier: GPL-3.0-or-later
#include "sparse-resample-backend.hpp"
namespace dome {
struct SparseFieldSampler::Impl {
  sparse_resample::Grid grid;
  sparse_resample::Parameters p;
  Impl(const sparse_resample::Grid &g,sparse_resample::Parameters params):grid(g),p(params){}
};
SparseFieldSampler::SparseFieldSampler(const sparse_resample::Grid &g,sparse_resample::Parameters p,size_t capacity) {
  if(!capacity||capacity>65536)throw std::invalid_argument("Invalid sparse resampling chunk capacity");
  sparse_resample::validate_queries({},p);impl_=std::make_unique<Impl>(g,p);
}
SparseFieldSampler::~SparseFieldSampler()=default;
std::vector<uint64_t> SparseFieldSampler::sample(std::span<const sparse_resample::Point> queries,
    const std::function<bool()> &cancelled) {
  const auto &grid=impl_->grid;const auto p=impl_->p;
  using namespace sparse_resample;
  validate_queries(queries,p,[&]{sparse_resample_checkpoint(cancelled);});
  const double source=std::bit_cast<double>(p.source),target=std::bit_cast<double>(p.target),background=std::bit_cast<double>(p.background);
  std::vector<uint64_t> result;result.reserve(queries.size());
  for(size_t i=0;i<queries.size();++i) {
    if((i&255)==0)sparse_resample_checkpoint(cancelled);
    const auto q=queries[i];const double v[]={double(q.x)*target/source,double(q.y)*target/source,double(q.z)*target/source};
    const Point lo{int32_t(std::floor(v[0])),int32_t(std::floor(v[1])),int32_t(std::floor(v[2]))};
    const double f[]={v[0]-lo.x,v[1]-lo.y,v[2]-lo.z};double value=background;
    for(int z=0;z<2;++z)for(int y=0;y<2;++y)for(int x=0;x<2;++x)
      if(const auto *cell=find(grid.cells().data(),grid.mask(),{lo.x+x,lo.y+y,lo.z+z}))
        value+=(std::bit_cast<double>(cell->value)-background)*(x?f[0]:1-f[0])*(y?f[1]:1-f[1])*(z?f[2]:1-f[2]);
    result.push_back(std::bit_cast<uint64_t>(value));
  }
  sparse_resample_checkpoint(cancelled);return result;
}
} // namespace dome
