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
#include "emission-raster-retopo-bake-kernel.hpp"

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
__global__ void render_bake_radiance_pixels(DeviceInput input,MappedSourceImage image,
    const EmissionRasterDevice::BakeRadianceSample *samples,size_t count,uint32_t *out){
  const size_t index=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(index<count)render_bake_radiance(input,image,samples[index],out+3*index);
}
inline constexpr unsigned summary_lanes=128;
__global__ void summarize_bake_radiance_pixels(const uint32_t *radiance,size_t count,BakeRadianceSummary *summary){
  __shared__ uint32_t common[3][summary_lanes],seen[3][summary_lanes];
  const auto lane=threadIdx.x;
  uint32_t every[3]{UINT32_MAX,UINT32_MAX,UINT32_MAX},any[3]{};
  for(size_t i=lane;i<count;i+=summary_lanes)for(unsigned c=0;c<3;++c){
    const auto word=radiance[i*3+c];
    every[c]&=word;
    any[c]|=word;
  }
  for(unsigned c=0;c<3;++c){common[c][lane]=every[c];seen[c][lane]=any[c];}
  __syncthreads();
  for(unsigned stride=summary_lanes/2;stride;stride/=2){
    if(lane<stride)for(unsigned c=0;c<3;++c){
      common[c][lane]&=common[c][lane+stride];
      seen[c][lane]|=seen[c][lane+stride];
    }
    __syncthreads();
  }
  if(!lane)for(unsigned c=0;c<3;++c){summary->common[c]=common[c][0];summary->seen[c]=seen[c][0];}
}
__global__ void filter_bake_uv_pixels(Bake bake,const EmissionRasterDevice::BakeUvSample *samples,
                                      size_t count,uint32_t *out){
  const size_t index=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(index<count)filter_bake_uv(bake,samples[index],out+7*index);
}
__global__ void filter_bake_uv_map_pixels(BakeUvMaps maps,Bake bake,BakeUvRequests requests,
    size_t count,EmissionRasterDevice::BakeUvMapsPixel *out,uint32_t *emission){
  const size_t index=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(index<count){
    const auto sample=bake_uv_sample(requests,index);
    if(out)out[index]=filter_bake_uv_maps(maps,sample);
    if(emission)filter_bake_uv(bake,sample,emission+7*index);
  }
}
__global__ void filter_retopo_bake_pixels(BakeUvMaps maps,Bake bake,BakeUvRequests requests,
    const EmissionRasterDevice::RetopoBakeTexel *texels,RetopoBakeFormat format,size_t count,
    EmissionRasterDevice::RetopoBakePixel *out){
  const size_t index=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(index<count)out[index]=filter_retopo_bake(bake_uv_sample(requests,index),maps,bake,texels,format);
}
__global__ void filter_uv_layer_pixels(UvLayer layer,BakeUvGeometry geometry,
    const EmissionRasterDevice::BakeUvPoint *points,size_t count,paint_grid::Pixel *out){
  const size_t index=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(index<count)out[index]=filter_uv_layer(layer,geometry,points[index]);
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
    const auto status=DOME_DEVICE(Malloc)(&memory_.back(),bytes);
    if(status==DOME_DEVICE(ErrorMemoryAllocation)){
      (void)DOME_DEVICE(GetLastError)();
      throw std::bad_alloc();
    }
    checked(status);
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
  void render_bake_radiance(DeviceInput input,MappedSourceImage image,
      const EmissionRasterDevice::BakeRadianceSample *samples,size_t count,uint32_t *out){
    constexpr size_t block=128;
    render_bake_radiance_pixels<<<(count+block-1)/block,block,0,stream_>>>(input,image,samples,count,out);
    checked(DOME_DEVICE(GetLastError)());
  }
  void summarize_bake_radiance(const uint32_t *radiance,size_t count,BakeRadianceSummary *summary){
    summarize_bake_radiance_pixels<<<1,summary_lanes,0,stream_>>>(radiance,count,summary);
    checked(DOME_DEVICE(GetLastError)());
  }
  void filter_bake_uv(Bake bake,const EmissionRasterDevice::BakeUvSample *samples,size_t count,uint32_t *out){
    constexpr size_t block=128;
    filter_bake_uv_pixels<<<(count+block-1)/block,block,0,stream_>>>(bake,samples,count,out);
    checked(DOME_DEVICE(GetLastError)());
  }
  void filter_bake_uv_maps(BakeUvMaps maps,Bake bake,BakeUvRequests requests,
      size_t count,EmissionRasterDevice::BakeUvMapsPixel *out,uint32_t *emission){
    constexpr size_t block=128;
    filter_bake_uv_map_pixels<<<(count+block-1)/block,block,0,stream_>>>(maps,bake,requests,count,out,emission);
    checked(DOME_DEVICE(GetLastError)());
  }
  void filter_retopo_bake(BakeUvMaps maps,Bake bake,BakeUvRequests requests,
      const EmissionRasterDevice::RetopoBakeTexel *texels,RetopoBakeFormat format,size_t count,
      EmissionRasterDevice::RetopoBakePixel *out){
    constexpr size_t block=128;
    filter_retopo_bake_pixels<<<(count+block-1)/block,block,0,stream_>>>(maps,bake,requests,texels,format,count,out);
    checked(DOME_DEVICE(GetLastError)());
  }
  void filter_uv_layer(UvLayer layer,BakeUvGeometry geometry,const EmissionRasterDevice::BakeUvPoint *points,
                       size_t count,paint_grid::Pixel *out){
    constexpr size_t block=128;
    filter_uv_layer_pixels<<<(count+block-1)/block,block,0,stream_>>>(layer,geometry,points,count,out);
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
std::unique_ptr<EmissionRasterDevice> upload(const Input &input,const std::function<bool()> &cancelled,
    const std::function<void(size_t)> &reserve){
  return std::make_unique<Raster<Device>>(input,cancelled,nullptr,reserve);
}
std::unique_ptr<SourceMaterialRaster> upload_material(const Input &input,const MaterialInput &material,
    const std::function<bool()> &cancelled){
  return std::make_unique<Raster<Device>>(input,cancelled,&material);
}
}
