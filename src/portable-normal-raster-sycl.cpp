// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "portable-normal-raster-kernel.hpp"
#include "paint-composite-backend.hpp"
#include "dome/paint.hpp"

namespace dome::detail::portable_normal {
namespace {
struct Release {
 sycl::queue *queue;
 template<class T>void operator()(T *p) const noexcept{
  if(p)try{sycl::free(p,*queue);}catch(...){}
 }
};
class SyclRaster final:public Raster {
 sycl::queue queue_{sycl::gpu_selector_v,[](sycl::exception_list errors){
  for(auto error:errors)std::rethrow_exception(error);
 },sycl::property_list{sycl::property::queue::in_order{}}};
 template<class T>std::unique_ptr<T,Release> allocate(size_t count){
  std::unique_ptr<T,Release> p{sycl::malloc_device<T>(count,queue_),{&queue_}};
  if(!p)throw std::bad_alloc();
  return p;
 }
 std::unique_ptr<uint8_t,Release> image_;
 std::unique_ptr<paint_compute::Quantization,Release> quantization_;
 std::unique_ptr<Chart,Release> charts_=allocate<Chart>(batch_charts);
 std::unique_ptr<Sample,Release> samples_=allocate<Sample>(batch_pixels);
 std::unique_ptr<uint32_t,Release> result_=allocate<uint32_t>(batch_pixels);
 uint32_t width_,height_;
public:
 explicit SyclRaster(const Image &image):image_(allocate<uint8_t>(image.rgba.size())),
     quantization_(allocate<paint_compute::Quantization>(1)),width_(image.width),height_(image.height){
  const auto &table=paint_compute::quantization();
  queue_.memcpy(quantization_.get(),&table,sizeof(table)).wait_and_throw();
  queue_.memcpy(image_.get(),image.rgba.data(),image.rgba.size()).wait_and_throw();
 }
 void encode(std::span<const Chart> charts,std::span<const Sample> pending,std::span<uint32_t> output,
     const std::function<bool()> &cancelled) override{
  validate_batch(charts,pending,output);
  paint_compute::checkpoint(cancelled);
  const ImageView source{image_.get(),width_,height_};
  auto *frames=charts_.get();
  const auto *q=quantization_.get();
  auto *samples=samples_.get();
  auto *result=result_.get();
  try{
   queue_.memcpy(frames,charts.data(),charts.size_bytes());
   queue_.memcpy(samples,pending.data(),pending.size_bytes());
   queue_.parallel_for(sycl::range<1>{pending.size()},[=](sycl::id<1> id){
    const auto sample=samples[id[0]];
    result[id[0]]=encode_texel(frames[sample.chart],sample,source,*q);
   });
   queue_.memcpy(output.data(),result,output.size_bytes()).wait_and_throw();
  }catch(...){
   try{queue_.wait_and_throw();}catch(...){}
   throw;
  }
  paint_compute::checkpoint(cancelled);
 }
};
}
std::unique_ptr<Raster> upload(const Image &image,const std::function<bool()> &cancelled){
 paint_compute::checkpoint(cancelled);
 image.validate();
 auto raster=std::make_unique<SyclRaster>(image);
 paint_compute::checkpoint(cancelled);
 return raster;
}
}
