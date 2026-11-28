// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "bake-raster-kernel.hpp"
#include "surface-ray-softfloat.hpp"

namespace dome::detail::bake_raster {
namespace {
using Atomic=sycl::atomic_ref<uint32_t,sycl::memory_order::relaxed,sycl::memory_scope::device,
                             sycl::access::address_space::global_space>;
}

void launch_raster(sycl::queue &queue,View view,const Span *spans,uint32_t span_count,uint32_t count,
                   uint32_t first,Sample *output){
  queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
    const uint32_t slot=id[0];
    output[slot]=raster<surface_ray::SoftPrecision>(view,spans,span_count,slot,first,
        [](uint32_t *owner,uint32_t value){Atomic(*owner).fetch_min(value);});
  });
}

void launch_compact(sycl::queue &queue,View view,const Sample *candidates,uint32_t count,uint32_t first,
                    uint32_t blocks,Block *ranges,Sample *output,Totals *totals){
  queue.parallel_for(sycl::range<1>{blocks},[=](sycl::id<1> id){
    compact(view,candidates,count,first,uint32_t(id[0]),ranges,output,
            [=](uint32_t n){return Atomic(totals->samples).fetch_add(n);},
            [=](uint32_t n){Atomic(totals->overlaps).fetch_add(n);});
  });
}
} // namespace dome::detail::bake_raster
