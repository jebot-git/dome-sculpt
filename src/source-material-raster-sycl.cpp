// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "source-material-raster-kernel.hpp"

namespace dome::detail::emission_raster {
void launch_material_sources(sycl::queue &queue,MappedSourceImage image,const paint_compute::Quantization *q,
    const SourceUvSampler::Coordinate *uv,size_t count,uint32_t *out){
  queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
    sample_mapped_source(image,uv[id[0]],*q,out,id[0]);
  });
}
void launch_material(sycl::queue &queue,DeviceInput input,MaterialView material,FiniteGrid grid,
    MaterialSources source,const uint32_t *texels,size_t count,paint_compute::Output *out){
  queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
    out[id[0]]=render_material(input,material,grid,source,texels[id[0]],id[0]);
  });
}
}
