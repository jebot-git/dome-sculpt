// SPDX-License-Identifier: GPL-3.0-or-later
#if defined(__HIPCC__)
#include <hip/hip_runtime.h>
#define DOME_GPU(name) hip##name
#else
#include <cuda_runtime.h>
#define DOME_GPU(name) cuda##name
#endif
#include "weight-transfer-device-query.hpp"
#include "retopo-material-kernel.hpp"
#include "surface-ray-kernel.hpp"
#include "key-mirror-device-query.hpp"

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
template<class Data,class State,class Match>
__global__ void key_mirror_search(key_mirror::View view,const Vec3 *rest,const Data *data,size_t first,
    const key_mirror::Cell *cells,key_mirror::Metric metric,uint32_t allowance,State *progress,Match *matches,
    size_t count,uint32_t *status,bool initialize){
  const size_t i=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(i>=count)return;
  if(initialize)progress[i]={};
  const auto visited=key_mirror::visit<surface_ray::NativePrecision>(
      view,rest,data,first+i,cells[i],metric,allowance,progress[i],matches[i]);
  if(visited&key_mirror::pending)atomicOr(status,key_mirror::pending);
  if(visited&~key_mirror::pending)atomicAdd(status,visited&~key_mirror::pending);
}
__global__ void weight_brush_hits(weight_brush::Stroke stroke,const Vec3 *points,float *previous,size_t count,
    weight_brush::Hit *hits,uint32_t *size){
  const size_t i=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(i>=count)return;
  const auto alpha=weight_brush::changed_falloff(stroke,points[i],previous?previous+i:nullptr);
  if(alpha!=0)hits[atomicAdd(size,1)]={uint32_t(i),alpha};
}
__global__ void closest_triangles(SurfaceView view,const Vec3 *points,float radius,Hit *hits,size_t count,
                                  uint32_t *visits,uint32_t work,bool all_queries_fit){
  const size_t id=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(id<count){
    Counter counter{visits};
    hits[id]=budgeted_search(view,points[id],radius,counter,work,all_queries_fit);
  }
}
__global__ void nearest_triangles(SurfaceView view,const Vec3 *points,retopo_surface::Hit *hits,
    retopo_surface::Progress *progress,size_t count,uint32_t budget,uint32_t *status,bool first){
  const size_t i=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(i<count){
    const auto flags=retopo_surface::search<surface_ray::NativePrecision>(
        view,points[i],budget,hits[i],progress[i],first);
    if(flags)atomicOr(status,flags);
  }
}
template<bool Affine>
__global__ void transfer_triangles(SurfaceView view,const Vec3 *normals,const uint8_t *membership,
    retopo_surface::TransferMetric metric,const retopo_surface::TransferPoint *points,
    retopo_surface::TransferHit *hits,retopo_surface::Progress *progress,size_t count,
    retopo_surface::TransferStatus *status,bool first){
  const size_t i=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(i<count){
    const uint32_t before=first?0:progress[i].visited;
    const auto flags=retopo_surface::search_material<surface_ray::NativePrecision,Affine>(
        view,normals,membership,metric,points[i],hits[i],progress[i],first);
    if(flags)atomicOr(&status->flags,flags);
    const auto visited=progress[i].visited-before;
    if(visited)atomicAdd(&status->visits,visited);
  }
}

