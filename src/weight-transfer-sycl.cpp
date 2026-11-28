// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "weight-transfer-device-query.hpp"
#include "surface-ray-softfloat.hpp"

namespace dome::detail::retopo_surface {
class NearestTriangles;
}

namespace dome::detail::weight_transfer {
namespace {
struct Counter {
  uint32_t *address;
  uint32_t add(uint32_t value) const {
    return sycl::atomic_ref<uint32_t,sycl::memory_order::relaxed,sycl::memory_scope::device,
      sycl::access::address_space::global_space>(*address).fetch_add(value);
  }
};

class SyclApi {
  sycl::queue queue_{sycl::gpu_selector_v,[](sycl::exception_list errors){
    for(auto error:errors)std::rethrow_exception(error);
  },sycl::property_list{sycl::property::queue::in_order{}}};
public:
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
  void launch(SurfaceView view,const Vec3 *points,float radius,Hit *hits,size_t count,
              uint32_t *visits,uint32_t work,bool all_queries_fit){
    queue_.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
      Counter counter{visits};
      hits[id[0]]=budgeted_search(view,points[id[0]],radius,counter,work,all_queries_fit);
    });
  }
  void nearest(SurfaceView view,const Vec3 *points,retopo_surface::Hit *hits,
      retopo_surface::Progress *progress,size_t count,uint32_t budget,uint32_t *status,bool first){
    queue_.parallel_for<retopo_surface::NearestTriangles>(sycl::range<1>{count},[=](sycl::id<1> index){
      const size_t i=index[0];
      const auto flags=retopo_surface::search<surface_ray::SoftPrecision>(
          view,points[i],budget,hits[i],progress[i],first);
      if(flags)sycl::atomic_ref<uint32_t,sycl::memory_order::relaxed,sycl::memory_scope::device,
          sycl::access::address_space::global_space>(*status).fetch_or(flags);
    });
  }
};
} // namespace

std::unique_ptr<Query> upload(const Surface &surface){return std::make_unique<DeviceQuery<SyclApi>>(surface);}
} // namespace dome::detail::weight_transfer

namespace dome::detail::retopo_surface {
std::unique_ptr<Query> upload(const weight_transfer::Surface &surface){
  return std::make_unique<DeviceQuery<weight_transfer::SyclApi>>(surface);
}
}
