// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "portable-normal-raster-kernel.hpp"
#include "paint-composite-backend.hpp"
#include "dome/paint.hpp"

namespace dome::detail::portable_normal {
void launch_certificate(sycl::queue &,const Chart *,const Probe *,const paint_compute::Quantization *,
    ImageView source,ImageView emitted,Verdict *,size_t);
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
 std::unique_ptr<uint8_t,Release> emitted_{nullptr,{&queue_}};
 std::unique_ptr<Probe,Release> probes_{nullptr,{&queue_}};
 std::unique_ptr<Verdict,Release> verdicts_{nullptr,{&queue_}};
 uint32_t width_,height_;
 uint32_t emitted_width_=0,emitted_height_=0;

 template<class Input,class Output,class Launch>
 void dispatch(std::span<const Chart> charts,std::span<const Input> pending,std::span<Output> output,
     Input *samples,Output *result,const Launch &launch,const std::function<bool()> &cancelled){
  validate_batch(charts,pending,output);
  paint_compute::checkpoint(cancelled);
  auto *frames=charts_.get();
  try{
   queue_.memcpy(frames,charts.data(),charts.size_bytes());
   queue_.memcpy(samples,pending.data(),pending.size_bytes());
   launch(frames,samples,result,pending.size());
   queue_.memcpy(output.data(),result,output.size_bytes()).wait_and_throw();
  }catch(...){
   try{queue_.wait_and_throw();}catch(...){}
   throw;
  }
  paint_compute::checkpoint(cancelled);
 }
public:
 explicit SyclRaster(const Image &image):image_(allocate<uint8_t>(image.rgba.size())),
     quantization_(allocate<paint_compute::Quantization>(1)),width_(image.width),height_(image.height){
  const auto &table=paint_compute::quantization();
  queue_.memcpy(quantization_.get(),&table,sizeof(table)).wait_and_throw();
  queue_.memcpy(image_.get(),image.rgba.data(),image.rgba.size()).wait_and_throw();
 }
 void encode(std::span<const Chart> charts,std::span<const Sample> pending,std::span<uint32_t> output,
     const std::function<bool()> &cancelled) override{
  const ImageView source{image_.get(),width_,height_};
  const auto *q=quantization_.get();
  const auto launch=[&](const Chart *frames,const Sample *samples,uint32_t *result,size_t count){
   queue_.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
    const auto sample=samples[id[0]];
    result[id[0]]=encode_texel(frames[sample.chart],sample,source,*q);
   });
  };
  dispatch(charts,pending,output,samples_.get(),result_.get(),launch,cancelled);
 }
 void set_emitted_image(const Image &image,const std::function<bool()> &cancelled) override{
  paint_compute::checkpoint(cancelled);
  image.validate();
  if(!probes_)probes_=allocate<Probe>(batch_pixels);
  if(!verdicts_)verdicts_=allocate<Verdict>(batch_pixels);
  emitted_.reset();
  auto next=allocate<uint8_t>(image.rgba.size());
  queue_.memcpy(next.get(),image.rgba.data(),image.rgba.size()).wait_and_throw();
  paint_compute::checkpoint(cancelled);
  emitted_=std::move(next);
  emitted_width_=image.width;
  emitted_height_=image.height;
 }
 void certify(std::span<const Chart> charts,std::span<const Probe> pending,std::span<Verdict> output,
     const std::function<bool()> &cancelled) override{
  if(!emitted_)throw std::logic_error("Portable normal certification has no emitted image");
  const ImageView source{image_.get(),width_,height_},emitted{emitted_.get(),emitted_width_,emitted_height_};
  const auto launch=[&](const Chart *frames,const Probe *probes,Verdict *result,size_t count){
   launch_certificate(queue_,frames,probes,quantization_.get(),source,emitted,result,count);
  };
  dispatch(charts,pending,output,probes_.get(),verdicts_.get(),launch,cancelled);
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
