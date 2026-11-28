// SPDX-License-Identifier: GPL-3.0-or-later
#if defined(__HIPCC__)
#include <hip/hip_runtime.h>
#define DOME_GPU(name) hip##name
#else
#include <cuda_runtime.h>
#define DOME_GPU(name) cuda##name
#endif
#include "volume-binding-device-solver.hpp"

namespace dome::detail::volume_device {
namespace {
void checked(DOME_GPU(Error_t) status){
  if(status!=DOME_GPU(Success))throw std::runtime_error(DOME_GPU(GetErrorString)(status));
}
size_t blocks(size_t count){return (count+lanes-1)/lanes;}

__global__ void initialize_field(float *field,size_t count){
  const size_t id=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(id<count)field[id]=infinity;
}
__global__ void scatter_seeds(float *field,const Seed *seeds,size_t count){
  const size_t id=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(id<count)field[seeds[id].node]=seeds[id].cells;
}
template<bool Track>
__global__ void propagate(View graph,const float *current,float *next,size_t count,uint32_t *flag){
  const size_t id=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  bool changed=false;
  if(id<count){
    const auto value=relax(id,graph,current);
    if constexpr(Track)changed=value<current[id];
    next[id]=value;
  }
  if constexpr(Track){
    const bool block_changed=__syncthreads_or(changed)!=0;
    if(block_changed&&threadIdx.x==0)atomicOr(flag,1u);
  }
}

class GpuApi {
  DOME_GPU(Stream_t) stream_{};
public:
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
  void reset(uint32_t *flag){checked(DOME_GPU(MemsetAsync)(flag,0,sizeof(*flag),stream_));}
  void initialize(float *field,size_t count){
    initialize_field<<<blocks(count),lanes,0,stream_>>>(field,count);
    checked(DOME_GPU(GetLastError)());
  }
  void scatter(float *field,const Seed *seeds,size_t count){
    scatter_seeds<<<blocks(count),lanes,0,stream_>>>(field,seeds,count);
    checked(DOME_GPU(GetLastError)());
  }
  void step(View graph,const float *current,float *next,size_t count,uint32_t *flag){
    if(flag)propagate<true><<<blocks(count),lanes,0,stream_>>>(graph,current,next,count,flag);
    else propagate<false><<<blocks(count),lanes,0,stream_>>>(graph,current,next,count,flag);
    checked(DOME_GPU(GetLastError)());
  }
};
} // namespace

std::unique_ptr<Solver> upload(const Graph &graph,size_t maximum_seeds){
  return std::make_unique<DeviceSolver<GpuApi>>(graph,maximum_seeds);
}
} // namespace dome::detail::volume_device
#undef DOME_GPU
