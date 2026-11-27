// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "paint-composite-backend.hpp"
#include "paint-grid-filter.hpp"
#include <algorithm>
#include <memory>
namespace dome {
namespace {
struct UsmDeleter {sycl::queue *queue;template<class T>void operator()(T*p)const noexcept{if(p)try{sycl::free(p,*queue);}catch(...){}}};
}
namespace paint_compute {
std::vector<Output> composite(const Input &in,const std::function<bool()> &cancelled){
 validate(in,cancelled);const auto &table=quantization();
 sycl::queue queue{sycl::gpu_selector_v,[](sycl::exception_list errors){for(auto error:errors)std::rethrow_exception(error);},sycl::property_list{sycl::property::queue::in_order{}}};
 auto allocate=[&]<class T>(size_t count){auto p=std::unique_ptr<T,UsmDeleter>{sycl::malloc_device<T>(std::max(size_t(1),count),queue),{&queue}};if(!p)throw std::bad_alloc();return p;};
 auto layers=allocate.template operator()<Layer>(in.layers.size());auto tiles=allocate.template operator()<uint32_t>(in.tiles.size());
 auto pixels=allocate.template operator()<Pixel>(in.pixels.size());auto padding=allocate.template operator()<int32_t>(in.padding.size());
 auto quant=allocate.template operator()<Quantization>(1);auto output=allocate.template operator()<Output>(chunk_pixels);
 std::vector<Output> result(in.padding.size());
 try{
  checkpoint(cancelled);
  if(!in.layers.empty())queue.memcpy(layers.get(),in.layers.data(),in.layers.size()*sizeof(Layer));
  if(!in.tiles.empty())queue.memcpy(tiles.get(),in.tiles.data(),in.tiles.size()*sizeof(uint32_t));
  if(!in.pixels.empty())queue.memcpy(pixels.get(),in.pixels.data(),in.pixels.size()*sizeof(Pixel));
  queue.memcpy(padding.get(),in.padding.data(),in.padding.size()*sizeof(int32_t));queue.memcpy(quant.get(),&table,sizeof(table));
  const auto p=in.parameters;const auto l=layers.get();const auto t=tiles.get();const auto px=pixels.get();const auto pad=padding.get();const auto q=quant.get();const auto out=output.get();
  for(size_t first=0;first<result.size();){checkpoint(cancelled);const auto count=std::min(chunk_pixels,result.size()-first);
   queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){out[id]=compose(uint32_t(first+id[0]),p,l,t,px,pad,*q);});
   queue.memcpy(result.data()+first,out,count*sizeof(Output));queue.wait_and_throw();first+=count;
  }
  checkpoint(cancelled);
 }catch(...){try{queue.wait_and_throw();}catch(...){}throw;}
 return result;
}
}

namespace paint_grid {
struct Filter::Impl {
  sycl::queue queue{sycl::gpu_selector_v,[](sycl::exception_list errors){
    for(auto error:errors)std::rethrow_exception(error);
  },sycl::property_list{sycl::property::queue::in_order{}}};
  template<class T> std::unique_ptr<T,UsmDeleter> allocate(size_t count) {
    std::unique_ptr<T,UsmDeleter> result{sycl::malloc_device<T>(count,queue),{&queue}};
    if(!result)throw std::bad_alloc();
    return result;
  }
  std::unique_ptr<Tables,UsmDeleter> tables=allocate<Tables>(1);
  std::unique_ptr<Pixel,UsmDeleter> input=allocate<Pixel>(max_input_pixels);
  std::unique_ptr<Pixel,UsmDeleter> output=allocate<Pixel>(max_output_pixels);
  explicit Impl(const Tables &host) {
    queue.memcpy(tables.get(),&host,sizeof(host)).wait_and_throw();
  }
};
Filter::Filter(const Tables &tables):impl_(std::make_unique<Impl>(tables)){}
Filter::~Filter()=default;
void Filter::run(const Batch &batch,const Layout &layout,std::span<const Pixel> input,
                 std::span<Pixel> output,const std::function<bool()> &cancelled) {
  validate(batch,layout,input.size(),output.size());
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
}
}
