// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "bake-overlay-kernel.hpp"

namespace dome::detail::bake_material {
void launch_overlay(sycl::queue &queue,OverlayView view,const Coordinates *coordinates,Values *values,size_t count){
  queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> index){
    auto value=values[index[0]];
    apply_overlay(view,coordinates[index[0]],value);
    values[index[0]]=value;
  });
}
} // namespace dome::detail::bake_material
