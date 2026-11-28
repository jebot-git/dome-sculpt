// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "bake-ray-kernel.hpp"
#include "surface-ray-softfloat.hpp"

namespace dome::detail::bake_rays {
// Keep edge recovery in its own device module. A Bake with only exact hits
// should only compile the kernels it actually dispatches.
void launch_boundary(sycl::queue &queue,SurfaceView surface,const ProjectionRay *rays,ProjectionHit *hits,
    ProjectionProgress<surface_ray::SoftScalar> *progress,size_t count,uint32_t budget,uint32_t *status,bool first){
  queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
    const size_t i=id[0];
    const uint32_t flags=trace_boundary<surface_ray::SoftPrecision>(surface,rays[i],budget,first,progress[i],hits[i]);
    if(flags)
      sycl::atomic_ref<uint32_t,sycl::memory_order::relaxed,sycl::memory_scope::device,
                       sycl::access::address_space::global_space>(*status).fetch_or(flags);
  });
}
} // namespace dome::detail::bake_rays
