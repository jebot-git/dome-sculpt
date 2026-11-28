// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "source-material-raster-kernel.hpp"
#include "emission-raster-transfer.hpp"
#include "paint-composite-backend.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace dome::detail::emission_raster {
template<class Device>
class Raster final : public EmissionRasterDevice, public SourceMaterialRaster {
public:
  Raster(const Input &input,const std::function<bool()> &cancelled,const MaterialInput *material=nullptr,
         const std::function<void(size_t)> &reserve={}):reserve_(reserve){
    load(input,cancelled,material);
    reserve_={};
  }

private:
  void load(const Input &input,const std::function<bool()> &cancelled,const MaterialInput *material){
    const auto stop=[&]{paint_compute::checkpoint(cancelled);};
    view_.resolution=input.resolution;
    bake_uv_capacity_=input.bake_uv_capacity;
    if(bake_uv_capacity_){
      stop();
      upload_bake(input);
      bake_uv_triangles_=input.bake_uv_source.triangles.size();
      if(bake_uv_triangles_){
        bake_uv_geometry_={upload(input.bake_uv_source.triangles),upload(input.bake_uv_source.padding),input.resolution};
        bake_uv_points_=allocate<BakeUvPoint>(bake_uv_capacity_);
      }else bake_uv_samples_=allocate<BakeUvSample>(bake_uv_capacity_);
      if(input.uv_layer){
        const auto &layer=*input.uv_layer;
        upload_physical(input.physical,stop);
        uv_layer_={upload(std::span(layer.entries)),upload_layer_tiles<PaintTile>(input.common,stop),
            upload_layer_tiles<PaintColorTile>(layer.pbr,stop),view_.physical,view_.physical_values,
            upload(std::span(layer.tables,1)),layer.prepared};
        uv_layer_output_=allocate<paint_grid::Pixel>(bake_uv_capacity_);
        stop();
        return;
      }
      if(input.physical_bake)output_=allocate<uint32_t>(7*bake_uv_capacity_);
      if(input.bake_uv_maps){
        const auto &bake=*input.bake_uv_maps;
        bake_uv_maps_.color=upload(std::span(bake.maps.color.rgba));
        bake_uv_maps_.orm=upload(std::span(bake.maps.metal_rough.rgba));
        bake_uv_maps_.legacy=upload(std::span(bake.maps.emission.rgba));
        bake_uv_maps_.coverage=upload(std::span(bake.coverage));
        bake_uv_maps_.normals[0]=upload(std::span(bake.maps.normal.rgba));
        bake_uv_maps_.normals[1]=upload(std::span(bake.maps.mtoon_normal.rgba));
        bake_uv_maps_.quantization=upload(std::span(&paint_compute::quantization(),1));
        bake_uv_maps_.frames=upload(input.bake_uv_frames);
        bake_uv_faces_=input.bake_uv_frames.size();
        bake_uv_maps_output_=allocate<BakeUvMapsPixel>(bake_uv_capacity_);
      }
      stop();
      return;
    }
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
    mapped_field_=input.mapped_field;
    finite_={upload(std::span(input.finite_axis)),uint32_t(input.finite_axis.size())};
    if(input.bake_radiance_capacity){
      bake_radiance_capacity_=input.bake_radiance_capacity;
      output_=allocate<uint32_t>(3*bake_radiance_capacity_);
      bake_radiance_coordinates_=allocate<BakeRadianceSample>(bake_radiance_capacity_);
      if(input.summarize_bake_radiance)bake_summary_=allocate<BakeRadianceSummary>(1);
    }else if(!material)output_=allocate<uint32_t>(mapped_capacity_?mapped_capacity_*(mapped_field_?10:3):chunk_pixels*(finite_.resolution?3:10));
    if(mapped_capacity_)coordinates_=allocate<MappedSample>(mapped_capacity_);
    stop();
    upload_common(input.common,stop);
    upload_physical(input.physical,stop);

    upload_bake(input);
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

public:
  void read(size_t first,std::span<Pixel> rows,const std::function<bool()> &cancelled) override {
    static_assert(sizeof(Pixel)==10*sizeof(uint32_t));
    if(finite_.resolution||mapped_capacity_||bake_uv_capacity_||bake_radiance_capacity_)
      throw std::logic_error("Expected a contiguous exact GPU emission raster");
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
    if(mapped_field_)throw std::logic_error("Expected a finite mapped GPU emission raster");
    upload_mapped(samples,pixels.size(),finite_.resolution,cancelled);
    device_.render_mapped(view_,finite_,mapped_source_,coordinates_,samples.size(),output_);
    device_.download(pixels.data(),output_,pixels.size_bytes());
    paint_compute::checkpoint(cancelled);
    validate_finite(pixels);
  }

  void read_mapped_field(std::span<const MappedSample> samples,std::span<Pixel> pixels,
                         const std::function<bool()> &cancelled) override {
    if(!mapped_field_)throw std::logic_error("Expected an exact mapped GPU emission raster");
    upload_mapped(samples,pixels.size(),view_.resolution,cancelled);
    device_.render_mapped_field(view_,mapped_source_,coordinates_,samples.size(),output_);
    device_.download(pixels.data(),output_,pixels.size_bytes());
    paint_compute::checkpoint(cancelled);
    validate_exact(std::span<const Pixel>(pixels));
  }

  void read_bake_radiance(std::span<const BakeRadianceSample> samples,std::span<Radiance> pixels,
                          const std::function<bool()> &cancelled,BakeRadianceSummary *summary) override {
    if(samples.empty()||samples.size()!=pixels.size()||samples.size()>bake_radiance_capacity_||(summary&&!bake_summary_))
      throw std::invalid_argument("Invalid GPU Bake radiance batch");
    paint_compute::checkpoint(cancelled);
    for(const auto &sample:samples)
      if(!bake_material::finite_uv(sample.paint_uv)||!bake_material::finite_uv(sample.image_uv))
        throw std::invalid_argument("Invalid Bake radiance sample coordinate");
    device_.upload(bake_radiance_coordinates_,samples.data(),samples.size_bytes());
    device_.render_bake_radiance(view_,mapped_source_,bake_radiance_coordinates_,samples.size(),output_);
    if(summary)device_.summarize_bake_radiance(output_,samples.size(),bake_summary_);
    device_.download(pixels.data(),output_,pixels.size_bytes());
    paint_compute::checkpoint(cancelled);
    validate_exact(std::span<const Radiance>(pixels));
    if(summary){
      BakeRadianceSummary result;
      device_.download(&result,bake_summary_,sizeof(result));
      paint_compute::checkpoint(cancelled);
      summary->merge(result);
    }
  }

  void read_bake_uv(std::span<const BakeUvSample> samples,std::span<BakedEmissionSample> output,
                    const std::function<bool()> &cancelled) override {
    if(samples.size()!=output.size()||!view_.bake.physical)
      throw std::invalid_argument("Invalid GPU Bake UV filter batch");
    upload_bake_uv_taps(samples,cancelled);
    device_.filter_bake_uv(view_.bake,bake_uv_samples_,samples.size(),output_);
    download_bake_uv(output,cancelled);
  }
  void read_bake_uv_maps(std::span<const BakeUvSample> samples,std::span<BakeUvMapsPixel> maps,
                        std::span<BakedEmissionSample> emission,const std::function<bool()> &cancelled) override {
    if(samples.size()!=maps.size()||!bake_uv_maps_output_||
       (!emission.empty()&&(!view_.bake.physical||emission.size()!=samples.size())))
      throw std::invalid_argument("Invalid GPU Bake UV maps batch");
    if(bake_uv_faces_)for(const auto &sample:samples){
      if(sample.face>=bake_uv_faces_||!valid_bake_uv_bary(sample.bary))
        throw std::invalid_argument("Invalid GPU Bake UV tangent sample");
    }
    upload_bake_uv_taps(samples,cancelled);
    device_.filter_bake_uv_maps(bake_uv_maps_,view_.bake,{{},bake_uv_samples_,nullptr},samples.size(),
                                bake_uv_maps_output_,emission.empty()?nullptr:output_);
    device_.download(maps.data(),bake_uv_maps_output_,maps.size_bytes());
    download_bake_uv(emission,cancelled);
  }
  void read_bake_uv_points(std::span<const BakeUvPoint> points,std::span<BakeUvMapsPixel> maps,
                          std::span<BakedEmissionSample> emission,const std::function<bool()> &cancelled) override {
    if(!bake_uv_maps_output_||
       (maps.empty()&&emission.empty())||(!maps.empty()&&maps.size()!=points.size())||
       (!emission.empty()&&(!view_.bake.physical||emission.size()!=points.size())))
      throw std::invalid_argument("Invalid GPU Bake UV correspondence batch");
    upload_bake_uv_points(points,cancelled);
    device_.filter_bake_uv_maps(bake_uv_maps_,view_.bake,{bake_uv_geometry_,nullptr,bake_uv_points_},points.size(),
                                maps.empty()?nullptr:bake_uv_maps_output_,emission.empty()?nullptr:output_);
    if(!maps.empty())device_.download(maps.data(),bake_uv_maps_output_,maps.size_bytes());
    download_bake_uv(emission,cancelled);
  }
  void read_uv_layer(std::span<const BakeUvPoint> points,std::span<paint_grid::Pixel> output,
                     const std::function<bool()> &cancelled) override {
    if(!uv_layer_output_||points.size()!=output.size())
      throw std::invalid_argument("Invalid resident UV layer batch");
    upload_bake_uv_points(points,cancelled);
    device_.filter_uv_layer(uv_layer_,bake_uv_geometry_,bake_uv_points_,points.size(),uv_layer_output_);
    device_.download(output.data(),uv_layer_output_,output.size_bytes());
    paint_compute::checkpoint(cancelled);
  }
  void upload_bake_uv_points(std::span<const BakeUvPoint> points,const std::function<bool()> &cancelled) {
    if(!bake_uv_points_||points.empty()||points.size()>bake_uv_capacity_)
      throw std::invalid_argument("Invalid GPU UV correspondence batch");
    paint_compute::checkpoint(cancelled);
    for(size_t i=0;i<points.size();++i){
      if((i&1023)==0)paint_compute::checkpoint(cancelled);
      const auto &point=points[i];
      if(point.face!=UINT32_MAX&&(point.face>=bake_uv_triangles_||!valid_bake_uv_bary(point.bary)))
        throw std::invalid_argument("Invalid GPU UV correspondence point");
    }
    device_.upload(bake_uv_points_,points.data(),points.size_bytes());
  }
  static bool valid_bake_uv_bary(const float (&bary)[2]){
    const auto first=double(bary[0]),second=double(bary[1]),third=1.-first-second;
    const auto valid=[](double value){return value>=-.00001&&value<=1.00001;};
    return valid(first)&&valid(second)&&valid(third);
  }
  void upload_bake_uv_taps(std::span<const BakeUvSample> samples,const std::function<bool()> &cancelled) {
    if(!bake_uv_samples_||samples.empty()||samples.size()>bake_uv_capacity_)
      throw std::invalid_argument("Invalid GPU Bake UV filter taps");
    paint_compute::checkpoint(cancelled);
    const auto pixels=size_t(view_.resolution)*view_.resolution;
    for(const auto &sample:samples)for(size_t i=0;i<4;++i)
      if(sample.texels[i]>=pixels||!std::isfinite(sample.weights[i])||sample.weights[i]<0||sample.weights[i]>1)
        throw std::invalid_argument("Invalid GPU Bake UV filter tap");
    device_.upload(bake_uv_samples_,samples.data(),samples.size_bytes());
  }
  void download_bake_uv(std::span<BakedEmissionSample> output,const std::function<bool()> &cancelled){
    static_assert(sizeof(BakedEmissionSample)==7*sizeof(uint32_t));
    if(!output.empty())device_.download(output.data(),output_,output.size_bytes());
    paint_compute::checkpoint(cancelled);
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
      throw std::invalid_argument(material_.shared_shade?"Prepared MToon1 shade exceeds the portable texture range":
          "Portable source paint radiance exceeds byte material representation");
  }

private:
  void upload_bake(const Input &input){
    transfer_bake(view_.bake,input,[&](auto data){return upload(data);});
  }
  void upload_mapped(std::span<const MappedSample> samples,size_t count,size_t resolution,
                     const std::function<bool()> &cancelled){
    if(samples.size()!=count||samples.empty()||samples.size()>mapped_capacity_)
      throw std::invalid_argument("Invalid mapped GPU emission batch");
    paint_compute::checkpoint(cancelled);
    const auto size=resolution*resolution;
    for(const auto &sample:samples)
      if(sample.texel>=size||!std::isfinite(sample.u)||!std::isfinite(sample.v))
        throw std::invalid_argument("Invalid mapped GPU emission sample");
    device_.upload(coordinates_,samples.data(),samples.size_bytes());
  }
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
    std::ranges::copy(input.shade_factor,material_.shade_factor);
    material_.shared_shade=input.shared_shade;
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
  template<class Output>
  static void validate_exact(std::span<const Output> pixels){
    for(const auto &pixel:pixels){
      const auto status=std::bit_cast<uint32_t>(pixel[0]);
      if(status==invalid_uv)throw std::runtime_error("MToon UV overflow");
      if(status==source_radiance_overflow)throw std::overflow_error("Emission Bake source multiplication overflows FLOAT");
      if(status==radiance_overflow)throw std::overflow_error("Baked emission radiance exceeds FLOAT range");
      for(unsigned k=0;k<3;++k)if(!std::isfinite(pixel[k]))
        throw std::invalid_argument("GPU emission produced an invalid native FLOAT coefficient or radiance");
    }
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
      validate_exact(std::span<const Output>(batch));
      pixels=pixels.subspan(batch.size());
      first+=batch.size();
    }
    paint_compute::checkpoint(cancelled);
  }

