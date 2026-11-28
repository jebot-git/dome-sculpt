// SPDX-License-Identifier: GPL-3.0-or-later
#include "weight-transfer-sycl-memory.hpp"
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

class SyclApi : public SyclSurfaceMemory {
public:
  void reset(uint32_t *value){queue_.fill(value,uint32_t(0),1);}
  void brush(weight_brush::Stroke stroke,const Vec3 *points,size_t count,
      weight_brush::Hit *hits,uint32_t *size){
    queue_.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
      const auto alpha=weight_brush::strongest(stroke,points[id[0]]);
      if(alpha!=0){
        Counter used{size};
        hits[used.add(1)]={uint32_t(id[0]),alpha};
      }
    });
  }
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

namespace dome::detail::weight_brush {
std::unique_ptr<Query> upload(std::span<const Capsule> direct,std::span<const Capsule> reflected){
  return std::make_unique<DeviceQuery<weight_transfer::SyclApi>>(direct,reflected);
}
}

namespace dome::detail::retopo_surface {
std::unique_ptr<Query> upload(const weight_transfer::Surface &surface){
  return std::make_unique<DeviceQuery<weight_transfer::SyclApi>>(surface);
}

}
