// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "source-material-raster-kernel.hpp"
#include "paint-composite-backend.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dome::detail::emission_raster {
template<class Device>
class Raster final : public EmissionRasterDevice, public SourceMaterialRaster {
public:
  Raster(const Input &input,const std::function<bool()> &cancelled,const MaterialInput *material=nullptr){
    const auto stop=[&]{paint_compute::checkpoint(cancelled);};
    view_.resolution=input.resolution;
    view_.layer_count=uint32_t(input.layers.size());
    view_.tiles_per_layer=(input.resolution/32)*(input.resolution/32);
    for(unsigned k=0;k<3;++k)view_.background[k]=input.background[k];
    view_.strength=input.strength;
    view_.padding=upload(std::span(input.padding));
    view_.layers=upload(std::span(input.layers));
    view_.entries=upload(std::span(input.entries));
    view_.quantization=upload(std::span(&paint_compute::quantization(),1));
    texture_set_=input.texture_set;
    mapped_capacity_=input.mapped_capacity;
    finite_={upload(std::span(input.finite_axis)),uint32_t(input.finite_axis.size())};
    if(!material)output_=allocate<uint32_t>(mapped_capacity_?mapped_capacity_*3:chunk_pixels*(finite_.resolution?3:10));
    if(mapped_capacity_)coordinates_=allocate<MappedSample>(mapped_capacity_);
    stop();
    upload_common(input.common,stop);
    upload_physical(input.physical,stop);

    auto &bake=view_.bake;
    bake.physical=input.physical_bake;
    if(bake.physical){
      for(unsigned k=0;k<7;++k)bake.packed.constants[k]=std::bit_cast<uint32_t>(input.bake.constants[k]);
      bake.packed.mask=input.bake.varying_mask;
      bake.packed.stride=std::popcount(input.bake.varying_mask);
      bake.dense=reinterpret_cast<const uint32_t *>(upload(std::as_bytes(input.bake.dense)));
      bake.varying=reinterpret_cast<const uint32_t *>(upload(input.bake.varying));
      bake.coverage_bits=upload(input.bake.binary_coverage);
    }else{
      bake.legacy=upload(input.legacy_bake);
      bake.coverage=upload(input.bake_coverage);
    }
    stop();
    if(material)upload_material(*material,cancelled);
    if(input.mapped_source){
      mapped_source_=mapped_source_image(*input.mapped_source,upload(input.source),true,
          SourceUvSampler::Output::LinearRgba,SourceUvSampler::Transform::SourceAffine);
    }else if(!input.source.empty()){
      view_.source=input.source_sampler;
      view_.source.rgba=upload(input.source);
      view_.source.coordinates.columns=upload(std::span(input.columns));
      view_.source.coordinates.rows=upload(std::span(input.rows));
      view_.source.aligned_mtoon_linear=upload(input.aligned_mtoon_linear);
    }
    stop();
  }

  void read(size_t first,std::span<Pixel> rows,const std::function<bool()> &cancelled) override {
    static_assert(sizeof(Pixel)==10*sizeof(uint32_t));
    if(finite_.resolution)throw std::logic_error("Expected an exact GPU emission raster");
    read_exact(first,rows,cancelled,[&](size_t at,size_t count){
      if(texture_set_)device_.render_texture_set(view_,at,count,output_,true);
      else device_.render(view_,at,count,output_);
    });
  }

  void read_texture_set_preview(size_t first,std::span<TextureSetPreview> pixels,const std::function<bool()> &cancelled) override {
    static_assert(sizeof(TextureSetPreview)==4*sizeof(uint32_t));
    if(!texture_set_)throw std::logic_error("Expected a texture-set GPU emission raster");
    read_exact(first,pixels,cancelled,[&](size_t at,size_t count){
      device_.render_texture_set(view_,at,count,output_,false);
    });
  }

  void read_finite(size_t first,std::span<Radiance> pixels,const std::function<bool()> &cancelled) override {
    static_assert(sizeof(Radiance)==3*sizeof(uint32_t));
    if(!finite_.resolution||mapped_capacity_)throw std::logic_error("Expected a contiguous finite GPU emission raster");
    const auto size=size_t(finite_.resolution)*finite_.resolution;
    if(first>size||pixels.size()>size-first)throw std::out_of_range("GPU finite emission read exceeds its grid");
    while(!pixels.empty()){
      paint_compute::checkpoint(cancelled);
      auto batch=pixels.first(std::min(pixels.size(),chunk_pixels));
      device_.render_finite(view_,finite_,first,batch.size(),output_);
      device_.download(batch.data(),output_,batch.size_bytes());
      paint_compute::checkpoint(cancelled);
      validate_finite(batch);
      pixels=pixels.subspan(batch.size());
      first+=batch.size();
    }
    paint_compute::checkpoint(cancelled);
  }