  template<class T>T *allocate(size_t count){
    if(!count)return nullptr;
    const auto bytes=count*sizeof(T);
    if(reserve_)reserve_(bytes);
    return static_cast<T *>(device_.allocate(bytes));
  }
  template<class T,size_t Extent> const T *upload(std::span<const T,Extent> data){
    auto *buffer=allocate<T>(data.size());
    if(buffer)device_.upload(buffer,data.data(),data.size_bytes());
    return buffer;
  }
  auto raster_allocate(){return [this]<class T>(size_t count){return allocate<T>(count);};}
  auto raster_copy(){return [this](void *to,const void *from,size_t bytes){device_.upload(to,from,bytes);};}
  template<class T,class Stop> const uint8_t *upload_layer_tiles(std::span<const T *const> tiles,Stop stop){
    return transfer_layer_tiles<T>(tiles,raster_allocate(),raster_copy(),stop);
  }
  template<class Stop> void upload_common(std::span<const PaintTile *const> tiles,Stop stop){
    transfer_common(view_,tiles,raster_allocate(),raster_copy(),stop);
  }
  template<class Stop> void upload_physical(std::span<const PhysicalTile> tiles,Stop stop){
    transfer_physical(view_,tiles,raster_allocate(),[this](auto data){return upload(data);},raster_copy(),stop);
  }

  Device device_;
  std::function<void(size_t)> reserve_;
  DeviceInput view_;
  FiniteGrid finite_;
  MappedSourceImage mapped_source_{};
  MappedSample *coordinates_=nullptr;
  BakeRadianceSample *bake_radiance_coordinates_=nullptr;
  BakeRadianceSummary *bake_summary_=nullptr;
  BakeUvSample *bake_uv_samples_=nullptr;
  BakeUvPoint *bake_uv_points_=nullptr;
  BakeUvGeometry bake_uv_geometry_{};
  BakeUvMaps bake_uv_maps_{};
  BakeUvMapsPixel *bake_uv_maps_output_=nullptr;
  UvLayer uv_layer_{};
  paint_grid::Pixel *uv_layer_output_=nullptr;
  size_t bake_uv_faces_=0,bake_uv_triangles_=0;
  size_t mapped_capacity_=0;
  size_t bake_uv_capacity_=0;
  size_t bake_radiance_capacity_=0;
  bool mapped_field_=false;
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
