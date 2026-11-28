// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "bake-padding-kernel.hpp"

namespace dome::detail::bake_padding {
namespace {
using Atomic=sycl::atomic_ref<uint32_t,sycl::memory_order::relaxed,sycl::memory_scope::device,
                              sycl::access::address_space::global_space>;
struct Enqueue {
  void operator()(uint32_t *size,uint32_t *queue,uint32_t pixel) const {
    queue[Atomic(*size).fetch_add(1)]=pixel;
  }
};
}

void launch_seed(sycl::queue &queue,Frontier view,const uint8_t *coverage,uint32_t first,uint32_t count){
  queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
    seed(view,coverage,first+uint32_t(id[0]),Enqueue{});
  });
}

void launch_expand(sycl::queue &queue,Frontier view,uint32_t first,uint32_t count,uint32_t distance){
  queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
    expand(view,first+uint32_t(id[0]),distance,[](uint32_t *key){return Atomic(*key).load();},
        [](uint32_t *key,uint32_t value){return Atomic(*key).fetch_min(value);},Enqueue{});
  });
}

void launch_gather(sycl::queue &queue,Frontier view,uint32_t first,uint32_t count,Pixel *output){
  queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
    output[id[0]]=pair_at(view,first+uint32_t(id[0]));
  });
}
}
