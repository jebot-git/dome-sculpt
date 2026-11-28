// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "bake-overlay-kernel.hpp"

namespace dome::detail::bake_material {
void launch_overlay(sycl::queue &queue,OverlayView view,const Uv *coordinates,SampleOutput output,size_t count){
  queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> index){
    overlay_sample(view,coordinates,output,index[0]);
  });
}
} // namespace dome::detail::bake_material
