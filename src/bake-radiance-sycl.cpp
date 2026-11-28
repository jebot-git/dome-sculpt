// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "emission-raster-kernel.hpp"

namespace dome::detail::emission_raster {
void launch_bake_radiance(sycl::queue &queue,DeviceInput input,MappedSourceImage image,
    const EmissionRasterDevice::BakeRadianceSample *samples,size_t count,uint32_t *out,bool filter_proof){
  queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
    render_bake_radiance(input,image,samples[id[0]],out+(filter_proof?7:3)*id[0],filter_proof);
  });
}

void launch_bake_radiance_summary(sycl::queue &queue,const uint32_t *radiance,size_t count,BakeRadianceSummary *summary){
  constexpr size_t lanes=128;
  queue.parallel_for(sycl::nd_range<1>{{lanes},{lanes}},[=](sycl::nd_item<1> item){
    const auto lane=item.get_local_linear_id();
    const auto group=item.get_group();
    uint32_t common[3]{UINT32_MAX,UINT32_MAX,UINT32_MAX},seen[3]{};
    for(size_t i=lane;i<count;i+=lanes)for(unsigned c=0;c<3;++c){
      const auto word=radiance[i*3+c];
      common[c]&=word;
      seen[c]|=word;
    }
    for(unsigned c=0;c<3;++c){
      const auto every=sycl::reduce_over_group(group,common[c],sycl::bit_and<uint32_t>{});
      const auto any=sycl::reduce_over_group(group,seen[c],sycl::bit_or<uint32_t>{});
      if(!lane){summary->common[c]=every;summary->seen[c]=any;}
    }
  });
}
} // namespace dome::detail::emission_raster
