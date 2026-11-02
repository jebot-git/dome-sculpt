// SPDX-License-Identifier: GPL-3.0-or-later
#include "sparse-resample-backend.hpp"
#include "sparse-resample-native.hpp"
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
  std::vector<uint64_t> result;result.reserve(queries.size());
  for(size_t i=0;i<queries.size();++i) {
    if((i&255)==0)sparse_resample_checkpoint(cancelled);
    result.push_back(std::bit_cast<uint64_t>(sparse_resample::native_sample(grid,p,queries[i])));
  }
  sparse_resample_checkpoint(cancelled);return result;
}
uint64_t SparseFieldSampler::maximum_error(std::span<const sparse_resample::ErrorQuery> queries,
    const std::function<bool()> &cancelled) {
  const auto &grid=impl_->grid;const auto p=impl_->p;
  sparse_resample::validate_error_queries(queries,p,[&]{sparse_resample_checkpoint(cancelled);});
  double maximum=0;
  for(size_t i=0;i<queries.size();++i){
    if((i&255)==0)sparse_resample_checkpoint(cancelled);
    maximum=std::max(maximum,std::abs(sparse_resample::native_sample(grid,p,queries[i].point)-std::bit_cast<double>(queries[i].expected)));
  }
  sparse_resample_checkpoint(cancelled);return sparse_resample::checked_error(std::bit_cast<uint64_t>(maximum));
}
} // namespace dome
