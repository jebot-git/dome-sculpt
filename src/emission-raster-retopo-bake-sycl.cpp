// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "emission-raster-retopo-bake-kernel.hpp"

namespace dome::detail::emission_raster {
void launch_retopo_bake(sycl::queue &queue,BakeUvMaps maps,Bake bake,BakeUvRequests requests,
    const EmissionRasterDevice::RetopoBakeTexel *texels,RetopoBakeFormat format,size_t count,
    EmissionRasterDevice::RetopoBakePixel *out){
  queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
    const size_t index=id[0];
    out[index]=filter_retopo_bake(bake_uv_sample(requests,index),maps,bake,texels,format);
  });
}
}
