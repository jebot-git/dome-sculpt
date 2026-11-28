// SPDX-License-Identifier: GPL-3.0-or-later
#if defined(__HIPCC__)
#include <hip/hip_runtime.h>
#define DOME_GPU(name) hip##name
#else
#include <cuda_runtime.h>
#define DOME_GPU(name) cuda##name
#endif
#include "bake-material-device-job.hpp"
#include "bake-cage-ray-kernel.hpp"
#include "bake-raster-device-job.hpp"
#include "bake-ray-device-query.hpp"
#include "paint-composite-kernel.hpp"

namespace dome::detail {
namespace bake_rays {
namespace {
void checked(DOME_GPU(Error_t) status){
  if(status!=DOME_GPU(Success))throw std::runtime_error(DOME_GPU(GetErrorString)(status));
}
struct ReciprocalLength {
  __host__ __device__ float operator()(float squared) const {
    return paint_compute::normal_reciprocal(paint_compute::normal_length(squared));
  }
};

__global__ void ambient_rays(SurfaceView surface,const Frame *frames,const Vec3 *pattern,size_t samples,
                             float maximum,uint32_t *states,size_t rays,uint32_t *remaining,bool first){
  const size_t index=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(index>=rays)return;
  const uint32_t start=first?0:states[index];
  if(start>=surface.node_count)return;
  const auto next=trace<surface_ray::NativePrecision>(surface,frames[index/samples],
      pattern[index%samples],maximum,start,ReciprocalLength{});
  states[index]=next;
  if(next<surface.node_count)atomicExch(remaining,1);
}

__global__ void ambient_counts(const uint32_t *states,size_t samples,uint16_t *counts,size_t size){
  const size_t index=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(index<size)counts[index]=count_blocked_rays(states+index*samples,samples);
}

template<bool Edges>
__global__ void project_rays(SurfaceView surface,const ProjectionRay *rays,ProjectionHit *hits,
    ProjectionProgress<double> *progress,size_t count,uint32_t budget,uint32_t *status,bool first){
  const size_t i=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(i>=count)return;
  using Precision=surface_ray::NativePrecision;
  const uint32_t flags=Edges?trace_boundary<Precision>(surface,rays[i],budget,first,progress[i],hits[i]):
                             trace_projection<Precision>(surface,rays[i],budget,first,progress[i],hits[i]);
  if(flags)atomicOr(status,flags);
}

struct ClaimTexel {
  __host__ __device__ void operator()(uint32_t *owner,uint32_t candidate) const {
#if defined(__CUDA_ARCH__) || defined(__HIP_DEVICE_COMPILE__)
    atomicMin(owner,candidate);
#endif
  }
};
struct AddCount {
  uint32_t *counter;
  __host__ __device__ uint32_t operator()(uint32_t count) const {
#if defined(__CUDA_ARCH__) || defined(__HIP_DEVICE_COMPILE__)
    return atomicAdd(counter,count);
#else
    return 0;
#endif
  }
};

__global__ void raster_bake(bake_raster::View view,const bake_raster::Span *spans,uint32_t span_count,
                            uint32_t count,uint32_t first,bake_raster::Sample *output){
  const uint32_t slot=blockIdx.x*blockDim.x+threadIdx.x;
  if(slot<count)output[slot]=bake_raster::raster<surface_ray::NativePrecision>(view,spans,span_count,slot,first,ClaimTexel{});
}
__global__ void compact_bake(bake_raster::View view,const bake_raster::Sample *candidates,uint32_t count,
    uint32_t first,uint32_t blocks,bake_raster::Block *ranges,bake_raster::Sample *output,bake_raster::Totals *totals){
  const uint32_t block=blockIdx.x*blockDim.x+threadIdx.x;
  if(block<blocks)bake_raster::compact(view,candidates,count,first,block,ranges,output,
                                      AddCount{&totals->samples},AddCount{&totals->overlaps});
}

__global__ void prepare_cage(const bake_raster::ProjectionFace *faces,bake_raster::ProjectionSettings settings,
    const bake_raster::Sample *samples,size_t count,ProjectionRay *rays,uint32_t *status){
  const size_t i=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(i>=count)return;
  const auto &sample=samples[i];
  if(!bake_raster::prepare_ray(faces[sample.span],sample,settings,rays[i])){
    rays[i]={{},{0,1,0},0,0};
    atomicOr(status,invalid_ray);
  }
}

__global__ void bake_material_samples(bake_material::View view,const bake_material::Coordinates *coordinates,
    const paint_compute::Quantization *table,bake_material::Values *values,size_t count){
  const size_t i=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(i<count)values[i]=bake_material::evaluate(view,coordinates[i],*table);
}
__global__ void bake_overlay_samples(bake_material::OverlayView view,const bake_material::Coordinates *coordinates,
                                    bake_material::Values *values,size_t count){
  const size_t i=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(i>=count)return;
  auto value=values[i];
  bake_material::apply_overlay(view,coordinates[i],value);
  values[i]=value;
}

class GpuApi {
  DOME_GPU(Stream_t) stream_{};
public:
  using Precision=surface_ray::NativePrecision;
  using Progress=ProjectionProgress<Precision::Scalar>;
  explicit GpuApi(const paint_compute::MaterialAtlas * = nullptr){
    checked(DOME_GPU(StreamCreateWithFlags)(&stream_,DOME_GPU(StreamNonBlocking)));
  }
  ~GpuApi(){(void)DOME_GPU(StreamDestroy)(stream_);}
  void *allocate(size_t bytes){
    void *pointer=nullptr;
    const auto status=DOME_GPU(Malloc)(&pointer,bytes);
    if(status==DOME_GPU(ErrorMemoryAllocation)){
      (void)DOME_GPU(GetLastError)();
      throw std::bad_alloc();
    }
    checked(status);
    return pointer;
  }
  void release(void *pointer) noexcept {(void)DOME_GPU(Free)(pointer);}
  void wait(){checked(DOME_GPU(StreamSynchronize)(stream_));}
  void drain() noexcept {(void)DOME_GPU(StreamSynchronize)(stream_);}
  void upload(void *to,const void *from,size_t bytes){
    checked(DOME_GPU(MemcpyAsync)(to,from,bytes,DOME_GPU(MemcpyHostToDevice),stream_));
  }
  void read(void *to,const void *from,size_t bytes){
    checked(DOME_GPU(MemcpyAsync)(to,from,bytes,DOME_GPU(MemcpyDeviceToHost),stream_));
    wait();
  }
  void reset(uint32_t *value){checked(DOME_GPU(MemsetAsync)(value,0,sizeof(*value),stream_));}
  void clear_owners(uint32_t *owners,size_t count){
    checked(DOME_GPU(MemsetAsync)(owners,0xff,count*sizeof(*owners),stream_));
  }
  void clear_totals(bake_raster::Totals *totals){
    checked(DOME_GPU(MemsetAsync)(totals,0,sizeof(*totals),stream_));
  }
  void raster(bake_raster::View view,const bake_raster::Span *spans,uint32_t span_count,uint32_t count,
              uint32_t first,bake_raster::Sample *output){
    constexpr unsigned lanes=128;
    raster_bake<<<(count+lanes-1)/lanes,lanes,0,stream_>>>(view,spans,span_count,count,first,output);
    checked(DOME_GPU(GetLastError)());
  }
  void compact(bake_raster::View view,const bake_raster::Sample *candidates,uint32_t count,uint32_t first,
               uint32_t blocks,bake_raster::Block *ranges,bake_raster::Sample *output,bake_raster::Totals *totals){
    constexpr unsigned lanes=128;
    compact_bake<<<(blocks+lanes-1)/lanes,lanes,0,stream_>>>(view,candidates,count,first,blocks,ranges,output,totals);
    checked(DOME_GPU(GetLastError)());
  }
  void prepare_cage_rays(const bake_raster::ProjectionFace *faces,bake_raster::ProjectionSettings settings,
                         const bake_raster::Sample *samples,size_t count,ProjectionRay *rays,uint32_t *status){
    constexpr unsigned lanes=128;
    prepare_cage<<<(count+lanes-1)/lanes,lanes,0,stream_>>>(faces,settings,samples,count,rays,status);
    checked(DOME_GPU(GetLastError)());
  }
  void sample_bake_material(bake_material::View view,const bake_material::Coordinates *coordinates,
                            const paint_compute::Quantization *table,bake_material::Values *values,size_t count){
    constexpr unsigned lanes=128;
    bake_material_samples<<<(count+lanes-1)/lanes,lanes,0,stream_>>>(view,coordinates,table,values,count);
    checked(DOME_GPU(GetLastError)());
  }
  void composite_bake_overlay(bake_material::OverlayView view,const bake_material::Coordinates *coordinates,
                               bake_material::Values *values,size_t count){
    constexpr unsigned lanes=128;
    bake_overlay_samples<<<(count+lanes-1)/lanes,lanes,0,stream_>>>(view,coordinates,values,count);
    checked(DOME_GPU(GetLastError)());
  }

