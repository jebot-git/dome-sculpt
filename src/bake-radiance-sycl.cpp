// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "emission-raster-kernel.hpp"

namespace dome::detail::emission_raster {
void launch_bake_radiance(sycl::queue &queue,DeviceInput input,MappedSourceImage image,
    const EmissionRasterDevice::BakeRadianceSample *samples,size_t count,uint32_t *out){
  queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
    render_bake_radiance(input,image,samples[id[0]],out+3*id[0]);
  });
}
} // namespace dome::detail::emission_raster
