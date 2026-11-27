// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "paint-composite-backend.hpp"
#include <algorithm>
#include <memory>
namespace dome {
namespace {
struct UsmDeleter {sycl::queue *queue;template<class T>void operator()(T*p)const noexcept{if(p)try{sycl::free(p,*queue);}catch(...){}}};
}
namespace paint_compute {
Result composite(const Input &in,const std::function<bool()> &cancelled){
 validate(in,cancelled);const auto &table=quantization();
 sycl::queue queue{sycl::gpu_selector_v,[](sycl::exception_list errors){for(auto error:errors)std::rethrow_exception(error);},sycl::property_list{sycl::property::queue::in_order{}}};
 auto allocate=[&]<class T>(size_t count){auto p=std::unique_ptr<T,UsmDeleter>{sycl::malloc_device<T>(std::max(size_t(1),count),queue),{&queue}};if(!p)throw std::bad_alloc();return p;};
 auto layers=allocate.template operator()<Layer>(in.layers.size());auto tiles=allocate.template operator()<uint32_t>(in.tiles.size());
 auto pixels=allocate.template operator()<Pixel>(in.pixels.size());auto padding=allocate.template operator()<int32_t>(in.padding.size());
 std::unique_ptr<uint8_t,UsmDeleter> baked{nullptr,{&queue}};
 if(const auto bytes=in.bake.size_bytes())baked=allocate.template operator()<uint8_t>(bytes);
 auto quant=allocate.template operator()<Quantization>(1);auto output=allocate.template operator()<Output>(chunk_pixels);
 std::unique_ptr<uint8_t,UsmDeleter> normal_output{nullptr,{&queue}};
 Result result;
 result.pixels.resize(in.padding.size());
 if(!in.bake.normal.empty()){
  normal_output=allocate.template operator()<uint8_t>(4*chunk_pixels);
  result.normals.resize(4*in.padding.size());
 }
 try{
  checkpoint(cancelled);
  if(!in.layers.empty())queue.memcpy(layers.get(),in.layers.data(),in.layers.size()*sizeof(Layer));
  if(!in.tiles.empty())queue.memcpy(tiles.get(),in.tiles.data(),in.tiles.size()*sizeof(uint32_t));
  if(!in.pixels.empty())queue.memcpy(pixels.get(),in.pixels.data(),in.pixels.size()*sizeof(Pixel));
  queue.memcpy(padding.get(),in.padding.data(),in.padding.size()*sizeof(int32_t));queue.memcpy(quant.get(),&table,sizeof(table));
  const auto bake=in.bake.device_view(baked.get());
  if(baked){
   auto *target=baked.get();
   for(const auto plane:{in.bake.coverage,in.bake.color,in.bake.orm,in.bake.emission,in.bake.normal}){
    if(plane.empty())continue;
    queue.memcpy(target,plane.data(),plane.size_bytes());
    target+=plane.size_bytes();
   }
  }
  const auto p=in.parameters;const auto l=layers.get();const auto t=tiles.get();const auto px=pixels.get();const auto pad=padding.get();const auto q=quant.get();const auto out=output.get();
  auto *normal=normal_output.get();
  for(size_t first=0;first<result.pixels.size();){
   checkpoint(cancelled);
   const auto count=std::min(chunk_pixels,result.pixels.size()-first);
   queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
    const auto at=uint32_t(first+id[0]);
    out[id]=compose(at,p,l,t,px,pad,bake,*q);
    if(normal)compose_normal(pad[at],bake,*q,normal+4*id[0]);
   });
   queue.memcpy(result.pixels.data()+first,out,count*sizeof(Output));
   if(normal)queue.memcpy(result.normals.data()+4*first,normal,count*4);
   queue.wait_and_throw();
   first+=count;
  }
  checkpoint(cancelled);
 }catch(...){try{queue.wait_and_throw();}catch(...){}throw;}
 return result;
}
}
}
