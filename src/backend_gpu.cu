// SPDX-License-Identifier: GPL-3.0-or-later
#include "brush_kernel.hpp"
#include "csg-kernel.hpp"
#include <stdexcept>
#include <vector>
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
__global__ void compose_boxes(const Sample *samples,float *distances,size_t count,
                              const CsgBox *boxes,size_t box_count,float band) {
  size_t i=blockIdx.x*blockDim.x+threadIdx.x;
  if(i<count)distances[i]=csg_detail::sample(samples[i],boxes,box_count,band);
}
template<class T> struct Buffer {
  T *data=nullptr;
  explicit Buffer(size_t count) {checked(DS_ALLOC(reinterpret_cast<void **>(&data),count*sizeof(T)));}
  ~Buffer(){if(data)(void)DS_FREE(data);}
  Buffer(const Buffer &)=delete;
};
struct Device {
  Sample *samples = nullptr;
  ~Device() {
    if (samples)
      (void)DS_FREE(samples);
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
void apply_csg(std::span<Sample> samples,std::span<const CsgBox> boxes,float band,
               const std::function<bool()> &cancelled) {
  if(samples.empty() || boxes.empty())return;
  csg_detail::validate(samples,boxes,band,cancelled);
  std::vector<float> result(samples.size());
  size_t capacity=std::min(samples.size(),csg_detail::chunk_limit(boxes.size()));
  Buffer<Sample> input(capacity);Buffer<float> output(capacity);Buffer<CsgBox> operations(boxes.size());
  checked(DS_COPY(operations.data,boxes.data(),boxes.size_bytes(),DS_TO_DEVICE));
  for(size_t first=0;first<samples.size();) {
    csg_detail::checkpoint(cancelled);size_t count=std::min(capacity,samples.size()-first);
    checked(DS_COPY(input.data,samples.data()+first,count*sizeof(Sample),DS_TO_DEVICE));
    compose_boxes<<<(count+255)/256,256>>>(input.data,output.data,count,operations.data,boxes.size(),band);
    checked(DS_LAST_ERROR());checked(DS_SYNC());
    checked(DS_COPY(result.data()+first,output.data,count*sizeof(float),DS_TO_HOST));first+=count;
  }
  csg_detail::checkpoint(cancelled);
  for(size_t i=0;i<samples.size();++i)samples[i].distance=result[i];
}
} // namespace dome
