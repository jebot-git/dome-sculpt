// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "bake-material-kernel.hpp"

namespace dome::detail::bake_material {
void launch_material(sycl::queue &queue,View view,const Uv *coordinates,
                     const paint_compute::Quantization *table,SampleOutput output,size_t count){
  queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> index){
    evaluate_sample(view,coordinates,*table,output,index[0]);
  });
}
} // namespace dome::detail::bake_material
