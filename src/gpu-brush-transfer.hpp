// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/brush.hpp"
#include <algorithm>
#include <type_traits>
#include <vector>

namespace dome::gpu_brush {
inline constexpr size_t chunk_samples=2'000'000;
static_assert(sizeof(Sample)==24 && sizeof(float)==4);
static_assert(std::is_trivially_copyable_v<Sample>);

// The runtime adapter owns vendor error checking. Both production CUDA/HIP
// and explicitly labelled test shims execute this same host transaction.
template<class T,class Api> class Buffer {
public:
  Buffer(Api &api,size_t count):api_(api) {
    void *allocation=nullptr;
    try {api_.allocate(&allocation,count*sizeof(T));data=static_cast<T*>(allocation);}
    catch(...) {if(allocation)api_.release(allocation);throw;}
  }
  ~Buffer()noexcept{if(data)api_.release(data);}
  Buffer(const Buffer&)=delete;
  T *data=nullptr;
private:
  Api &api_;
};

template<class Api> void apply(std::span<Sample> samples,const Brush &brush,Api &api) {
  static_assert(noexcept(api.release(nullptr))&&noexcept(api.drain()));
  if(samples.empty())return;
  // No caller output is passed to a runtime copy. Even a partial failed
  // readback can only change this detached, all-result staging allocation.
  std::vector<float> result(samples.size());
  const size_t capacity=std::min(samples.size(),chunk_samples);
  Buffer<Sample,Api> input(api,capacity);
  Buffer<float,Api> output(api,capacity);
  try {
    for(size_t first=0;first<samples.size();) {
      const size_t count=std::min(capacity,samples.size()-first);
      api.upload(input.data,samples.data()+first,count*sizeof(Sample));
      api.launch(input.data,output.data,count,brush);
      api.wait();
      api.readback(result.data()+first,output.data,count*sizeof(float));
      first+=count;
    }
  }catch(...) {
    // A failed runtime call may still own device work or host staging memory.
    // Drain best-effort before either buffer or the result vector unwinds.
    api.drain();
    throw;
  }
  // Scalar assignment cannot allocate or throw. Preserve every input field
  // until all chunks, including the last readback, have succeeded.
  for(size_t i=0;i<samples.size();++i)samples[i].distance=result[i];
}
} // namespace dome::gpu_brush