  void read_mapped(std::span<const MappedSample> samples,std::span<Radiance> pixels,
                   const std::function<bool()> &cancelled) override {
    static_assert(sizeof(Radiance)==3*sizeof(uint32_t));
    if(samples.size()!=pixels.size()||samples.empty()||samples.size()>mapped_capacity_)
      throw std::invalid_argument("Invalid mapped GPU emission batch");
    paint_compute::checkpoint(cancelled);
    const auto size=size_t(finite_.resolution)*finite_.resolution;
    for(const auto &sample:samples)
      if(sample.texel>=size||!std::isfinite(sample.u)||!std::isfinite(sample.v))
        throw std::invalid_argument("Invalid mapped GPU emission sample");
    device_.upload(coordinates_,samples.data(),samples.size_bytes());
    device_.render_mapped(view_,finite_,mapped_source_,coordinates_,samples.size(),output_);
    device_.download(pixels.data(),output_,pixels.size_bytes());
    paint_compute::checkpoint(cancelled);
    validate_finite(pixels);
  }

  size_t capacity() const noexcept override {return material_coordinates_.size();}
  void read_material(std::span<const uint32_t> texels,const Coordinates &coordinates,
      std::span<paint_compute::Output> pixels,const std::function<bool()> &cancelled) override {
    const auto stop=[&]{paint_compute::checkpoint(cancelled);};
    if(texels.empty()||texels.size()!=pixels.size()||texels.size()>capacity())
      throw std::invalid_argument("Invalid source material GPU batch");
    stop();
    const auto size=size_t(finite_.resolution)*finite_.resolution;
    for(const auto at:texels)if(at>=size)throw std::invalid_argument("Invalid source material texel");
    device_.upload(material_texels_,texels.data(),texels.size_bytes());
    auto uv=std::span(material_coordinates_).first(texels.size());
    int previous_set=-1;
    for(const auto role:material_order_){
      const auto &image=material_roles_[role];
      if(!image.rgba)continue;
      stop();
      if(material_sets_[role]!=previous_set){
        coordinates(role,uv);
        for(const auto &q:uv)if(!std::isfinite(q.u)||!std::isfinite(q.v))
          throw std::invalid_argument("Invalid source material sample coordinate");
        device_.upload(material_uv_,uv.data(),uv.size_bytes());
        previous_set=material_sets_[role];
      }
      if(rolling_image_)device_.upload(rolling_image_,source_images_[role]->rgba.data(),
                                      source_images_[role]->rgba.size());
      device_.sample_material(image,view_.quantization,material_uv_,texels.size(),material_sources_.values[role]);
    }
    device_.render_material(view_,material_,finite_,material_sources_,material_texels_,texels.size(),material_output_);
    device_.download(pixels.data(),material_output_,pixels.size_bytes());
    stop();
    for(const auto &pixel:pixels)if(pixel.reserved)
      throw std::invalid_argument("Portable source paint radiance exceeds byte material representation");
  }

private:
  void upload_material(const MaterialInput &input,const std::function<bool()> &cancelled){
    static_assert(sizeof(PaintPixel)==sizeof(paint_compute::Pixel));
    static_assert(sizeof(PaintColorTile)==tile_pixels*sizeof(paint_compute::ColorPixel));
    const auto stop=[&]{paint_compute::checkpoint(cancelled);};
    material_.entries=upload(std::span(input.entries));
    auto *common=allocate<paint_compute::Pixel>(input.common.size()*tile_pixels);
    material_.common=common;
    for(const auto *tile:input.common){
      stop();
      device_.upload(common,tile->pixels.data(),sizeof(tile->pixels));
      common+=tile_pixels;
    }
    auto *pbr=allocate<paint_compute::ColorPixel>(input.pbr.size()*tile_pixels);
    material_.pbr=pbr;
    for(const auto *tile:input.pbr){
      stop();
      device_.upload(pbr,tile->pixels.data(),sizeof(tile->pixels));
      pbr+=tile_pixels;
    }
    material_.bake_color=upload(input.bake_color);
    material_.bake_orm=upload(input.bake_orm);
    material_.bake_coverage=upload(input.bake_coverage);
    material_.factors=input.factors;
    material_.occlusion_strength=input.occlusion_strength;
    material_coordinates_.resize(input.capacity);
    material_texels_=allocate<uint32_t>(input.capacity);
    material_uv_=allocate<SourceUvSampler::Coordinate>(input.capacity);
    material_output_=allocate<paint_compute::Output>(input.capacity);
    if(!input.resident)rolling_image_=allocate<uint8_t>(input.largest_image);
    for(unsigned role=0;role<input.roles.size();++role){
      material_order_[role]=role;
      const auto *source=input.roles[role];
      if(!source)continue;
      stop();
      const auto &image=source->image;
      source_images_[role]=image;
      material_sets_[role]=source->texcoord;
      const uint8_t *bytes=rolling_image_;
      if(input.resident){
        const auto prior=std::find(source_images_.begin(),source_images_.begin()+role,image);
        bytes=prior==source_images_.begin()+role?upload(std::span(image->rgba)):
            material_roles_[size_t(prior-source_images_.begin())].rgba;
      }
      material_roles_[role]=mapped_source_image(*source,bytes,
          role==paint_compute::SourceColor||role==paint_compute::SourceEmission,
          SourceUvSampler::Output::LinearRgba,SourceUvSampler::Transform::SourceAffine);
      material_sources_.values[role]=allocate<uint32_t>(4*input.capacity);
    }
    std::ranges::sort(material_order_,{},[&](auto role){return material_sets_[role];});
    stop();
  }
  static void validate_finite(std::span<const Radiance> pixels){
    for(const auto &pixel:pixels)if(std::bit_cast<uint32_t>(pixel[0])==invalid)
      throw std::invalid_argument("Native physical radiance is outside its finite range");
  }
  template<class Output,class Render>
  void read_exact(size_t first,std::span<Output> pixels,const std::function<bool()> &cancelled,Render render){
    const auto size=size_t(view_.resolution)*view_.resolution;
    if(first>size||pixels.size()>size-first)throw std::out_of_range("GPU emission read exceeds its owning grid");
    while(!pixels.empty()){
      paint_compute::checkpoint(cancelled);
      auto batch=pixels.first(std::min(pixels.size(),chunk_pixels));
      render(first,batch.size());
      device_.download(batch.data(),output_,batch.size_bytes());
      for(const auto &pixel:batch){
        const auto status=std::bit_cast<uint32_t>(pixel[0]);
        if(status==invalid_uv)throw std::runtime_error("MToon UV overflow");
        if(status==radiance_overflow)throw std::overflow_error("Baked emission radiance exceeds FLOAT range");
        for(unsigned k=0;k<3;++k)if(!std::isfinite(pixel[k]))
          throw std::invalid_argument("GPU emission produced an invalid native FLOAT coefficient or radiance");
      }
      pixels=pixels.subspan(batch.size());
      first+=batch.size();
    }
    paint_compute::checkpoint(cancelled);
  }

