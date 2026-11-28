// SPDX-License-Identifier: GPL-3.0-or-later

#include <sycl/sycl.hpp>
#include "emission-raster-kernel.hpp"

namespace dome::detail::emission_raster {
void launch_finite(sycl::queue &queue,DeviceInput input,FiniteGrid grid,size_t first,size_t count,uint32_t *out){
  queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
    render_finite(input,grid,first+id[0],out+3*id[0]);
  });
}
}
