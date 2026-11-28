// SPDX-License-Identifier: GPL-3.0-or-later

#include <sycl/sycl.hpp>
#include "emission-raster-kernel.hpp"

namespace dome::detail::emission_raster {
void launch_texture_set(sycl::queue &queue,DeviceInput input,size_t first,size_t count,uint32_t *out,bool coefficients){
  const unsigned channels=coefficients?10:4;
  queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
    render_texture_set(input,first+id[0],out+channels*id[0],coefficients);
  });
}
}
