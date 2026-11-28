// SPDX-License-Identifier: GPL-3.0-or-later
#include "weight-transfer-sycl-memory.hpp"
#include "weight-transfer-device-query.hpp"
#include "retopo-material-kernel.hpp"
#include "surface-ray-softfloat.hpp"

namespace dome::detail::retopo_surface {
template<bool Affine> class TransferTriangles;
namespace {
using weight_transfer::SurfaceView;

class SyclTransfer : public weight_transfer::SyclSurfaceMemory {
public:
  void reset_transfer(retopo_surface::TransferStatus *status){queue_.memset(status,0,sizeof(*status));}
  void transfer(SurfaceView view,const Vec3 *normals,const uint8_t *membership,
      retopo_surface::TransferMetric metric,const retopo_surface::TransferPoint *points,
      retopo_surface::TransferHit *hits,retopo_surface::Progress *progress,size_t count,
      retopo_surface::TransferStatus *status,bool first){
    const auto launch=[&]<bool Affine>(){
      queue_.parallel_for<retopo_surface::TransferTriangles<Affine>>(sycl::range<1>{count},[=](sycl::id<1> index){
        const size_t i=index[0];
        const uint32_t before=first?0:progress[i].visited;
        const auto flags=retopo_surface::search_material<surface_ray::SoftPrecision,Affine>(
            view,normals,membership,metric,points[i],hits[i],progress[i],first);
        using Atomic=sycl::atomic_ref<uint32_t,sycl::memory_order::relaxed,sycl::memory_scope::device,
                                     sycl::access::address_space::global_space>;
        if(flags)Atomic(status->flags).fetch_or(flags);
        const auto visited=progress[i].visited-before;
        if(visited)Atomic(status->visits).fetch_add(visited);
      });
    };
    if(metric.affine)launch.template operator()<true>();
    else launch.template operator()<false>();
  }
};
} // namespace

std::unique_ptr<TransferQuery> upload_transfer(const weight_transfer::Surface &surface,
    std::span<const Vec3> normals,TransferMetric metric){
  return std::make_unique<DeviceTransferQuery<SyclTransfer>>(surface,normals,metric);
}
} // namespace dome::detail::retopo_surface
