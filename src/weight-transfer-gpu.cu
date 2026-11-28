// SPDX-License-Identifier: GPL-3.0-or-later
#if defined(__HIPCC__)
#include <hip/hip_runtime.h>
#define DOME_GPU(name) hip##name
#else
#include <cuda_runtime.h>
#define DOME_GPU(name) cuda##name
#endif
#include "weight-transfer-device-query.hpp"

namespace dome::detail::weight_transfer {
namespace {
void checked(DOME_GPU(Error_t) status){
  if(status!=DOME_GPU(Success))throw std::runtime_error(DOME_GPU(GetErrorString)(status));
}
struct Counter {
  uint32_t *address;
  __host__ __device__ uint32_t add(uint32_t value) const {
#if defined(__CUDA_ARCH__) || defined(__HIP_DEVICE_COMPILE__)
    return atomicAdd(address,value);
#else
    return 0;
#endif
  }
};
__global__ void closest_triangles(SurfaceView view,const Vec3 *points,float radius,Hit *hits,size_t count,
                                  uint32_t *visits,uint32_t work,bool all_queries_fit){
  const size_t id=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(id<count){
    Counter counter{visits};
    hits[id]=budgeted_search(view,points[id],radius,counter,work,all_queries_fit);
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
  void reset(uint32_t *value){checked(DOME_GPU(MemsetAsync)(value,0,sizeof(*value),stream_));}
  void launch(SurfaceView view,const Vec3 *points,float radius,Hit *hits,size_t count,
              uint32_t *visits,uint32_t work,bool all_queries_fit){
    constexpr unsigned lanes=128;
    closest_triangles<<<(count+lanes-1)/lanes,lanes,0,stream_>>>(view,points,radius,hits,count,visits,work,all_queries_fit);
    checked(DOME_GPU(GetLastError)());
  }
};
} // namespace

std::unique_ptr<Query> upload(const Surface &surface){return std::make_unique<DeviceQuery<GpuApi>>(surface);}
} // namespace dome::detail::weight_transfer
#undef DOME_GPU
