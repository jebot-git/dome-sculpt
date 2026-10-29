// SPDX-License-Identifier: GPL-3.0-or-later
#include "brush_kernel.hpp"
#include <sycl/sycl.hpp>
#include <memory>
#include <vector>
namespace dome {
namespace {
struct UsmDeleter {
  sycl::queue *queue;
  template<class T> void operator()(T *p) const noexcept {
    if(p) try { sycl::free(p,*queue); } catch(...) { /* Cleanup after a lost device must not terminate the process. */ }
  }
};
struct BrushDevice {
  // No CPU fallback. Reuse one queue and bounded device storage per authoring
  // thread; queue ordering also protects storage reused by successive chunks.
  sycl::queue queue{sycl::gpu_selector_v, [](sycl::exception_list errors) {
    for(auto error:errors) std::rethrow_exception(error);
  }, sycl::property_list{sycl::property::queue::in_order{}}};
  std::unique_ptr<Sample,UsmDeleter> samples{nullptr,{&queue}};
  std::unique_ptr<float,UsmDeleter> distances{nullptr,{&queue}};
  size_t capacity=0;
  void reserve(size_t requested) {
    if(requested<=capacity)return;
    size_t next=256;
    while(next<requested)next=std::min(size_t(2'000'000),next*2);
    decltype(samples) input{sycl::malloc_device<Sample>(next,queue),{&queue}};
    decltype(distances) output{sycl::malloc_device<float>(next,queue),{&queue}};
    if(!input || !output)throw std::bad_alloc();
    samples=std::move(input);distances=std::move(output);capacity=next;
  }
};
thread_local std::unique_ptr<BrushDevice> device;
}
void apply_brush(std::span<Sample> samples, const Brush &b) {
  if (samples.empty())
    return;
  // Read back only the changed scalar. Publish after ALL chunks complete, so
  // neither an asynchronous error nor a later chunk can partially edit callers.
  std::vector<float> result(samples.size());
  try {
    if(!device)device=std::make_unique<BrushDevice>();
    device->reserve(std::min(samples.size(),size_t(2'000'000)));
    for(size_t first=0;first<samples.size();) {
      size_t count=std::min(device->capacity,samples.size()-first);
      auto input=device->samples.get();auto output=device->distances.get();
      device->queue.memcpy(input,samples.data()+first,count*sizeof(Sample));
      device->queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id) {
        output[id]=brush_sample(input[id],b);
      });
      device->queue.memcpy(result.data()+first,output,count*sizeof(float));
      first+=count;
    }
    device->queue.wait_and_throw();
  } catch(...) {
    device.reset(); // A failed queue is never silently reused or replaced by CPU work.
    throw;
  }
  for(size_t i=0;i<samples.size();++i)samples[i].distance=result[i];
}
std::string_view compute_backend() { return "ONEAPI"; }
} // namespace dome
