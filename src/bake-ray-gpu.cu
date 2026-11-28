// SPDX-License-Identifier: GPL-3.0-or-later
#if defined(__HIPCC__)
#include <hip/hip_runtime.h>
#define DOME_GPU(name) hip##name
#else
#include <cuda_runtime.h>
#define DOME_GPU(name) cuda##name
#endif
#include "bake-ray-device-query.hpp"
#include "paint-composite-kernel.hpp"

namespace dome::detail::bake_rays {
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

class GpuApi {
  DOME_GPU(Stream_t) stream_{};
public:
  using Precision=surface_ray::NativePrecision;
  using Progress=ProjectionProgress<Precision::Scalar>;
  GpuApi(){checked(DOME_GPU(StreamCreateWithFlags)(&stream_,DOME_GPU(StreamNonBlocking)));}
  ~GpuApi(){(void)DOME_GPU(StreamDestroy)(stream_);}
  void *allocate(size_t bytes){
    void *pointer=nullptr;
    checked(DOME_GPU(Malloc)(&pointer,bytes));
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
} // namespace dome::detail::bake_rays
#undef DOME_GPU
