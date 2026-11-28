// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "portable-normal-certificate-kernel.hpp"

namespace dome::detail::portable_normal {
void launch_certificate(sycl::queue &queue,const Chart *charts,const Probe *probes,
    const paint_compute::Quantization *q,ImageView source,ImageView emitted,Verdict *result,size_t count){
 queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
  const auto probe=probes[id[0]];
  result[id[0]]=certify_texel(charts[probe.source.chart],probe,source,emitted,*q);
 });
}
}
