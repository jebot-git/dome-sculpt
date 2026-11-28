// SPDX-License-Identifier: GPL-3.0-or-later

#include <sycl/sycl.hpp>
#include "emission-raster-kernel.hpp"

namespace dome::detail::emission_raster {
void launch_mapped(sycl::queue &queue,DeviceInput input,FiniteGrid grid,MappedSourceImage image,
    const EmissionRasterDevice::MappedSample *samples,size_t count,uint32_t *out){
  queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
    render_mapped(input,grid,image,samples[id[0]],out+3*id[0]);
  });
}
}
