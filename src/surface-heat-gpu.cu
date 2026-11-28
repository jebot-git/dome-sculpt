// SPDX-License-Identifier: GPL-3.0-or-later
#if defined(__HIPCC__)
#include <hip/hip_runtime.h>
#define DOME_GPU(name) hip##name
#else
#include <cuda_runtime.h>
#define DOME_GPU(name) cuda##name
#endif
#include "surface-heat-device-fields.hpp"

namespace dome::detail::surface_heat_device {
namespace {
void checked(DOME_GPU(Error_t) status){
  if(status!=DOME_GPU(Success))throw std::runtime_error(DOME_GPU(GetErrorString)(status));
}
template<class Operation>
__global__ void apply(size_t count,Operation operation){
  const size_t id=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(id<count)operation(id);
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
  template<class Operation> void launch(size_t count,Operation operation){
    constexpr unsigned lanes=128;
    apply<<<(count+lanes-1)/lanes,lanes,0,stream_>>>(count,operation);
    checked(DOME_GPU(GetLastError)());
  }
};
} // namespace
std::unique_ptr<Fields> create(const System &input){return std::make_unique<DeviceFields<GpuApi>>(input);}
} // namespace dome::detail::surface_heat_device
#undef DOME_GPU
