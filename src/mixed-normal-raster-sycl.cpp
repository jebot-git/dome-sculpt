// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "mixed-normal-raster-kernel.hpp"
#include "paint-composite-backend.hpp"

namespace dome::detail::mixed_normal {
namespace {
struct Release {
 sycl::queue *queue;
 template<class T>void operator()(T *p) const noexcept {
  if(p)try{sycl::free(p,*queue);}catch(...){}
 }
};
template<class T> auto allocate(sycl::queue &queue,size_t size) {
 std::unique_ptr<T,Release> storage(nullptr,{&queue});
 if(size){
  storage.reset(sycl::malloc_device<T>(size,queue));
  if(!storage)throw std::bad_alloc();
 }
 return storage;
}
}

float raster(const Input &in,std::span<uint8_t> rgba,const std::function<bool()> &cancelled) {
 paint_compute::checkpoint(cancelled);
 validate(in,rgba);
 auto queue=sycl::queue{sycl::gpu_selector_v,[](sycl::exception_list errors){
  for(auto error:errors)std::rethrow_exception(error);
 },sycl::property_list{sycl::property::queue::in_order{}}};
 const auto limit=capacity(in),groups=(limit+group_size-1)/group_size;
 const auto original=allocate<uint8_t>(queue,in.original.rgba.size());
 const auto detail=allocate<uint8_t>(queue,in.detail.rgba.size());
 const auto coverage=allocate<uint8_t>(queue,in.coverage.rgba.size());
 const auto visible=allocate<uint8_t>(queue,in.visible.size());
 const auto table=allocate<paint_compute::Quantization>(queue,1);
 const auto taps=allocate<Tap>(queue,limit);
 const auto pixels=allocate<uint8_t>(queue,4*limit);
 const auto bounds=allocate<float>(queue,groups);
 std::vector<float> bound(groups);
 const View source{view(in.original,original.get()),view(in.detail,detail.get()),
                   coverage.get(),visible.get(),in.width,in.height,in.scale};
 const auto *quantization=table.get();
 auto *tap=taps.get();
 auto *pixel=pixels.get();
 auto *result=bounds.get();
 try {
  const auto &q=paint_compute::quantization();
  queue.memcpy(original.get(),in.original.rgba.data(),in.original.rgba.size());
  queue.memcpy(detail.get(),in.detail.rgba.data(),in.detail.rgba.size());
  queue.memcpy(coverage.get(),in.coverage.rgba.data(),in.coverage.rgba.size());
  if(visible)queue.memcpy(visible.get(),in.visible.data(),in.visible.size_bytes());
  queue.memcpy(table.get(),&q,sizeof(q));
  const auto maximum=visit_stripes(in,rgba,cancelled,[&](Stripe batch,std::span<uint8_t> output){
   queue.parallel_for(sycl::range<1>{batch.scratch()},[=](sycl::id<1> id){
    const size_t i=id[0];
    tap[i]=encode(source,batch,*quantization,i,pixel+4*i);
   });
   const auto total=batch.texels(),blocks=batch.groups();
   queue.parallel_for(sycl::nd_range<1>{blocks*group_size,group_size},[=](sycl::nd_item<1> item){
    const auto i=item.get_global_linear_id();
    const auto error=i<total?certify(source,batch,tap,i):0.f;
    const auto maximum=sycl::reduce_over_group(item.get_group(),error,sycl::maximum<float>{});
    if(item.get_local_linear_id()==0)result[item.get_group_linear_id()]=maximum;
   });
   queue.memcpy(output.data(),pixel+4*size_t(batch.halo)*batch.width,output.size_bytes());
   queue.memcpy(bound.data(),result,blocks*sizeof(float)).wait_and_throw();
   return *std::max_element(bound.begin(),bound.begin()+blocks);
  });
  return maximum;
 } catch(...) {
  try{queue.wait_and_throw();}catch(...){}
  throw;
 }
}
}