class GpuApi {
  DOME_GPU(Stream_t) stream_{};
public:
  GpuApi(){checked(DOME_GPU(StreamCreateWithFlags)(&stream_,DOME_GPU(StreamNonBlocking)));}
  ~GpuApi(){(void)DOME_GPU(StreamDestroy)(stream_);}
  void *allocate(size_t bytes){
    void *pointer=nullptr;
    const auto status=DOME_GPU(Malloc)(&pointer,bytes);
#if defined(__HIPCC__)
    const bool insufficient=status==hipErrorOutOfMemory;
#else
    const bool insufficient=status==cudaErrorMemoryAllocation;
#endif
    if(insufficient){
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
  template<class Data,class State,class Match>
  void mirror_keys(key_mirror::View view,const Vec3 *rest,const Data *data,size_t first,
      const key_mirror::Cell *cells,key_mirror::Metric metric,uint32_t allowance,State *progress,Match *matches,
      size_t count,uint32_t *status,bool initialize){
    constexpr unsigned lanes=128;
    key_mirror_search<<<(count+lanes-1)/lanes,lanes,0,stream_>>>(
        view,rest,data,first,cells,metric,allowance,progress,matches,count,status,initialize);
    checked(DOME_GPU(GetLastError)());
  }
  void clear_brush(float *values,size_t count){
    checked(DOME_GPU(MemsetAsync)(values,0,count*sizeof(float),stream_));
  }
  void brush(weight_brush::Stroke stroke,const Vec3 *points,float *previous,size_t count,
      weight_brush::Hit *hits,uint32_t *size){
    constexpr unsigned lanes=128;
    weight_brush_hits<<<(count+lanes-1)/lanes,lanes,0,stream_>>>(stroke,points,previous,count,hits,size);
    checked(DOME_GPU(GetLastError)());
  }
  void launch(SurfaceView view,const Vec3 *points,float radius,Hit *hits,size_t count,
              uint32_t *visits,uint32_t work,bool all_queries_fit){
    constexpr unsigned lanes=128;
    closest_triangles<<<(count+lanes-1)/lanes,lanes,0,stream_>>>(view,points,radius,hits,count,visits,work,all_queries_fit);
    checked(DOME_GPU(GetLastError)());
  }
  void nearest(SurfaceView view,const Vec3 *points,retopo_surface::Hit *hits,
      retopo_surface::Progress *progress,size_t count,uint32_t budget,uint32_t *status,bool first){
    constexpr unsigned lanes=128;
    nearest_triangles<<<(count+lanes-1)/lanes,lanes,0,stream_>>>(view,points,hits,progress,count,budget,status,first);
    checked(DOME_GPU(GetLastError)());
  }
  void reset_transfer(retopo_surface::TransferStatus *status){
    checked(DOME_GPU(MemsetAsync)(status,0,sizeof(*status),stream_));
  }
  void transfer(SurfaceView view,const Vec3 *normals,const uint8_t *membership,
      retopo_surface::TransferMetric metric,const retopo_surface::TransferPoint *points,
      retopo_surface::TransferHit *hits,retopo_surface::Progress *progress,size_t count,
      retopo_surface::TransferStatus *status,bool first){
    constexpr unsigned lanes=128;
    const auto blocks=(count+lanes-1)/lanes;
    if(metric.affine)transfer_triangles<true><<<blocks,lanes,0,stream_>>>(
        view,normals,membership,metric,points,hits,progress,count,status,first);
    else transfer_triangles<false><<<blocks,lanes,0,stream_>>>(
        view,normals,membership,metric,points,hits,progress,count,status,first);
    checked(DOME_GPU(GetLastError)());
  }
};
} // namespace

std::unique_ptr<Query> upload(const Surface &surface){return std::make_unique<DeviceQuery<GpuApi>>(surface);}
} // namespace dome::detail::weight_transfer

namespace dome::detail::key_mirror {
std::unique_ptr<Query<uint32_t>> upload(std::span<const Bin> bins,std::span<const uint32_t> order,
                                       std::span<const Vec3> rest,std::span<const Vec3> delta){
  return std::make_unique<VertexQuery<weight_transfer::GpuApi>>(bins,order,rest,delta,std::min(rest.size(),batch_capacity));
}
std::unique_ptr<Query<TriangleMatch>> upload_triangles(std::span<const Bin> bins,std::span<const uint32_t> order,
    std::span<const Vec3> rest,std::span<const uint32_t> indices,size_t capacity){
  return std::make_unique<TriangleQuery<weight_transfer::GpuApi>>(bins,order,rest,indices,capacity);
}
}

namespace dome::detail::weight_brush {
std::unique_ptr<Query> upload(std::span<const Capsule> direct,std::span<const Capsule> reflected,bool reusable){
  return std::make_unique<DeviceQuery<weight_transfer::GpuApi>>(direct,reflected,reusable);
}
}

namespace dome::detail::retopo_surface {
std::unique_ptr<Query> upload(const weight_transfer::Surface &surface){
  return std::make_unique<DeviceQuery<weight_transfer::GpuApi>>(surface);
}
std::unique_ptr<TransferQuery> upload_transfer(const weight_transfer::Surface &surface,
    std::span<const Vec3> normals,TransferMetric metric){
  return std::make_unique<DeviceTransferQuery<weight_transfer::GpuApi>>(surface,normals,metric);
}
}
#undef DOME_GPU
