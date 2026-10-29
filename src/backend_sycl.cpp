// SPDX-License-Identifier: GPL-3.0-or-later
#include "brush_kernel.hpp"
#include <sycl/sycl.hpp>
namespace dome {
void apply_brush(std::span<Sample> samples, const Brush &b) {
  if (samples.empty())
    return;
  // GPU selection is deliberate: a requested accelerator must not silently
  // turn into a CPU implementation when the driver/device is unavailable.
  sycl::queue queue{sycl::gpu_selector_v};
  sycl::buffer<Sample> buffer(samples.data(), sycl::range<1>{samples.size()});
  queue.submit([&](sycl::handler &command) {
    auto values = buffer.get_access<sycl::access::mode::read_write>(command);
    command.parallel_for(sycl::range<1>{samples.size()}, [=](sycl::id<1> id) {
      Sample s = values[id];
      s.distance = brush_sample(s, b);
      values[id] = s;
    });
  });
  queue.wait_and_throw();
}
std::string_view compute_backend() { return "ONEAPI"; }
} // namespace dome
