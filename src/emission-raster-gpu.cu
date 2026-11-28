// SPDX-License-Identifier: GPL-3.0-or-later

#if defined(__HIPCC__)
#include <hip/hip_runtime.h>
#define DOME_DEVICE(name) hip##name
#define DOME_RUNTIME_ERROR hipError_t
#else
#include <cuda_runtime.h>
#define DOME_DEVICE(name) cuda##name
#define DOME_RUNTIME_ERROR cudaError_t
#endif

#include "emission-raster-upload.hpp"

namespace dome::detail::emission_raster {
namespace {
void checked(DOME_RUNTIME_ERROR status){
  if(status!=DOME_DEVICE(Success))throw std::runtime_error(DOME_DEVICE(GetErrorString)(status));
}
template<bool typed>
__global__ void render_pixels(DeviceInput input,size_t first,size_t count,uint32_t *out){
  const size_t index=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(index<count)emission_raster::render<false,typed>(input,first+index,out+10*index);
}
__global__ void render_finite_pixels(DeviceInput input,FiniteGrid grid,size_t first,size_t count,uint32_t *out){
  const size_t index=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(index<count)emission_raster::render_finite(input,grid,first+index,out+3*index);
}
template<bool mapped>
__global__ void render_texture_set_pixels(DeviceInput input,size_t first,size_t count,uint32_t *out,bool coefficients){
  const size_t index=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  const unsigned channels=coefficients?10:4;
  if(index<count)emission_raster::render_texture_set<mapped>(input,first+index,out+channels*index,coefficients);
}
class Device {
public:
  Device(){
    memory_.reserve(16);
    checked(DOME_DEVICE(StreamCreateWithFlags)(&stream_,DOME_DEVICE(StreamNonBlocking)));
  }
  ~Device(){
    (void)DOME_DEVICE(StreamSynchronize)(stream_);
    for(auto *p:memory_)if(p)(void)DOME_DEVICE(Free)(p);
    (void)DOME_DEVICE(StreamDestroy)(stream_);
  }
  void *allocate(size_t bytes){
    memory_.push_back(nullptr);
    checked(DOME_DEVICE(Malloc)(&memory_.back(),bytes));
    return memory_.back();
  }
  void upload(void *device,const void *host,size_t bytes){
    checked(DOME_DEVICE(MemcpyAsync)(device,host,bytes,DOME_DEVICE(MemcpyHostToDevice),stream_));
    checked(DOME_DEVICE(StreamSynchronize)(stream_));
  }
  void download(void *host,const void *device,size_t bytes){
    checked(DOME_DEVICE(MemcpyAsync)(host,device,bytes,DOME_DEVICE(MemcpyDeviceToHost),stream_));
    checked(DOME_DEVICE(StreamSynchronize)(stream_));
  }
  void render(DeviceInput input,size_t first,size_t count,uint32_t *out){
    constexpr size_t block=128;
    if(input.source.typed_transform)
      render_pixels<true><<<(count+block-1)/block,block,0,stream_>>>(input,first,count,out);
    else render_pixels<false><<<(count+block-1)/block,block,0,stream_>>>(input,first,count,out);
    checked(DOME_DEVICE(GetLastError)());
  }
  void render_finite(DeviceInput input,FiniteGrid grid,size_t first,size_t count,uint32_t *out){
    constexpr size_t block=128;
    render_finite_pixels<<<(count+block-1)/block,block,0,stream_>>>(input,grid,first,count,out);
    checked(DOME_DEVICE(GetLastError)());
  }
  void render_texture_set(DeviceInput input,size_t first,size_t count,uint32_t *out,bool coefficients){
    constexpr size_t block=128;
    if(input.source.coordinates.columns)
      render_texture_set_pixels<true><<<(count+block-1)/block,block,0,stream_>>>(input,first,count,out,coefficients);
    else render_texture_set_pixels<false><<<(count+block-1)/block,block,0,stream_>>>(input,first,count,out,coefficients);
    checked(DOME_DEVICE(GetLastError)());
  }
private:
  DOME_DEVICE(Stream_t) stream_{};
  std::vector<void *> memory_;
};
}
std::unique_ptr<EmissionRasterDevice> upload(const Input &input,const std::function<bool()> &cancelled){
  return std::make_unique<Raster<Device>>(input,cancelled);
}
}