  void trace(SurfaceView surface,const Frame *frames,const Vec3 *pattern,size_t samples,float maximum,
             uint32_t *states,size_t rays,uint32_t *remaining,bool first){
    constexpr unsigned lanes=128;
    ambient_rays<<<(rays+lanes-1)/lanes,lanes,0,stream_>>>(surface,frames,pattern,samples,maximum,states,rays,remaining,first);
    checked(DOME_GPU(GetLastError)());
  }

  void reduce(const uint32_t *states,size_t samples,uint16_t *counts,size_t size){
    constexpr unsigned lanes=128;
    ambient_counts<<<(size+lanes-1)/lanes,lanes,0,stream_>>>(states,samples,counts,size);
    checked(DOME_GPU(GetLastError)());
  }

  void project(SurfaceView surface,const ProjectionRay *rays,ProjectionHit *hits,Progress *progress,
               size_t count,uint32_t budget,uint32_t *status,bool first,bool boundary){
    constexpr unsigned lanes=128;
    const auto groups=(count+lanes-1)/lanes;
    if(boundary)project_rays<true><<<groups,lanes,0,stream_>>>(surface,rays,hits,progress,count,budget,status,first);
    else project_rays<false><<<groups,lanes,0,stream_>>>(surface,rays,hits,progress,count,budget,status,first);
    checked(DOME_GPU(GetLastError)());
  }
};
} // namespace

std::unique_ptr<Query> upload(const Surface &surface,std::span<const Vec3> pattern,float maximum,bool projection){
  return upload_device<GpuApi>(surface,pattern,maximum,projection);
}
} // namespace bake_rays

std::unique_ptr<bake_raster::Raster> bake_raster::create(std::span<const float> axis,std::span<const uint8_t> writable){
  return create_device<bake_rays::GpuApi>(axis,writable);
}
std::unique_ptr<bake_material::Sampler> bake_material::upload(const ImageInput (&roles)[RoleCount],Parameters parameters,
    const PaintOverlayPacket *overlay,const std::function<bool()> &cancelled,const paint_compute::MaterialAtlas *atlas,
    const std::function<void(size_t)> &reserve){
  return std::make_unique<DeviceSampler<bake_rays::GpuApi>>(roles,parameters,overlay,cancelled,atlas,reserve);
}
} // namespace dome::detail
#undef DOME_GPU
