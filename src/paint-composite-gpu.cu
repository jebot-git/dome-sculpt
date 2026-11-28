// SPDX-License-Identifier: GPL-3.0-or-later
#include <stdexcept>
#include <memory>
#include <algorithm>
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
#include "paint-grid-filter.hpp"
#include "paint-source-coordinate-kernel.hpp"
#include "portable-normal-raster-kernel.hpp"
#include "portable-normal-certificate-kernel.hpp"
#include "dome/paint.hpp"
#include "dome/source-maps.hpp"
#include "source-uv-sampler-kernel.hpp"
namespace dome {namespace {
template<class E>void checked(E value){if(value!=DS_SUCCESS)throw std::runtime_error(DS_ERROR_TEXT(value));}
__global__ void map_source_coordinates(paint_compute::SourceCoordinateBatch batch,uint32_t width,
                                       uint32_t first,size_t count){
 const size_t i=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
 if(i>=count*batch.size)return;
 const size_t index=i%count;
 const uint32_t at=first+uint32_t(index);
 const auto &job=batch.jobs[i/count];
 job.output[index]=paint_compute::source_coordinates(job,at%width,at/width);
}
__global__ void paint_pixels(paint_compute::Parameters p,const paint_compute::Layer *layers,const uint32_t *tiles,
    const paint_compute::Pixel *pixels,const int32_t *padding,paint_compute::Bake bake,
    paint_compute::PreparedColor prepared,paint_compute::SourceMaterial source,const paint_compute::Quantization *q,
    paint_compute::OutputView out,paint_compute::OutputGrid grid,size_t first,size_t count){
 const size_t i=blockIdx.x*blockDim.x+threadIdx.x;
 if(i>=count)return;
 const auto at=uint32_t(first+i);
 paint_compute::write_output(out,grid,uint32_t(i),at,p,layers,tiles,pixels,padding,bake,prepared,source,*q);
}
template<class T> struct Buffer {
  T *data=nullptr;
  explicit Buffer(size_t count) {
    if(!count)return;
    void *allocation=nullptr;
    try{checked(DS_ALLOC(&allocation,count*sizeof(T)));data=static_cast<T*>(allocation);}
    catch(...){if(allocation)(void)DS_FREE(allocation);throw;}
  }
  ~Buffer(){if(data)(void)DS_FREE(data);}
  Buffer(const Buffer &)=delete;
};
}
namespace detail {
namespace {
__global__ void sample_source_uvs(MappedSourceImage source, const paint_compute::Quantization *table,
    const SourceUvSampler::Coordinate *coordinates, uint32_t *colors, size_t count) {
  const size_t at = size_t(blockIdx.x) * blockDim.x + threadIdx.x;
  if (at < count) sample_mapped_source(source, coordinates[at], *table, colors, at);
}

class GpuSourceUvSampler final : public SourceUvSampler {
  Buffer<uint8_t> image_;
  Buffer<paint_compute::Quantization> table_{1};
  Buffer<Coordinate> coordinates_;
  Buffer<uint32_t> colors_;
  MappedSourceImage source_;

public:
  GpuSourceUvSampler(const PaintSourceTexture &source, bool srgb, Output format, size_t capacity,Transform transform)
      : SourceUvSampler(capacity, format), image_(source.image->rgba.size()), coordinates_(capacity),
        colors_(capacity * words(format)),
        source_(mapped_source_image(source,image_.data,srgb,format,transform)) {
    const auto &table = paint_compute::quantization();
    checked(DS_COPY(image_.data, source.image->rgba.data(), source.image->rgba.size(), DS_TO_DEVICE));
    checked(DS_COPY(table_.data, &table, sizeof(table), DS_TO_DEVICE));
  }

private:
  void dispatch(std::span<const Coordinate> pending, std::span<uint32_t> result) override {
    checked(DS_COPY(coordinates_.data, pending.data(), pending.size_bytes(), DS_TO_DEVICE));
    sample_source_uvs<<<(pending.size() + 127) / 128, 128>>>(source_, table_.data,
        coordinates_.data, colors_.data, pending.size());
    checked(DS_LAST_ERROR());
    checked(DS_COPY(result.data(), colors_.data, result.size_bytes(), DS_TO_HOST));
  }
};
}

std::unique_ptr<SourceUvSampler> upload_source_uv_sampler(
    const PaintSourceTexture &source, bool srgb, SourceUvSampler::Output format, size_t capacity,
    SourceUvSampler::Transform transform) {
  return std::make_unique<GpuSourceUvSampler>(source, srgb, format, capacity, transform);
}
}
namespace detail::portable_normal {
namespace {
__global__ void normal_texels(const Chart *charts,ImageView source,const paint_compute::Quantization *q,
                             const Sample *samples,uint32_t *result,size_t count){
 const size_t at=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
 if(at<count){
  const auto sample=samples[at];
  result[at]=encode_texel(charts[sample.chart],sample,source,*q);
 }
}
__global__ void normal_probes(const Chart *charts,ImageView source,ImageView emitted,
    const paint_compute::Quantization *q,const Probe *probes,Verdict *result,size_t count){
 const size_t at=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
 if(at<count){
  const auto probe=probes[at];
  result[at]=certify_texel(charts[probe.source.chart],probe,source,emitted,*q);
 }
}
class GpuRaster final:public Raster {
 Buffer<uint8_t> image_;
 Buffer<paint_compute::Quantization> quantization_{1};
 Buffer<Chart> charts_{batch_charts};
 Buffer<Sample> samples_{batch_pixels};
 Buffer<uint32_t> result_{batch_pixels};
 std::unique_ptr<Buffer<uint8_t>> emitted_;
 std::unique_ptr<Buffer<Probe>> probes_;
 std::unique_ptr<Buffer<Verdict>> verdicts_;
 uint32_t width_,height_;
 uint32_t emitted_width_=0,emitted_height_=0;
public:
 explicit GpuRaster(const Image &image):image_(image.rgba.size()),width_(image.width),height_(image.height){
  const auto &table=paint_compute::quantization();
  checked(DS_COPY(quantization_.data,&table,sizeof(table),DS_TO_DEVICE));
  checked(DS_COPY(image_.data,image.rgba.data(),image.rgba.size(),DS_TO_DEVICE));
 }
 void encode(std::span<const Chart> charts,std::span<const Sample> pending,std::span<uint32_t> output,
     const std::function<bool()> &cancelled) override{
  validate_batch(charts,pending,output);
  paint_compute::checkpoint(cancelled);
  checked(DS_COPY(charts_.data,charts.data(),charts.size_bytes(),DS_TO_DEVICE));
  checked(DS_COPY(samples_.data,pending.data(),pending.size_bytes(),DS_TO_DEVICE));
  normal_texels<<<(pending.size()+127)/128,128>>>(charts_.data,{image_.data,width_,height_},
      quantization_.data,samples_.data,result_.data,pending.size());
  checked(DS_LAST_ERROR());
  checked(DS_COPY(output.data(),result_.data,output.size_bytes(),DS_TO_HOST));
  paint_compute::checkpoint(cancelled);
 }
 void set_emitted_image(const Image &image,const std::function<bool()> &cancelled) override{
  paint_compute::checkpoint(cancelled);
  image.validate();
  if(!probes_)probes_=std::make_unique<Buffer<Probe>>(batch_pixels);
  if(!verdicts_)verdicts_=std::make_unique<Buffer<Verdict>>(batch_pixels);
  emitted_.reset();
  auto next=std::make_unique<Buffer<uint8_t>>(image.rgba.size());
  checked(DS_COPY(next->data,image.rgba.data(),image.rgba.size(),DS_TO_DEVICE));
  paint_compute::checkpoint(cancelled);
  emitted_=std::move(next);
  emitted_width_=image.width;
  emitted_height_=image.height;
 }
 void certify(std::span<const Chart> charts,std::span<const Probe> pending,std::span<Verdict> output,
     const std::function<bool()> &cancelled) override{
  validate_batch(charts,pending,output);
  paint_compute::checkpoint(cancelled);
  if(!emitted_)throw std::logic_error("Portable normal certification has no emitted image");
  checked(DS_COPY(charts_.data,charts.data(),charts.size_bytes(),DS_TO_DEVICE));
  checked(DS_COPY(probes_->data,pending.data(),pending.size_bytes(),DS_TO_DEVICE));
  normal_probes<<<(pending.size()+127)/128,128>>>(charts_.data,{image_.data,width_,height_},
      {emitted_->data,emitted_width_,emitted_height_},quantization_.data,
      probes_->data,verdicts_->data,pending.size());
  checked(DS_LAST_ERROR());
  checked(DS_COPY(output.data(),verdicts_->data,output.size_bytes(),DS_TO_HOST));
  paint_compute::checkpoint(cancelled);
 }
};
}
std::unique_ptr<Raster> upload(const Image &image,const std::function<bool()> &cancelled){
 paint_compute::checkpoint(cancelled);
 image.validate();
 auto raster=std::make_unique<GpuRaster>(image);
 paint_compute::checkpoint(cancelled);
 return raster;
}
}
namespace paint_compute {
Result composite(const Input &in,const std::function<bool()> &cancelled){
 validate(in,cancelled);const auto &q=quantization();
 Buffer<Layer> layers(std::max(size_t(1),in.layers.size()));Buffer<uint32_t> tiles(std::max(size_t(1),in.tiles.size()));
 const auto host_padding=in.padding_view();
 Buffer<Pixel> pixels(std::max(size_t(1),in.packed_texels()));Buffer<int32_t> padding(host_padding.size());Buffer<Quantization> quant(1);
 Buffer<uint8_t> output(output_scratch_bytes);
 Buffer<uint8_t> baked(in.image_bytes());
 Buffer<detail::SourceTextureAxis> axes(in.axis_count());
 Buffer<SourceTerms> terms(in.term_count());
 Buffer<SourcePixelTaps> mapped(in.coupled_source_count()*chunk_pixels);
 Buffer<uint32_t> color_tiles(in.color_tiles.size());
 Buffer<ColorPixel> color_pixels(in.color_pixels.size());
 Result result(in);
 try{
  checkpoint(cancelled);
  if(!in.layers.empty())checked(DS_COPY(layers.data,in.layers.data(),in.layers.size()*sizeof(Layer),DS_TO_DEVICE));
  if(!in.tiles.empty())checked(DS_COPY(tiles.data,in.tiles.data(),in.tiles.size()*sizeof(uint32_t),DS_TO_DEVICE));
  if(!in.pixels.empty())checked(DS_COPY(pixels.data,in.pixels.data(),in.pixels.size()*sizeof(Pixel),DS_TO_DEVICE));
  if(!in.native_tiles.empty())visit_native_tile_uploads(in,[&](size_t first,std::span<const std::byte> bytes){
   checked(DS_COPY(pixels.data+first,bytes.data(),bytes.size(),DS_TO_DEVICE));
  },cancelled);
  if(color_tiles.data)checked(DS_COPY(color_tiles.data,in.color_tiles.data(),in.color_tiles.size()*sizeof(uint32_t),DS_TO_DEVICE));
  if(color_pixels.data)checked(DS_COPY(color_pixels.data,in.color_pixels.data(),in.color_pixels.size()*sizeof(ColorPixel),DS_TO_DEVICE));
  checked(DS_COPY(padding.data,host_padding.data(),host_padding.size_bytes(),DS_TO_DEVICE));checked(DS_COPY(quant.data,&q,sizeof(q),DS_TO_DEVICE));
  const auto bake=in.bake.device_view(baked.data);
  const auto prepared=in.device_colors(baked.data,color_tiles.data,color_pixels.data,axes.data);
  auto *image_target=baked.data;
  in.visit_images([&](const auto plane){
   checked(DS_COPY(image_target,plane.data(),plane.size_bytes(),DS_TO_DEVICE));
   image_target+=plane.size();
  });
  auto *axis_target=axes.data;
  in.visit_axes([&](const auto taps){
   checked(DS_COPY(axis_target,taps.data(),taps.size_bytes(),DS_TO_DEVICE));
   axis_target+=taps.size();
  });
  auto *term_target=terms.data;
  in.visit_terms([&](const auto values){
   checked(DS_COPY(term_target,values.data(),values.size_bytes(),DS_TO_DEVICE));
   term_target+=values.size();
  });
  for(const auto grid:in.output_grids()){
   if(!grid.channels)break;
   auto source=in.device_sources(baked.data,axes.data);
   const auto coordinates=in.device_coordinate_jobs(terms.data,mapped.data,chunk_pixels,source,grid.channels);
   const auto out=scratch_view(output.data,grid,!result.pixels.empty());
   for(size_t first=0;first<grid.extent.size();){
    checkpoint(cancelled);
    const auto count=std::min(chunk_pixels,grid.extent.size()-first);
    source.first=uint32_t(first);
    if(coordinates.size){
     map_source_coordinates<<<(count*coordinates.size+255)/256,256>>>(coordinates,grid.extent.width,source.first,count);
     checked(DS_LAST_ERROR());
    }
    paint_pixels<<<(count+255)/256,256>>>(in.parameters,layers.data,tiles.data,pixels.data,padding.data,
        bake,prepared,source,quant.data,out,grid,first,count);
    checked(DS_LAST_ERROR());checked(DS_SYNC());
    read_output(result.view(grid,first),out,count,[&](auto *target,const auto *device,size_t bytes){
     checked(DS_COPY(target,device,bytes,DS_TO_HOST));
    });
    first+=count;
   }
  }
  checkpoint(cancelled);
 }catch(...){(void)DS_SYNC();throw;}
 return result;
}
}

namespace paint_grid {
namespace {
__global__ void grid_pixels(Batch batch,Layout layout,const Tables *tables,
                            const Pixel *input,Pixel *output,size_t count) {
  const size_t i=size_t(blockIdx.x)*blockDim.x+threadIdx.x;
  if(i<count)output[i]=filter(i,batch,layout,*tables,input);
}
}
struct Filter::Impl {
  Buffer<Tables> tables{1};
  Buffer<Pixel> input{max_input_pixels},output{max_output_pixels};
  explicit Impl(const Tables &host) {
    checked(DS_COPY(tables.data,&host,sizeof(host),DS_TO_DEVICE));
  }
};
Filter::Filter(const Tables &tables):impl_(std::make_unique<Impl>(tables)){}
Filter::~Filter()=default;
void Filter::run(const Batch &batch,const Layout &layout,std::span<const Pixel> input,
                 std::span<Pixel> output,const std::function<bool()> &cancelled) {
  validate(batch,layout,input.size(),output.size());
  checkpoint(cancelled);
  auto &gpu=*impl_;
  checked(DS_COPY(gpu.input.data,input.data(),input.size_bytes(),DS_TO_DEVICE));
  constexpr unsigned block=128;
  grid_pixels<<<(output.size()+block-1)/block,block>>>(batch,layout,gpu.tables.data,
      gpu.input.data,gpu.output.data,output.size());
  checked(DS_LAST_ERROR());
  checked(DS_COPY(output.data(),gpu.output.data,output.size_bytes(),DS_TO_HOST));
  checkpoint(cancelled);
}
}
}
