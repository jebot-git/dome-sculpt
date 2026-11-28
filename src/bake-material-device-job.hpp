// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake-device-buffer.hpp"
#include "bake-overlay-kernel.hpp"
#include "emission-raster-transfer.hpp"
#include "paint-composite-backend.hpp"
#include "paint-overlay-packet.hpp"

namespace dome::detail::bake_material {
template<class Api> class DeviceSampler final:public Sampler {
  template<class T> using Buffer=BakeDeviceBuffer<Api,T>;
  struct ImageLayout {
    size_t offset[RoleCount]{},bytes=0;
    bool upload[RoleCount]{};
  };
  static ImageLayout layout(const ImageInput (&inputs)[RoleCount],const std::function<void(size_t)> &reserve) {
    ImageLayout result;
    for(unsigned i=0;i<RoleCount;++i){
      if(!inputs[i].image)continue;
      unsigned first=0;
      while(first<i&&inputs[first].image!=inputs[i].image)++first;
      if(first<i)result.offset[i]=result.offset[first];
      else{
        result.offset[i]=result.bytes;
        result.bytes+=inputs[i].image->rgba.size();
        result.upload[i]=true;
      }
    }
    if(reserve)reserve(result.bytes+sizeof(paint_compute::Quantization)+batch_capacity*(sizeof(Coordinates)+sizeof(Values)));
    return result;
  }

  Api api_;
  Buffer<uint8_t> images_;
  Buffer<paint_compute::Quantization> table_;
  Buffer<Coordinates> coordinates_;
  Buffer<Values> results_;
  View view_{};
  OverlayView overlay_{};
  struct ReleaseAllocation {
    Api *api;
    void operator()(void *pointer) const noexcept {api->release(pointer);}
  };
  std::vector<std::unique_ptr<void,ReleaseAllocation>> overlay_storage_;

  void load_overlay(const PaintOverlayPacket &packet,const Parameters &parameters,const std::function<bool()> &cancelled,
                    const std::function<void(size_t)> &reserve){
    using namespace emission_raster;
    emission_raster::MaterialInput material;
    const auto input=describe_bake_overlay(packet,material,parameters.separate_pbr,
                                           parameters.include_overlay_emission,cancelled);
    const auto stop=[&]{poll(cancelled);};
    const auto allocate=[&]<class T>(size_t count)->T *{
      if(!count)return nullptr;
      const auto bytes=count*sizeof(T);
      if(reserve)reserve(bytes);
      std::unique_ptr<void,ReleaseAllocation> storage(api_.allocate(bytes),{&api_});
      auto *pointer=static_cast<T *>(storage.get());
      overlay_storage_.push_back(std::move(storage));
      return pointer;
    };
    const auto copy=[&](void *to,const void *from,size_t bytes){
      try{api_.upload(to,from,bytes);api_.wait();}
      catch(...){api_.drain();throw;}
    };
    const auto upload=[&]<class T,size_t Extent>(std::span<const T,Extent> data){
      auto *to=allocate.template operator()<T>(data.size());
      constexpr size_t chunk=8u*1024*1024/sizeof(T);
      for(size_t first=0;first<data.size();){
        stop();
        const auto part=data.subspan(first,std::min(chunk,data.size()-first));
        copy(to+first,part.data(),part.size_bytes());
        first+=part.size();
      }
      return to;
    };
    auto &device=overlay_.input;
    auto &maps=overlay_.material;
    device.resolution=input.resolution;
    device.layer_count=uint32_t(input.layers.size());
    device.tiles_per_layer=(input.resolution/32)*(input.resolution/32);
    device.quantization=table_.data;
    device.padding=upload(input.padding);
    device.layers=upload(std::span(input.layers));
    device.entries=upload(std::span(input.entries));
    maps.entries=upload(std::span<const MaterialEntry>(material.entries));
    maps.common=reinterpret_cast<const paint_compute::Pixel *>(
        transfer_layer_tiles<PaintTile>(material.common,allocate,copy,stop));
    maps.pbr=reinterpret_cast<const paint_compute::ColorPixel *>(
        transfer_layer_tiles<PaintColorTile>(material.pbr,allocate,copy,stop));
    maps.bake_color=upload(material.bake_color);
    maps.bake_orm=upload(material.bake_orm);
    maps.bake_coverage=upload(material.bake_coverage);
    if(parameters.include_baked_normal){
      if(!packet.baked()||packet.baked()->maps.normal.rgba.empty())
        throw std::logic_error("Covered Bake normal lacks its baked source");
      overlay_.normal={upload(std::span(packet.baked()->maps.normal.rgba)),maps.bake_coverage,
                       device.padding,device.resolution};
    }
    if(parameters.separate_pbr&&packet.baked()&&
       packet.baked()->color_interpretation==BakedColorInterpretation::ResolvedPbr)
      maps.bake_pbr=upload(std::span(packet.baked()->maps.pbr_color.rgba));
    overlay_.separate_pbr=parameters.separate_pbr;
    overlay_.has_emission=input.physical_bake||!input.legacy_bake.empty()||!input.common.empty()||!input.physical.empty();
    if(overlay_.has_emission){
      if(input.physical_bake)transfer_bake(device.bake,input,upload);
      else if(!input.legacy_bake.empty()){
        device.bake.legacy=upload(input.legacy_bake);
        device.bake.coverage=maps.bake_coverage;
      }
      transfer_common(device,input.common,allocate,copy,stop);
      transfer_physical(device,input.physical,allocate,upload,copy,stop);
    }
    stop();
  }

  DeviceSampler(const ImageInput (&roles)[RoleCount],Parameters parameters,const PaintOverlayPacket *overlay,
                const std::function<bool()> &cancelled,const paint_compute::MaterialAtlas *atlas,
                const std::function<void(size_t)> &reserve,ImageLayout layout)
      :api_(atlas),images_(api_,layout.bytes),table_(api_,1),coordinates_(api_,batch_capacity),results_(api_,batch_capacity) {
    if(!layout.bytes&&!overlay&&!atlas)throw std::logic_error("Missing Bake material images or paint");
    view_.parameters=parameters;
    try {
      // The captured Bake field owns these planes through every batch.
      if(atlas){
        const auto resolution=atlas->resolution();
        for(unsigned role=Color;role<=Emission;++role)if(const auto *pixels=atlas->plane(role))
          view_.images[role]={{pixels,resolution,resolution,33071,33071,false,role!=Orm,
                               SourceUvSampler::Output::LinearRgba},false};
      }
      for(unsigned i=0;i<RoleCount;++i){
        const auto &role=roles[i];
        if(!role.image)continue;
        const bool srgb=i==Color||i==Emission||i==PbrColor;
        auto *pixels=images_.data+layout.offset[i];
        if(layout.upload[i])api_.upload(pixels,role.image->rgba.data(),role.image->rgba.size());
        if(role.original){
          view_.images[i]={mapped_source_image(*role.original,pixels,srgb,SourceUvSampler::Output::LinearRgba,
                                               SourceUvSampler::Transform::SourceAffine),true};
        }else{
          const auto wrap=role.repeat?10497:33071;
          view_.images[i]={{pixels,role.image->width,role.image->height,wrap,wrap,false,srgb,
                            SourceUvSampler::Output::LinearRgba},false};
        }
      }
      const auto &table=paint_compute::quantization();
      api_.upload(table_.data,&table,sizeof(table));
      api_.wait();
      if(overlay)load_overlay(*overlay,parameters,cancelled,reserve);
    }catch(...){api_.drain();throw;}
  }
  static void poll(const std::function<bool()> &cancelled){
    if(cancelled&&cancelled())throw std::runtime_error("Mesh baking cancelled");
  }
public:
  DeviceSampler(const ImageInput (&roles)[RoleCount],Parameters parameters,const PaintOverlayPacket *overlay,
                const std::function<bool()> &cancelled,const paint_compute::MaterialAtlas *atlas,
                const std::function<void(size_t)> &reserve)
      :DeviceSampler(roles,parameters,overlay,cancelled,atlas,reserve,layout(roles,reserve)){}

  void read(std::span<const Coordinates> coordinates,std::span<Values> values,
            const std::function<bool()> &cancelled) override {
    if(coordinates.empty()||coordinates.size()>batch_capacity||values.size()!=coordinates.size())
      throw std::logic_error("Invalid Bake material sample batch");
    poll(cancelled);
    try {
      api_.upload(coordinates_.data,coordinates.data(),coordinates.size_bytes());
      api_.sample_bake_material(view_,coordinates_.data,table_.data,results_.data,coordinates.size());
      if(overlay_.input.padding)
        api_.composite_bake_overlay(overlay_,coordinates_.data,results_.data,coordinates.size());
      api_.read(values.data(),results_.data,values.size_bytes());
      poll(cancelled);
      for(const auto &value:values)if(value.invalid)throw std::runtime_error("Source UV sampling exceeds finite bounds");
    }catch(...){api_.drain();throw;}
  }
};
} // namespace dome::detail::bake_material
