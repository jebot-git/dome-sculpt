// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "paint-grid-filter.hpp"
#include <memory>
namespace dome {
namespace {
struct UsmDeleter {sycl::queue *queue;template<class T>void operator()(T*p)const noexcept{if(p)try{sycl::free(p,*queue);}catch(...){}}};
}
namespace paint_grid {
struct Filter::Impl {
  sycl::queue queue{sycl::gpu_selector_v,[](sycl::exception_list errors){
    for(auto error:errors)std::rethrow_exception(error);
  },sycl::property_list{sycl::property::queue::in_order{}}};
  template<class T> std::unique_ptr<T,UsmDeleter> allocate(size_t count) {
    if(!count)return {nullptr,{&queue}};
    std::unique_ptr<T,UsmDeleter> result{sycl::malloc_device<T>(count,queue),{&queue}};
    if(!result)throw std::bad_alloc();
    return result;
  }
  std::unique_ptr<Tables,UsmDeleter> tables=allocate<Tables>(1);
  const size_t mapped;
  std::unique_ptr<Pixel,UsmDeleter> input=allocate<Pixel>(mapped?mapped*4:max_input_pixels);
  std::unique_ptr<Pixel,UsmDeleter> output=allocate<Pixel>(mapped?mapped:max_output_pixels);
  std::unique_ptr<MappedTaps,UsmDeleter> taps=allocate<MappedTaps>(mapped);
  explicit Impl(const Tables &host,size_t count):mapped(mapped_capacity(count)) {
    queue.memcpy(tables.get(),&host,sizeof(host)).wait_and_throw();
  }
};
Filter::Filter(const Tables &tables,size_t count):impl_(std::make_unique<Impl>(tables,count)){}
Filter::~Filter()=default;
void Filter::run(const Batch &batch,const Layout &layout,std::span<const Pixel> input,
                 std::span<Pixel> output,const std::function<bool()> &cancelled) {
  validate(batch,layout,input.size(),output.size());
  if(impl_->mapped)throw std::logic_error("Expected a paint-grid rectangle filter");
  checkpoint(cancelled);
  auto &gpu=*impl_;
  const auto tables=gpu.tables.get();
  const auto src=gpu.input.get();
  const auto dst=gpu.output.get();
  const Batch map=batch;
  const Layout roles=layout;
  try {
    gpu.queue.memcpy(src,input.data(),input.size_bytes());
    gpu.queue.parallel_for(sycl::range<1>{output.size()},[=](sycl::id<1> id) {
      dst[id[0]]=filter(id[0],map,roles,*tables,src);
    });
    gpu.queue.memcpy(output.data(),dst,output.size_bytes());
    gpu.queue.wait_and_throw();
  }catch(...){
    try{gpu.queue.wait_and_throw();}catch(...){}
    throw;
  }
  checkpoint(cancelled);
}
void Filter::run_mapped(const Layout &layout,std::span<const Pixel> input,std::span<const MappedTaps> taps,
                        std::span<Pixel> output,const std::function<bool()> &cancelled){
  auto &gpu=*impl_;
  validate_mapped(layout,taps,input.size(),output.size(),gpu.mapped,cancelled);
  const auto tables=gpu.tables.get();
  const auto src=gpu.input.get();
  const auto dst=gpu.output.get();
  const auto map=gpu.taps.get();
  const auto roles=layout;
  try {
    if(!input.empty())gpu.queue.memcpy(src,input.data(),input.size_bytes());
    gpu.queue.memcpy(map,taps.data(),taps.size_bytes());
    gpu.queue.parallel_for(sycl::range<1>{output.size()},[=](sycl::id<1> id) {
      dst[id[0]]=filter_mapped(map[id[0]],roles,*tables,src);
    });
    gpu.queue.memcpy(output.data(),dst,output.size_bytes());
    gpu.queue.wait_and_throw();
  }catch(...){
    try{gpu.queue.wait_and_throw();}catch(...){}
    throw;
  }
  checkpoint(cancelled);
}
}
}