  template<class T>T *allocate(size_t count){
    return count?static_cast<T *>(device_.allocate(count*sizeof(T))):nullptr;
  }
  template<class T,size_t Extent> const T *upload(std::span<const T,Extent> data){
    auto *buffer=allocate<T>(data.size());
    if(buffer)device_.upload(buffer,data.data(),data.size_bytes());
    return buffer;
  }
  template<class Stop> void upload_common(std::span<const PaintTile *const> tiles,Stop stop){
    if(tiles.empty())return;
    auto *destination=allocate<CommonPixel>(tiles.size()*tile_pixels);
    view_.common=destination;
    constexpr size_t tiles_per_batch=32;
    std::vector<CommonPixel> staging(std::min(tiles.size(),tiles_per_batch)*tile_pixels);
    while(!tiles.empty()){
      stop();
      const auto batch=tiles.first(std::min(tiles.size(),tiles_per_batch));
      size_t next=0;
      for(const auto *tile:batch)for(const auto &pixel:tile->pixels){
        const auto &data=pixel.data;
        staging[next++]={{data[6],data[7],data[8]},data[12],data[13]};
      }
      device_.upload(destination,staging.data(),next*sizeof(CommonPixel));
      destination+=next;
      tiles=tiles.subspan(batch.size());
    }
  }
  template<class Stop> void upload_physical(std::span<const PhysicalTile> tiles,Stop stop){
    if(tiles.empty())return;
    const auto &last=tiles.back();
    const size_t words=size_t(last.field.offset)+last.samples.size();
    auto *destination=allocate<uint32_t>(words);
    view_.physical_values=destination;
    std::vector<PackedField> fields;
    fields.reserve(tiles.size());
    std::vector<float> staging;
    constexpr size_t words_per_batch=65536;
    staging.reserve(std::min(words,words_per_batch));
    auto flush=[&]{
      if(staging.empty())return;
      device_.upload(destination,staging.data(),staging.size()*sizeof(float));
      destination+=staging.size();
      staging.clear();
    };
    for(size_t i=0;i<tiles.size();++i){
      if((i&255)==0)stop();
      const auto &tile=tiles[i];
      fields.push_back(tile.field);
      if(staging.size()+tile.samples.size()>words_per_batch)flush();
      staging.insert(staging.end(),tile.samples.begin(),tile.samples.end());
    }
    flush();
    view_.physical=upload(std::span<const PackedField>(fields));
  }

  Device device_;
  DeviceInput view_;
  FiniteGrid finite_;
  MappedSourceImage mapped_source_{};
  MappedSample *coordinates_=nullptr;
  size_t mapped_capacity_=0;
  bool texture_set_=false;
  uint32_t *output_=nullptr;
  MaterialView material_;
  MaterialSources material_sources_;
  std::array<MappedSourceImage,paint_compute::SourceRoleCount> material_roles_{};
  std::array<std::shared_ptr<const Image>,paint_compute::SourceRoleCount> source_images_{};
  std::array<int,paint_compute::SourceRoleCount> material_sets_{};
  std::array<unsigned,paint_compute::SourceRoleCount> material_order_{};
  uint8_t *rolling_image_=nullptr;
  std::vector<SourceUvSampler::Coordinate> material_coordinates_;
  SourceUvSampler::Coordinate *material_uv_=nullptr;
  uint32_t *material_texels_=nullptr;
  paint_compute::Output *material_output_=nullptr;
};
}
