// SPDX-License-Identifier: GPL-3.0-or-later
#include "brush_kernel.hpp"
#include "csg-kernel.hpp"
#include "sparse-resample-backend.hpp"
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
  std::unique_ptr<CsgBox,UsmDeleter> boxes{nullptr,{&queue}};
  size_t capacity=0;
  size_t box_capacity=0;
  void reserve_boxes(size_t requested) {
    if(requested<=box_capacity)return;
    size_t count=1;while(count<requested)count*=2;
    decltype(boxes) next{sycl::malloc_device<CsgBox>(count,queue),{&queue}};
    if(!next)throw std::bad_alloc();
    boxes=std::move(next);box_capacity=count;
  }
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
void discard_device() noexcept {
  if(!device)return;
  // A failed/cancelled submission may still own an asynchronous copy to host
  // staging memory. Drain it before freeing either USM or that host storage.
  try {device->queue.wait_and_throw();}catch(...) {}
  device.reset();
}
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
    discard_device(); // Never reuse a failed queue or substitute CPU work.
    throw;
  }
  for(size_t i=0;i<samples.size();++i)samples[i].distance=result[i];
}
std::string_view compute_backend() { return "ONEAPI"; }
struct SparseFieldSampler::Impl {
  sycl::queue queue{sycl::gpu_selector_v,[](sycl::exception_list errors){for(auto error:errors)std::rethrow_exception(error);},sycl::property_list{sycl::property::queue::in_order{}}};
  std::unique_ptr<sparse_resample::Cell,UsmDeleter> cells{nullptr,{&queue}};
  std::unique_ptr<sparse_resample::Point,UsmDeleter> points{nullptr,{&queue}};
  std::unique_ptr<uint64_t,UsmDeleter> values{nullptr,{&queue}};
  sparse_resample::Parameters parameters;
  uint32_t mask;size_t capacity;bool failed=false;
  Impl(const sparse_resample::Grid &grid,sparse_resample::Parameters p,size_t count):parameters(p),mask(grid.mask()),capacity(count) {
    cells.reset(sycl::malloc_device<sparse_resample::Cell>(grid.cells().size(),queue));
    points.reset(sycl::malloc_device<sparse_resample::Point>(capacity,queue));
    values.reset(sycl::malloc_device<uint64_t>(capacity,queue));
    if(!cells||!points||!values)throw std::bad_alloc();
    try{queue.memcpy(cells.get(),grid.cells().data(),grid.cells().size_bytes());queue.wait_and_throw();}
    catch(...){try{queue.wait_and_throw();}catch(...){}throw;}
  }
  ~Impl(){try{queue.wait_and_throw();}catch(...){}}
};
SparseFieldSampler::SparseFieldSampler(const sparse_resample::Grid &grid,sparse_resample::Parameters p,size_t capacity) {
  if(!capacity||capacity>65536)throw std::invalid_argument("Invalid sparse resampling chunk capacity");
  sparse_resample::validate_queries({},p);impl_=std::make_unique<Impl>(grid,p,capacity);
}
SparseFieldSampler::~SparseFieldSampler()=default;
std::vector<uint64_t> SparseFieldSampler::sample(std::span<const sparse_resample::Point> queries,const std::function<bool()> &cancelled) {
  auto &state=*impl_;if(state.failed)throw std::runtime_error("Sparse resampling GPU session has failed");
  sparse_resample::validate_queries(queries,state.parameters,[&]{sparse_resample_checkpoint(cancelled);});
  std::vector<uint64_t> result(queries.size());
  try {
    const auto *cells=state.cells.get();auto *points=state.points.get();auto *values=state.values.get();
    const auto mask=state.mask;const auto p=state.parameters;
    for(size_t first=0;first<queries.size();) {
      sparse_resample_checkpoint(cancelled);const auto count=std::min(state.capacity,queries.size()-first);
      state.queue.memcpy(points,queries.data()+first,count*sizeof(sparse_resample::Point));
      state.queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){values[id]=sparse_resample::sample(cells,mask,points[id],p);});
      state.queue.memcpy(result.data()+first,values,count*sizeof(uint64_t));state.queue.wait_and_throw();first+=count;
    }
    sparse_resample_checkpoint(cancelled);
  }catch(...){state.failed=true;try{state.queue.wait_and_throw();}catch(...){}throw;}
  return result;
}

void apply_csg(std::span<Sample> samples,std::span<const CsgBox> boxes,float band,
               const std::function<bool()> &cancelled) {
  if(samples.empty() || boxes.empty())return;
  csg_detail::validate(samples,boxes,band,cancelled);
  std::vector<float> result(samples.size());
  try {
    if(!device)device=std::make_unique<BrushDevice>();
    size_t limit=csg_detail::chunk_limit(boxes.size());
    device->reserve(std::min(samples.size(),limit));device->reserve_boxes(boxes.size());
    auto operations=device->boxes.get();size_t operation_count=boxes.size();
    device->queue.memcpy(operations,boxes.data(),boxes.size_bytes());
    for(size_t first=0;first<samples.size();) {
      csg_detail::checkpoint(cancelled);
      size_t count=std::min({device->capacity,limit,samples.size()-first});
      auto input=device->samples.get();auto output=device->distances.get();
      device->queue.memcpy(input,samples.data()+first,count*sizeof(Sample));
      device->queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id) {
        output[id]=csg_detail::sample(input[id],operations,operation_count,band);
      });
      device->queue.memcpy(result.data()+first,output,count*sizeof(float));
      device->queue.wait_and_throw();first+=count;
    }
    csg_detail::checkpoint(cancelled);
  } catch(...) {discard_device();throw;}
  for(size_t i=0;i<samples.size();++i)samples[i].distance=result[i];
}
} // namespace dome
