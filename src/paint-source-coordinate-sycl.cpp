// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "paint-source-coordinate-kernel.hpp"

namespace dome::paint_compute {
void launch_source_coordinates(sycl::queue &queue,SourceCoordinateBatch batch,
                               uint32_t width,uint32_t first,size_t count) {
  queue.parallel_for(sycl::range<1>{count*batch.size},[=](sycl::id<1> id){
    const size_t index=id[0]%count;
    const uint32_t at=first+uint32_t(index);
    const auto &job=batch.jobs[id[0]/count];
    job.output[index]=source_coordinates(job,at%width,at/width);
  });
}
}
