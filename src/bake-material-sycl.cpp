// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "bake-material-kernel.hpp"

namespace dome::detail::bake_material {
void launch_material(sycl::queue &queue,View view,const Coordinates *coordinates,
                     const paint_compute::Quantization *table,Values *values,size_t count){
  queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> index){
    values[index[0]]=evaluate(view,coordinates[index[0]],*table);
  });
}
} // namespace dome::detail::bake_material
