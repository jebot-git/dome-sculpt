// SPDX-License-Identifier: GPL-3.0-or-later
#include "weight-transfer-sycl-memory.hpp"
#include "weight-transfer-device-query.hpp"
#include "surface-ray-softfloat.hpp"
#include "key-mirror-device-query.hpp"

namespace dome::detail::retopo_surface {
class NearestTriangles;
}

namespace dome::detail::key_mirror { class MirrorVertices; }

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
  void mirror_vertices(key_mirror::View view,size_t first,const key_mirror::Cell *cells,
      key_mirror::Metric metric,uint32_t allowance,key_mirror::Progress *progress,uint32_t *donors,
      size_t count,uint32_t *status,bool initialize){
    queue_.parallel_for<key_mirror::MirrorVertices>(sycl::range<1>{count},[=](sycl::id<1> index){
      const auto i=index[0];
      if(initialize)progress[i]={};
      const auto visited=key_mirror::visit<surface_ray::SoftPrecision>(
          view,view.rest[first+i],cells[i],metric,allowance,progress[i],donors[i]);
      using Atomic=sycl::atomic_ref<uint32_t,sycl::memory_order::relaxed,sycl::memory_scope::device,
          sycl::access::address_space::global_space>;
      if(visited&key_mirror::pending)Atomic(*status).fetch_or(key_mirror::pending);
      if(visited&~key_mirror::pending)Atomic(*status).fetch_add(visited&~key_mirror::pending);
    });
  }
  void clear_brush(float *values,size_t count){queue_.fill(values,0.f,count);}
  void brush(weight_brush::Stroke stroke,const Vec3 *points,float *previous,size_t count,
      weight_brush::Hit *hits,uint32_t *size){
    queue_.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
      const auto i=id[0];
      const auto alpha=weight_brush::changed_falloff(stroke,points[i],previous?previous+i:nullptr);
      if(alpha!=0){
        Counter used{size};
        hits[used.add(1)]={uint32_t(i),alpha};
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

namespace dome::detail::key_mirror {
std::unique_ptr<Query> upload(std::span<const Bin> bins,std::span<const uint32_t> order,
                             std::span<const Vec3> rest,std::span<const Vec3> delta){
  return std::make_unique<DeviceQuery<weight_transfer::SyclApi>>(bins,order,rest,delta);
}
}

namespace dome::detail::weight_brush {
std::unique_ptr<Query> upload(std::span<const Capsule> direct,std::span<const Capsule> reflected,bool reusable){
  return std::make_unique<DeviceQuery<weight_transfer::SyclApi>>(direct,reflected,reusable);
}
}

namespace dome::detail::retopo_surface {
std::unique_ptr<Query> upload(const weight_transfer::Surface &surface){
  return std::make_unique<DeviceQuery<weight_transfer::SyclApi>>(surface);
}

}
