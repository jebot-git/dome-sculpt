// SPDX-License-Identifier: GPL-3.0-or-later
#include <sycl/sycl.hpp>
#include "paint-composite-backend.hpp"
#include <algorithm>
#include <memory>
namespace dome {
namespace {
struct UsmDeleter {sycl::queue *queue;template<class T>void operator()(T*p)const noexcept{if(p)try{sycl::free(p,*queue);}catch(...){}}};
}
namespace paint_compute {
void launch_source_coordinates(sycl::queue &,SourceCoordinateBatch,uint32_t,uint32_t,size_t);

Result composite(const Input &in,const std::function<bool()> &cancelled){
 validate(in,cancelled);const auto &table=quantization();
 sycl::queue queue{sycl::gpu_selector_v,[](sycl::exception_list errors){for(auto error:errors)std::rethrow_exception(error);},sycl::property_list{sycl::property::queue::in_order{}}};
 auto allocate=[&]<class T>(size_t count){auto p=std::unique_ptr<T,UsmDeleter>{sycl::malloc_device<T>(std::max(size_t(1),count),queue),{&queue}};if(!p)throw std::bad_alloc();return p;};
 auto layers=allocate.template operator()<Layer>(in.layers.size());auto tiles=allocate.template operator()<uint32_t>(in.tiles.size());
 const auto host_padding=in.padding_view();
 auto pixels=allocate.template operator()<Pixel>(in.packed_texels());auto padding=allocate.template operator()<int32_t>(host_padding.size());
 std::unique_ptr<uint8_t,UsmDeleter> baked{nullptr,{&queue}};
 if(const auto bytes=in.image_bytes())baked=allocate.template operator()<uint8_t>(bytes);
 std::unique_ptr<detail::SourceTextureAxis,UsmDeleter> axes{nullptr,{&queue}};
 if(const auto count=in.axis_count())axes=allocate.template operator()<detail::SourceTextureAxis>(count);
 std::unique_ptr<SourceTerms,UsmDeleter> terms{nullptr,{&queue}};
 std::unique_ptr<SourcePixelTaps,UsmDeleter> mapped{nullptr,{&queue}};
 if(const auto count=in.term_count())terms=allocate.template operator()<SourceTerms>(count);
 if(const auto count=in.coupled_source_count())mapped=allocate.template operator()<SourcePixelTaps>(count*chunk_pixels);
 auto quant=allocate.template operator()<Quantization>(1);
 auto output=allocate.template operator()<uint8_t>(output_scratch_bytes);
 std::unique_ptr<uint32_t,UsmDeleter> color_tiles{nullptr,{&queue}};
 std::unique_ptr<ColorPixel,UsmDeleter> color_pixels{nullptr,{&queue}};
 if(!in.color_tiles.empty())color_tiles=allocate.template operator()<uint32_t>(in.color_tiles.size());
 if(!in.color_pixels.empty())color_pixels=allocate.template operator()<ColorPixel>(in.color_pixels.size());
 Result result(in);
 try{
  checkpoint(cancelled);
  if(!in.layers.empty())queue.memcpy(layers.get(),in.layers.data(),in.layers.size()*sizeof(Layer));
  if(!in.tiles.empty())queue.memcpy(tiles.get(),in.tiles.data(),in.tiles.size()*sizeof(uint32_t));
  if(!in.pixels.empty())queue.memcpy(pixels.get(),in.pixels.data(),in.pixels.size()*sizeof(Pixel));
  if(!in.native_tiles.empty())visit_native_tile_uploads(in,[&](size_t first,std::span<const std::byte> bytes){
   queue.memcpy(pixels.get()+first,bytes.data(),bytes.size()).wait_and_throw();
  },cancelled);
  if(color_tiles)queue.memcpy(color_tiles.get(),in.color_tiles.data(),in.color_tiles.size()*sizeof(uint32_t));
  if(color_pixels)queue.memcpy(color_pixels.get(),in.color_pixels.data(),in.color_pixels.size()*sizeof(ColorPixel));
  queue.memcpy(padding.get(),host_padding.data(),host_padding.size_bytes());queue.memcpy(quant.get(),&table,sizeof(table));
  const auto bake=in.bake.device_view(baked.get());
  const auto prepared=in.device_colors(baked.get(),color_tiles.get(),color_pixels.get(),axes.get());
  auto *image_target=baked.get();
  in.visit_images([&](const auto plane){
   queue.memcpy(image_target,plane.data(),plane.size_bytes());
   image_target+=plane.size();
  });
  auto *axis_target=axes.get();
  in.visit_axes([&](const auto taps){
   queue.memcpy(axis_target,taps.data(),taps.size_bytes());
   axis_target+=taps.size();
  });
  auto *term_target=terms.get();
  in.visit_terms([&](const auto values){
   queue.memcpy(term_target,values.data(),values.size_bytes());
   term_target+=values.size();
  });
  const auto p=in.parameters;const auto l=layers.get();const auto t=tiles.get();const auto px=pixels.get();const auto pad=padding.get();const auto q=quant.get();
  for(const auto grid:in.output_grids()){
   if(!grid.channels)break;
   auto source=in.device_sources(baked.get(),axes.get());
   const auto coordinates=in.device_coordinate_jobs(terms.get(),mapped.get(),chunk_pixels,source,grid.channels);
   const auto out=scratch_view(output.get(),grid,!result.pixels.empty());
   for(size_t first=0;first<grid.extent.size();){
    checkpoint(cancelled);
    const auto count=std::min(chunk_pixels,grid.extent.size()-first);
    source.first=uint32_t(first);
    if(coordinates.size)launch_source_coordinates(queue,coordinates,grid.extent.width,source.first,count);
    queue.parallel_for(sycl::range<1>{count},[=](sycl::id<1> id){
     const auto index=uint32_t(id[0]);
     write_output(out,grid,index,uint32_t(first)+index,p,l,t,px,pad,bake,prepared,source,*q);
    });
    read_output(result.view(grid,first),out,count,[&](auto *target,const auto *device,size_t bytes){
     queue.memcpy(target,device,bytes);
    });
    queue.wait_and_throw();
    first+=count;
   }
  }
  checkpoint(cancelled);
 }catch(...){try{queue.wait_and_throw();}catch(...){}throw;}
 return result;
}
}
}
