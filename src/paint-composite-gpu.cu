// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdexcept>
#include <memory>
#include <algorithm>
#if defined(__HIPCC__)
#include <hip/hip_runtime.h>
#define DS_ALLOC hipMalloc
#define DS_FREE hipFree
#define DS_COPY hipMemcpy
#define DS_TO_DEVICE hipMemcpyHostToDevice
#define DS_TO_HOST hipMemcpyDeviceToHost
#define DS_SUCCESS hipSuccess
#define DS_LAST_ERROR hipGetLastError
#define DS_SYNC hipDeviceSynchronize
#define DS_ERROR_TEXT hipGetErrorString
#else
#include <cuda_runtime.h>
#define DS_ALLOC cudaMalloc
#define DS_FREE cudaFree
#define DS_COPY cudaMemcpy
#define DS_TO_DEVICE cudaMemcpyHostToDevice
#define DS_TO_HOST cudaMemcpyDeviceToHost
#define DS_SUCCESS cudaSuccess
#define DS_LAST_ERROR cudaGetLastError
#define DS_SYNC cudaDeviceSynchronize
#define DS_ERROR_TEXT cudaGetErrorString
#endif
#include "paint-composite-backend.hpp"
#include "paint-grid-filter.hpp"
namespace dome {namespace {
template<class E>void checked(E value){if(value!=DS_SUCCESS)throw std::runtime_error(DS_ERROR_TEXT(value));}
__global__ void paint_pixels(paint_compute::Parameters p,const paint_compute::Layer *layers,const uint32_t *tiles,
    const paint_compute::Pixel *pixels,const int32_t *padding,paint_compute::Bake bake,const paint_compute::Quantization *q,
    paint_compute::Output *out,uint8_t *normal,size_t first,size_t count){
 const size_t i=blockIdx.x*blockDim.x+threadIdx.x;
 if(i>=count)return;
 const auto at=uint32_t(first+i);
 out[i]=paint_compute::compose(at,p,layers,tiles,pixels,padding,bake,*q);
 if(normal)paint_compute::compose_normal(padding[at],bake,*q,normal+i*4);
}
template<class T> struct Buffer {
  T *data=nullptr;
  explicit Buffer(size_t count) {
    if(!count)return;
    void *allocation=nullptr;
    try{checked(DS_ALLOC(&allocation,count*sizeof(T)));data=static_cast<T*>(allocation);}
    catch(...){if(allocation)(void)DS_FREE(allocation);throw;}
  }
  ~Buffer(){if(data)(void)DS_FREE(data);}
  Buffer(const Buffer &)=delete;
};
}
namespace paint_compute {
Result composite(const Input &in,const std::function<bool()> &cancelled){
 validate(in,cancelled);const auto &q=quantization();
 Buffer<Layer> layers(std::max(size_t(1),in.layers.size()));Buffer<uint32_t> tiles(std::max(size_t(1),in.tiles.size()));
 Buffer<Pixel> pixels(std::max(size_t(1),in.pixels.size()));Buffer<int32_t> padding(in.padding.size());Buffer<Quantization> quant(1);Buffer<Output> output(chunk_pixels);
 Buffer<uint8_t> baked(in.bake.size_bytes());
 Buffer<uint8_t> normal(in.bake.normal.empty()?0:4*chunk_pixels);
 Result result;
 result.pixels.resize(in.padding.size());
 if(normal.data)result.normals.resize(4*in.padding.size());
 try{
  checkpoint(cancelled);
  if(!in.layers.empty())checked(DS_COPY(layers.data,in.layers.data(),in.layers.size()*sizeof(Layer),DS_TO_DEVICE));
  if(!in.tiles.empty())checked(DS_COPY(tiles.data,in.tiles.data(),in.tiles.size()*sizeof(uint32_t),DS_TO_DEVICE));
  if(!in.pixels.empty())checked(DS_COPY(pixels.data,in.pixels.data(),in.pixels.size()*sizeof(Pixel),DS_TO_DEVICE));
  checked(DS_COPY(padding.data,in.padding.data(),in.padding.size()*sizeof(int32_t),DS_TO_DEVICE));checked(DS_COPY(quant.data,&q,sizeof(q),DS_TO_DEVICE));
  const auto bake=in.bake.device_view(baked.data);
  if(baked.data){
   auto *target=baked.data;
   for(const auto plane:{in.bake.coverage,in.bake.color,in.bake.orm,in.bake.emission,in.bake.normal}){
    if(plane.empty())continue;
    checked(DS_COPY(target,plane.data(),plane.size_bytes(),DS_TO_DEVICE));
    target+=plane.size_bytes();
   }
  }
  for(size_t first=0;first<result.pixels.size();){
   checkpoint(cancelled);
   const auto count=std::min(chunk_pixels,result.pixels.size()-first);
   paint_pixels<<<(count+255)/256,256>>>(in.parameters,layers.data,tiles.data,pixels.data,padding.data,bake,quant.data,output.data,normal.data,first,count);
   checked(DS_LAST_ERROR());checked(DS_SYNC());
   checked(DS_COPY(result.pixels.data()+first,output.data,count*sizeof(Output),DS_TO_HOST));
   if(normal.data)checked(DS_COPY(result.normals.data()+4*first,normal.data,4*count,DS_TO_HOST));
   first+=count;
  }
  checkpoint(cancelled);
 }catch(...){(void)DS_SYNC();throw;}
 return result;
}
}

namespace paint_grid {
namespace {
__global__ void grid_pixels(Batch batch,Layout layout,const Tables *tables,
                            const Pixel *input,Pixel *output,size_t count) {
  const size_t i=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(i<count)output[i]=filter(i,batch,layout,*tables,input);
}
}
struct Filter::Impl {
  Buffer<Tables> tables{1};
  Buffer<Pixel> input{max_input_pixels},output{max_output_pixels};
  explicit Impl(const Tables &host) {
    checked(DS_COPY(tables.data,&host,sizeof(host),DS_TO_DEVICE));
  }
};
Filter::Filter(const Tables &tables):impl_(std::make_unique<Impl>(tables)){}
Filter::~Filter()=default;
void Filter::run(const Batch &batch,const Layout &layout,std::span<const Pixel> input,
                 std::span<Pixel> output,const std::function<bool()> &cancelled) {
  validate(batch,layout,input.size(),output.size());
  checkpoint(cancelled);
  auto &gpu=*impl_;
  checked(DS_COPY(gpu.input.data,input.data(),input.size_bytes(),DS_TO_DEVICE));
  constexpr unsigned block=128;
  grid_pixels<<<(output.size()+block-1)/block,block>>>(batch,layout,gpu.tables.data,
      gpu.input.data,gpu.output.data,output.size());
  checked(DS_LAST_ERROR());
  checked(DS_COPY(output.data(),gpu.output.data,output.size_bytes(),DS_TO_HOST));
  checkpoint(cancelled);
}
}
}
