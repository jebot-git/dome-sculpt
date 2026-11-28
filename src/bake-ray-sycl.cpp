// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "bake-ray-device-query.hpp"
#include "paint-composite-kernel.hpp"
#include "surface-ray-softfloat.hpp"

namespace dome::detail::bake_rays {
void launch_boundary(sycl::queue &,SurfaceView,const ProjectionRay *,ProjectionHit *,
    ProjectionProgress<surface_ray::SoftScalar> *,size_t count,uint32_t budget,uint32_t *status,bool first);

namespace {
class SyclApi {
  sycl::queue queue_{sycl::gpu_selector_v,[](sycl::exception_list errors){
    for(auto error:errors)std::rethrow_exception(error);
  },sycl::property_list{sycl::property::queue::in_order{}}};
public:
  using Precision=surface_ray::SoftPrecision;
  using Progress=ProjectionProgress<Precision::Scalar>;
  void *allocate(size_t bytes){
    auto *pointer=sycl::malloc_device(bytes,queue_);
    if(!pointer)throw std::bad_alloc();
    return pointer;
  }
  void release(void *pointer) noexcept {try{sycl::free(pointer,queue_);}catch(...){}}
  void wait(){queue_.wait_and_throw();}
  void drain() noexcept {try{wait();}catch(...){}}
  void upload(void *to,const void *from,size_t bytes){queue_.memcpy(to,from,bytes);}
  void read(void *to,const void *from,size_t bytes){queue_.memcpy(to,from,bytes).wait_and_throw();}
  void reset(uint32_t *value){queue_.fill(value,uint32_t(0),1);}

  void trace(SurfaceView surface,const Frame *frames,const Vec3 *pattern,size_t samples,float maximum,
             uint32_t *states,size_t rays,uint32_t *remaining,bool first){
    queue_.parallel_for(sycl::range<1>{rays},[=](sycl::id<1> id){
      const size_t index=id[0];
      const uint32_t start=first?0:states[index];
      if(start>=surface.node_count)return;
      const auto next=bake_rays::trace<surface_ray::SoftPrecision>(surface,frames[index/samples],
          pattern[index%samples],maximum,start,[](float value){
            return paint_compute::normal_reciprocal(paint_compute::normal_length(value));
          });
      states[index]=next;
      if(next<surface.node_count)
        sycl::atomic_ref<uint32_t,sycl::memory_order::relaxed,sycl::memory_scope::device,
                         sycl::access::address_space::global_space>(*remaining).store(1);
    });
  }

  void reduce(const uint32_t *states,size_t samples,uint16_t *counts,size_t size){
    queue_.parallel_for(sycl::range<1>{size},[=](sycl::id<1> id){
      counts[id[0]]=count_blocked_rays(states+id[0]*samples,samples);
    });
  }

  void project(SurfaceView surface,const ProjectionRay *rays,ProjectionHit *hits,Progress *progress,
               size_t count,uint32_t budget,uint32_t *status,bool first,bool boundary){
    if(boundary){
      launch_boundary(queue_,surface,rays,hits,progress,count,budget,status,first);
      return;
    }
    queue_.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
      const size_t i=id[0];
      const uint32_t flags=trace_projection<Precision>(surface,rays[i],budget,first,progress[i],hits[i]);
      if(flags)
        sycl::atomic_ref<uint32_t,sycl::memory_order::relaxed,sycl::memory_scope::device,
                         sycl::access::address_space::global_space>(*status).fetch_or(flags);
    });
  }
};
} // namespace

std::unique_ptr<Query> upload(const Surface &surface,std::span<const Vec3> pattern,float maximum,bool projection){
  return upload_device<SyclApi>(surface,pattern,maximum,projection);
}
} // namespace dome::detail::bake_rays
