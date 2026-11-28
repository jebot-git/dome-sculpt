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
__global__ void render_mapped_pixels(DeviceInput input,FiniteGrid grid,MappedSourceImage image,
    const EmissionRasterDevice::MappedSample *samples,size_t count,uint32_t *out){
  const size_t index=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(index<count)emission_raster::render_mapped(input,grid,image,samples[index],out+3*index);
}
__global__ void render_mapped_field_pixels(DeviceInput input,MappedSourceImage image,
    const EmissionRasterDevice::MappedSample *samples,size_t count,uint32_t *out){
  const size_t index=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(index<count){
    const auto sample=samples[index];
    emission_raster::render<false,false,true>(input,sample.texel,out+10*index,image,{sample.u,sample.v});
  }
}
__global__ void filter_bake_uv_pixels(Bake bake,const EmissionRasterDevice::BakeUvSample *samples,
                                      size_t count,uint32_t *out){
  const size_t index=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(index<count)filter_bake_uv(bake,samples[index],out+7*index);
}
__global__ void filter_bake_uv_map_pixels(BakeUvMaps maps,Bake bake,const EmissionRasterDevice::BakeUvSample *samples,
    size_t count,EmissionRasterDevice::BakeUvMapsPixel *out,uint32_t *emission){
  const size_t index=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(index<count){
    out[index]=filter_bake_uv_maps(maps,samples[index]);
    if(emission)filter_bake_uv(bake,samples[index],emission+7*index);
  }
}
__global__ void sample_material_sources(MappedSourceImage image,const paint_compute::Quantization *q,
    const SourceUvSampler::Coordinate *uv,size_t count,uint32_t *out){
  const size_t index=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(index<count)sample_mapped_source(image,uv[index],*q,out,index);
}
__global__ void render_material_pixels(DeviceInput input,MaterialView material,FiniteGrid grid,
    MaterialSources source,const uint32_t *texels,size_t count,paint_compute::Output *out){
  const size_t index=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(index<count)out[index]=emission_raster::render_material(input,material,grid,source,texels[index],index);
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
  void render_mapped(DeviceInput input,FiniteGrid grid,MappedSourceImage image,
      const EmissionRasterDevice::MappedSample *samples,size_t count,uint32_t *out){
    constexpr size_t block=128;
    render_mapped_pixels<<<(count+block-1)/block,block,0,stream_>>>(input,grid,image,samples,count,out);
    checked(DOME_DEVICE(GetLastError)());
  }
  void render_mapped_field(DeviceInput input,MappedSourceImage image,
      const EmissionRasterDevice::MappedSample *samples,size_t count,uint32_t *out){
    constexpr size_t block=128;
    render_mapped_field_pixels<<<(count+block-1)/block,block,0,stream_>>>(input,image,samples,count,out);
    checked(DOME_DEVICE(GetLastError)());
  }
  void filter_bake_uv(Bake bake,const EmissionRasterDevice::BakeUvSample *samples,size_t count,uint32_t *out){
    constexpr size_t block=128;
    filter_bake_uv_pixels<<<(count+block-1)/block,block,0,stream_>>>(bake,samples,count,out);
    checked(DOME_DEVICE(GetLastError)());
  }
  void filter_bake_uv_maps(BakeUvMaps maps,Bake bake,const EmissionRasterDevice::BakeUvSample *samples,
      size_t count,EmissionRasterDevice::BakeUvMapsPixel *out,uint32_t *emission){
    constexpr size_t block=128;
    filter_bake_uv_map_pixels<<<(count+block-1)/block,block,0,stream_>>>(maps,bake,samples,count,out,emission);
    checked(DOME_DEVICE(GetLastError)());
  }
  void sample_material(MappedSourceImage image,const paint_compute::Quantization *q,
      const SourceUvSampler::Coordinate *uv,size_t count,uint32_t *out){
    constexpr size_t block=128;
    sample_material_sources<<<(count+block-1)/block,block,0,stream_>>>(image,q,uv,count,out);
    checked(DOME_DEVICE(GetLastError)());
  }
  void render_material(DeviceInput input,MaterialView material,FiniteGrid grid,MaterialSources source,
      const uint32_t *texels,size_t count,paint_compute::Output *out){
    constexpr size_t block=128;
    render_material_pixels<<<(count+block-1)/block,block,0,stream_>>>(input,material,grid,source,texels,count,out);
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
std::unique_ptr<SourceMaterialRaster> upload_material(const Input &input,const MaterialInput &material,
    const std::function<bool()> &cancelled){
  return std::make_unique<Raster<Device>>(input,cancelled,&material);
}
}
