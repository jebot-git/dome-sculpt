// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake-device-buffer.hpp"
#include "bake-material-kernel.hpp"
#include "dome/paint.hpp"
#include "paint-composite-backend.hpp"

namespace dome::detail::bake_material {
template<class Api> class DeviceSampler final:public Sampler {
  template<class T> using Buffer=BakeDeviceBuffer<Api,T>;
  struct ImageLayout {
    size_t offset[RoleCount]{},bytes=0;
    bool upload[RoleCount]{};
  };
  static ImageLayout layout(const ImageInput (&inputs)[RoleCount]) {
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
    return result;
  }

  Api api_;
  Buffer<uint8_t> images_;
  Buffer<paint_compute::Quantization> table_;
  Buffer<Coordinates> coordinates_;
  Buffer<Values> results_;
  View view_{};

  DeviceSampler(const ImageInput (&roles)[RoleCount],Parameters parameters,ImageLayout layout)
      :images_(api_,layout.bytes),table_(api_,1),coordinates_(api_,batch_capacity),results_(api_,batch_capacity) {
    if(!layout.bytes)throw std::logic_error("Missing Bake material images");
    view_.parameters=parameters;
    try {
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
    }catch(...){api_.drain();throw;}
  }
  static void poll(const std::function<bool()> &cancelled){
    if(cancelled&&cancelled())throw std::runtime_error("Mesh baking cancelled");
  }
public:
  DeviceSampler(const ImageInput (&roles)[RoleCount],Parameters parameters)
      :DeviceSampler(roles,parameters,layout(roles)){}

  void read(std::span<const Coordinates> coordinates,std::span<Values> values,
            const std::function<bool()> &cancelled) override {
    if(coordinates.empty()||coordinates.size()>batch_capacity||values.size()!=coordinates.size())
      throw std::logic_error("Invalid Bake material sample batch");
    poll(cancelled);
    try {
      api_.upload(coordinates_.data,coordinates.data(),coordinates.size_bytes());
      api_.sample_bake_material(view_,coordinates_.data,table_.data,results_.data,coordinates.size());
      api_.read(values.data(),results_.data,values.size_bytes());
      poll(cancelled);
      for(const auto &value:values)if(value.invalid)throw std::runtime_error("Source UV sampling exceeds finite bounds");
    }catch(...){api_.drain();throw;}
  }
};
} // namespace dome::detail::bake_material
