// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "bake-cage-ray-kernel.hpp"
#include "bake-ray-kernel.hpp"

namespace dome::detail::bake_raster {
void launch_cage_rays(sycl::queue &queue,const ProjectionFace *faces,ProjectionSettings settings,
    const Sample *samples,size_t count,bake_rays::ProjectionRay *rays,uint32_t *status){
  queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
    const auto &sample=samples[id[0]];
    // Leave a bounded dummy ray on invalid input. The same status read as
    // the exact pass refuses the whole batch before any result leaves VRAM.
    if(!prepare_ray(faces[sample.span],sample,settings,rays[id[0]])){
      rays[id[0]]={{},{0,1,0},0,0};
      sycl::atomic_ref<uint32_t,sycl::memory_order::relaxed,sycl::memory_scope::device,
                       sycl::access::address_space::global_space>(*status).fetch_or(bake_rays::invalid_ray);
    }
  });
}
} // namespace dome::detail::bake_raster
