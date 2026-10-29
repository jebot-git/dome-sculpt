// SPDX-License-Identifier: GPL-3.0-or-later
#include "brush_kernel.hpp"
#include <stdexcept>
#if defined(__HIPCC__)
#include <hip/hip_runtime.h>
#define DS_ALLOC hipMalloc
#define DS_FREE hipFree
#define DS_COPY hipMemcpy
#define DS_TO_DEVICE hipMemcpyHostToDevice
#define DS_TO_HOST hipMemcpyDeviceToHost
#define DS_SUCCESS hipSuccess
#define DS_LAST_ERROR hipGetLastError
#define DS_SYNC hipDeviceSynchronize
#define DS_ERROR_TEXT hipGetErrorString
#else
#include <cuda_runtime.h>
#define DS_ALLOC cudaMalloc
#define DS_FREE cudaFree
#define DS_COPY cudaMemcpy
#define DS_TO_DEVICE cudaMemcpyHostToDevice
#define DS_TO_HOST cudaMemcpyDeviceToHost
#define DS_SUCCESS cudaSuccess
#define DS_LAST_ERROR cudaGetLastError
#define DS_SYNC cudaDeviceSynchronize
#define DS_ERROR_TEXT cudaGetErrorString
#endif
namespace dome {
namespace {
template <class E> void checked(E result) {
  if (result != DS_SUCCESS)
    throw std::runtime_error(DS_ERROR_TEXT(result));
}
__global__ void sculpt(Sample *samples, size_t count, Brush b) {
  size_t i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < count)
    samples[i].distance = brush_sample(samples[i], b);
}
struct Device {
  Sample *samples = nullptr;
  ~Device() {
    if (samples)
      DS_FREE(samples);
  }
};
} // namespace
void apply_brush(std::span<Sample> samples, const Brush &b) {
  if (samples.empty())
    return;
  Device device;
  size_t bytes = samples.size_bytes();
  checked(DS_ALLOC(reinterpret_cast<void **>(&device.samples), bytes));
  checked(DS_COPY(device.samples, samples.data(), bytes, DS_TO_DEVICE));
  sculpt<<<(samples.size() + 255) / 256, 256>>>(device.samples, samples.size(),
                                                b);
  checked(DS_LAST_ERROR());
  checked(DS_SYNC());
  checked(DS_COPY(samples.data(), device.samples, bytes, DS_TO_HOST));
}
std::string_view compute_backend() {
#if defined(__HIPCC__)
  return "ROCM";
#else
  return "CUDA";
#endif
}
} // namespace dome
