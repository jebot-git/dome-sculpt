// SPDX-License-Identifier: GPL-3.0-or-later

#include <sycl/sycl.hpp>
#include "emission-raster-kernel.hpp"

namespace dome::detail::emission_raster {
void launch_bake(sycl::queue &queue,DeviceInput input,size_t first,size_t count,uint32_t *out){
  queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
    render<true>(input,first+id[0],out+10*id[0]);
  });
}
void launch_bake_uv_filter(sycl::queue &queue,Bake bake,const EmissionRasterDevice::BakeUvSample *samples,
                          size_t count,uint32_t *out){
  queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
    filter_bake_uv(bake,samples[id[0]],out+7*id[0]);
  });
}
void launch_bake_uv_maps(sycl::queue &queue,BakeUvMaps maps,Bake bake,const EmissionRasterDevice::BakeUvSample *samples,
                        size_t count,EmissionRasterDevice::BakeUvMapsPixel *out,uint32_t *emission){
  queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
    const size_t at=id[0];
    out[at]=filter_bake_uv_maps(maps,samples[at]);
    if(emission)filter_bake_uv(bake,samples[at],emission+7*at);
  });
}
}
