// SPDX-License-Identifier: GPL-3.0-or-later
#include "brush_kernel.hpp"
#include "csg-kernel.hpp"
#include "sparse-resample-backend.hpp"
#include "gpu-brush-transfer.hpp"
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
#include "paint-composite-backend.hpp"
namespace dome {
namespace {
template <class E> void checked(E result) {
  if (result != DS_SUCCESS)
    throw std::runtime_error(DS_ERROR_TEXT(result));
}
__global__ void sculpt(const Sample *samples,float *distances,size_t count,Brush b) {
  size_t i = blockIdx.x * blockDim.x + threadIdx.x;
  if (i < count)
    distances[i] = brush_sample(samples[i], b);
}
__global__ void compose_boxes(const Sample *samples,float *distances,size_t count,
                              const CsgBox *boxes,size_t box_count,float band) {
  size_t i=blockIdx.x*blockDim.x+threadIdx.x;
  if(i<count)distances[i]=csg_detail::sample(samples[i],boxes,box_count,band);
}
__global__ void sparse_values(const sparse_resample::Cell *cells,uint32_t mask,
    const sparse_resample::Point *points,uint64_t *values,size_t count,sparse_resample::Parameters p) {
  const size_t i=blockIdx.x*blockDim.x+threadIdx.x;
  if(i<count)values[i]=sparse_resample::sample(cells,mask,points[i],p);
}
__global__ void paint_pixels(paint_compute::Parameters p,const paint_compute::Layer *layers,const uint32_t *tiles,
    const paint_compute::Pixel *pixels,const int32_t *padding,const paint_compute::Quantization *q,
    paint_compute::Output *out,size_t first,size_t count){
 const size_t i=blockIdx.x*blockDim.x+threadIdx.x;
 if(i<count)out[i]=paint_compute::compose(uint32_t(first+i),p,layers,tiles,pixels,padding,*q);
}
__global__ void sparse_errors(const sparse_resample::Cell *cells,uint32_t mask,
    const sparse_resample::ErrorQuery *points,uint64_t *partial,size_t count,sparse_resample::Parameters p) {
  __shared__ uint64_t local[sparse_resample::reduction_width];
  const auto lane=threadIdx.x;const size_t i=blockIdx.x*blockDim.x+lane;
  local[lane]=i<count?sparse_resample::absolute_error(cells,mask,points[i],p):0;
  __syncthreads();
  for(unsigned step=sparse_resample::reduction_width/2;step;step/=2){
    if(lane<step&&local[lane+step]>local[lane])local[lane]=local[lane+step];
    __syncthreads();
  }
  if(!lane)partial[blockIdx.x]=local[0];
}
__global__ void sparse_error_maximum(const uint64_t *partial,uint64_t *maximum,size_t count) {
  __shared__ uint64_t local[sparse_resample::reduction_width];
  const auto lane=threadIdx.x;uint64_t value=0;
  for(size_t i=lane;i<count;i+=blockDim.x)if(partial[i]>value)value=partial[i];
  local[lane]=value;__syncthreads();
  for(unsigned step=sparse_resample::reduction_width/2;step;step/=2){
    if(lane<step&&local[lane+step]>local[lane])local[lane]=local[lane+step];
    __syncthreads();
  }
  if(!lane)*maximum=local[0];
}
template<class T> struct Buffer {
  T *data=nullptr;
  explicit Buffer(size_t count) {
    void *allocation=nullptr;
    try{checked(DS_ALLOC(&allocation,count*sizeof(T)));data=static_cast<T*>(allocation);}
    catch(...){if(allocation)(void)DS_FREE(allocation);throw;}
  }
  ~Buffer(){if(data)(void)DS_FREE(data);}
  Buffer(const Buffer &)=delete;
};
struct BrushApi {
  void allocate(void **pointer,size_t bytes){checked(DS_ALLOC(pointer,bytes));}
  void release(void *pointer)noexcept{(void)DS_FREE(pointer);}
  void upload(void *target,const void *source,size_t bytes){checked(DS_COPY(target,source,bytes,DS_TO_DEVICE));}
  void readback(void *target,const void *source,size_t bytes){checked(DS_COPY(target,source,bytes,DS_TO_HOST));}
  void launch(const Sample *input,float *output,size_t count,Brush brush){
    sculpt<<<(count+255)/256,256>>>(input,output,count,brush);
    checked(DS_LAST_ERROR());
  }
  void wait(){checked(DS_SYNC());}
  void drain()noexcept{(void)DS_SYNC();}
};
} // namespace
void apply_brush(std::span<Sample> samples, const Brush &b) {
  BrushApi api;
  gpu_brush::apply(samples,b,api);
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
struct SparseFieldSampler::Impl {
  Buffer<sparse_resample::Cell> cells;
  Buffer<sparse_resample::Point> points;
  Buffer<uint64_t> values;
  std::unique_ptr<Buffer<sparse_resample::ErrorQuery>> error_queries;
  std::unique_ptr<Buffer<uint64_t>> error_partials;
  sparse_resample::Parameters parameters;
  uint32_t mask;size_t capacity;bool failed=false;
  Impl(const sparse_resample::Grid &grid,sparse_resample::Parameters p,size_t count):cells(grid.cells().size()),points(count),values(count),parameters(p),mask(grid.mask()),capacity(count) {
    checked(DS_COPY(cells.data,grid.cells().data(),grid.cells().size_bytes(),DS_TO_DEVICE));
  }
};
SparseFieldSampler::SparseFieldSampler(const sparse_resample::Grid &grid,sparse_resample::Parameters p,size_t capacity) {
  if(!capacity||capacity>65536)throw std::invalid_argument("Invalid sparse resampling chunk capacity");
  sparse_resample::validate_queries({},p);impl_=std::make_unique<Impl>(grid,p,capacity);
}
SparseFieldSampler::~SparseFieldSampler()=default;
std::vector<uint64_t> SparseFieldSampler::sample(std::span<const sparse_resample::Point> queries,const std::function<bool()> &cancelled) {
  auto &state=*impl_;if(state.failed)throw std::runtime_error("Sparse resampling GPU session has failed");
  sparse_resample::validate_queries(queries,state.parameters,[&]{sparse_resample_checkpoint(cancelled);});
  std::vector<uint64_t> result(queries.size());
  try {
    for(size_t first=0;first<queries.size();) {
      sparse_resample_checkpoint(cancelled);const auto count=std::min(state.capacity,queries.size()-first);
      checked(DS_COPY(state.points.data,queries.data()+first,count*sizeof(sparse_resample::Point),DS_TO_DEVICE));
      sparse_values<<<(count+127)/128,128>>>(state.cells.data,state.mask,state.points.data,state.values.data,count,state.parameters);
      checked(DS_LAST_ERROR());checked(DS_SYNC());
      checked(DS_COPY(result.data()+first,state.values.data,count*sizeof(uint64_t),DS_TO_HOST));first+=count;
    }
    sparse_resample_checkpoint(cancelled);
  }catch(...){state.failed=true;(void)DS_SYNC();throw;}
  return result;
}
uint64_t SparseFieldSampler::maximum_error(std::span<const sparse_resample::ErrorQuery> queries,const std::function<bool()> &cancelled) {
  auto &state=*impl_;if(state.failed)throw std::runtime_error("Sparse quality GPU session has failed");
  sparse_resample::validate_error_queries(queries,state.parameters,[&]{sparse_resample_checkpoint(cancelled);});
  if(queries.empty())return 0;
  uint64_t maximum=0,current=0;
  try{
    if(!state.error_queries){
      auto input=std::make_unique<Buffer<sparse_resample::ErrorQuery>>(state.capacity);
      auto partial=std::make_unique<Buffer<uint64_t>>((state.capacity+sparse_resample::reduction_width-1)/sparse_resample::reduction_width);
      state.error_queries=std::move(input);state.error_partials=std::move(partial);
    }
    for(size_t first=0;first<queries.size();){
      sparse_resample_checkpoint(cancelled);const auto count=std::min(state.capacity,queries.size()-first);
      const auto groups=(count+sparse_resample::reduction_width-1)/sparse_resample::reduction_width;
      checked(DS_COPY(state.error_queries->data,queries.data()+first,count*sizeof(sparse_resample::ErrorQuery),DS_TO_DEVICE));
      sparse_errors<<<groups,sparse_resample::reduction_width>>>(state.cells.data,state.mask,state.error_queries->data,state.error_partials->data,count,state.parameters);checked(DS_LAST_ERROR());
      sparse_error_maximum<<<1,sparse_resample::reduction_width>>>(state.error_partials->data,state.values.data,groups);checked(DS_LAST_ERROR());checked(DS_SYNC());
      checked(DS_COPY(&current,state.values.data,sizeof(current),DS_TO_HOST));
      maximum=std::max(maximum,sparse_resample::checked_error(current));first+=count;
    }
    sparse_resample_checkpoint(cancelled);
  }catch(...){state.failed=true;(void)DS_SYNC();throw;}
  return maximum;
}

namespace paint_compute {
std::vector<Output> composite(const Input &in,const std::function<bool()> &cancelled){
 validate(in,cancelled);const auto &q=quantization();
 Buffer<Layer> layers(std::max(size_t(1),in.layers.size()));Buffer<uint32_t> tiles(std::max(size_t(1),in.tiles.size()));
 Buffer<Pixel> pixels(std::max(size_t(1),in.pixels.size()));Buffer<int32_t> padding(in.padding.size());Buffer<Quantization> quant(1);Buffer<Output> output(chunk_pixels);
 std::vector<Output> result(in.padding.size());
 try{
  checkpoint(cancelled);
  if(!in.layers.empty())checked(DS_COPY(layers.data,in.layers.data(),in.layers.size()*sizeof(Layer),DS_TO_DEVICE));
  if(!in.tiles.empty())checked(DS_COPY(tiles.data,in.tiles.data(),in.tiles.size()*sizeof(uint32_t),DS_TO_DEVICE));
  if(!in.pixels.empty())checked(DS_COPY(pixels.data,in.pixels.data(),in.pixels.size()*sizeof(Pixel),DS_TO_DEVICE));
  checked(DS_COPY(padding.data,in.padding.data(),in.padding.size()*sizeof(int32_t),DS_TO_DEVICE));checked(DS_COPY(quant.data,&q,sizeof(q),DS_TO_DEVICE));
  for(size_t first=0;first<result.size();){checkpoint(cancelled);const auto count=std::min(chunk_pixels,result.size()-first);
   paint_pixels<<<(count+255)/256,256>>>(in.parameters,layers.data,tiles.data,pixels.data,padding.data,quant.data,output.data,first,count);checked(DS_LAST_ERROR());checked(DS_SYNC());
   checked(DS_COPY(result.data()+first,output.data,count*sizeof(Output),DS_TO_HOST));first+=count;
  }
  checkpoint(cancelled);
 }catch(...){(void)DS_SYNC();throw;}
 return result;
}
}
} // namespace dome
