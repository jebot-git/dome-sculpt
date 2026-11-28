// SPDX-License-Identifier: GPL-3.0-or-later

#include "emission-raster-device.hpp"
#include "paint-grid-filter-kernel.hpp"
#include "paint-overlay-packet.hpp"
#include "emission-paint-access.hpp"
#include "dome/mtoon.hpp"
#include "dome/scene.hpp"
#include "paint-composite-backend.hpp"
#include "dome/source-maps.hpp"
#include "source-texture-grid.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <unordered_map>

namespace dome::detail {
namespace {
SourceTextureProducts typed_products(const PaintSourceTexture &source,const MToonTexture &slot,uint32_t r,
                                     bool &coupled){
  source.validate();
  const auto bits=[](double value){return std::bit_cast<uint64_t>(value);};
  const auto center=[r](uint32_t at){return double((float(at)+.5f)/r);};
  const auto transformed=[&](uint32_t at,unsigned axis){return center(at)*slot.scale[axis]+slot.offset[axis];};
  const auto representable=[&](unsigned axis){
    constexpr double limit=std::numeric_limits<float>::max();
    return std::abs(transformed(0,axis))<=limit&&std::abs(transformed(r-1,axis))<=limit;
  };
  // Each axis is monotone before FLOAT rounding. If its endpoints are valid,
  // store the already-rounded axis in the retained affine sampler's terms.
  // Overflowing axes stay with the per-texel device validation.
  coupled=slot.rotation!=0||!representable(0)||!representable(1);
  SourceTextureProducts result;
  result.columns.reserve(r);
  result.rows.reserve(r);
  if(!coupled){
    const auto &m=source.uv_transform;
    result.transform={bits(m[4]),bits(m[5]),source.wrap_s,source.wrap_t};
    for(uint32_t i=0;i<r;++i){
      const double x=float(transformed(i,0)),y=float(transformed(i,1));
      result.columns.push_back({bits(m[0]*x),bits(m[2]*x)});
      result.rows.push_back({bits(m[1]*y),bits(m[3]*y)});
    }
    return result;
  }
  const double cosine=std::cos(slot.rotation),sine=std::sin(slot.rotation);
  result.transform={bits(slot.offset[0]),bits(slot.offset[1]),source.wrap_s,source.wrap_t};
  for(uint32_t i=0;i<r;++i){
    const double x=center(i)*slot.scale[0],y=center(i)*slot.scale[1];
    result.columns.push_back({bits(cosine*x),bits(sine*x)});
    result.rows.push_back({bits(-(sine*y)),bits(cosine*y)});
  }
  return result;
}
}
emission_raster::PhysicalTile EmissionExportAccess::emission_tile(const PaintEmissionTile &tile){
  emission_raster::PhysicalTile view;
  for(unsigned k=0;k<7;++k)view.field.constants[k]=std::bit_cast<uint32_t>(tile.constants_[k]);
  view.field.mask=tile.varying_mask_;
  view.field.stride=tile.varying_count_;
  if(tile.varying_count_)view.samples={tile.values_.get(),tile.sample_count*tile.varying_count_};
  return view;
}

size_t emission_raster_staging_bytes(const PaintedMesh &paint) noexcept {
  // The table and sparse references grow with authored tiles. Full FLOAT
  // payloads stay in their existing owners until the bounded upload.
  size_t bytes=768u*1024;
  for(const auto &layer:paint.layers()){
    bytes+=sizeof(layer)+(layer.tiles.size()+layer.pbr_tiles.size()+layer.emission_tiles.size())*
           sizeof(std::shared_ptr<const PaintTile>);
    if(layer.visible&&layer.opacity>0){
      bytes+=layer.tiles.size()*sizeof(emission_raster::Entry)*2+sizeof(emission_raster::Layer)*2;
      for(const auto &tile:layer.tiles)if(tile)bytes+=256;
    }
  }
  return bytes;
}

namespace {
template<class Layers>
emission_raster::Input describe_raster(Layers &&layers,const BakedTextures *baked,uint32_t resolution,
    std::span<const int32_t> padding,
    std::array<float,3> background,float strength,bool authoring_blend,const std::function<bool()> &cancelled,
    emission_raster::MaterialInput *material,bool include_emission,bool include_pbr){
  using namespace emission_raster;
  const auto stop=[&]{paint_compute::checkpoint(cancelled);};
  stop();
  if(!std::isfinite(strength)||strength<0||std::any_of(background.begin(),background.end(),
      [](float color){return !std::isfinite(color)||color<0;}))
    throw std::invalid_argument("Invalid native HDR background or legacy strength");
  Input input;
  input.padding=padding;
  input.resolution=resolution;
  for(unsigned k=0;k<3;++k)input.background[k]=std::bit_cast<uint32_t>(background[k]);
  input.strength=std::bit_cast<uint32_t>(strength);
  if(const auto *bake=include_emission?baked:nullptr){
    if(bake->emission_field){
      input.physical_bake=true;
      input.bake=bake->emission_field->storage();
    }else{
      input.legacy_bake=bake->maps.emission.rgba;
      input.bake_coverage=bake->coverage;
    }
  }
  std::unordered_map<const PaintTile *,uint32_t> common_ids;
  std::unordered_map<const PaintEmissionTile *,uint32_t> physical_ids;
  std::unordered_map<const PaintTile *,uint32_t> material_ids;
  std::unordered_map<const PaintColorTile *,uint32_t> pbr_ids;
  uint32_t coefficient_offset=0;
  for(const auto &layer:layers){
    if constexpr(requires{layer.visible;})if(!layer.visible||layer.opacity<=0)continue;
    stop();
    const auto first_entry=input.entries.size();
    bool contributes=false;
    auto &scales=input.layers.emplace_back().opacity;
    for(unsigned mask=0;mask<scales.size();++mask){
      const float scale=authoring_blend?(float(mask)/255.f)*layer.opacity:(layer.opacity*float(mask))/255.f;
      scales[mask]=std::bit_cast<uint32_t>(scale);
    }
    for(size_t i=0;i<layer.tiles.size();++i){
      if((i&255)==0)stop();
      auto &entry=input.entries.emplace_back();
      const auto *tile=layer.tiles[i].get();
      const auto *physical=!include_emission||layer.emission_tiles.empty()?nullptr:layer.emission_tiles[i].get();
      if(material){
        auto &material_entry=material->entries.emplace_back();
        if(tile&&!PaintTile::is_shared_default(tile)){
          contributes=true;
          const auto [id,added]=material_ids.try_emplace(tile,material->common.size());
          material_entry.common=id->second;
          if(added)material->common.push_back(tile);
          if(const auto *pbr=!include_pbr||layer.pbr_tiles.empty()?nullptr:layer.pbr_tiles[i].get()){
            const auto [color,inserted]=pbr_ids.try_emplace(pbr,material->pbr.size());
            material_entry.pbr=color->second;
            if(inserted)material->pbr.push_back(pbr);
          }
        }
      }
      const bool use_mask=include_emission&&(physical||(tile&&std::any_of(tile->pixels.begin(),tile->pixels.end(),
          [](const auto &pixel){return pixel.data[12]!=0;})));
      if(use_mask&&tile&&!PaintTile::is_shared_default(tile)){
        contributes=true;
        const auto [found,inserted]=common_ids.try_emplace(tile,input.common.size());
        entry.common=found->second;
        if(inserted)input.common.push_back(tile);
      }
      if(physical&&(!authoring_blend||tile)){
        contributes=true;
        const auto [found,inserted]=physical_ids.try_emplace(physical,input.physical.size());
        entry.physical=found->second;
        if(inserted){
          auto &view=input.physical.emplace_back(EmissionExportAccess::emission_tile(*physical));
          if(view.samples.size()>UINT32_MAX-coefficient_offset)
            throw std::length_error("GPU emission tile offsets exceed the native raster range");
          view.field.offset=coefficient_offset;
          coefficient_offset+=uint32_t(view.samples.size());
        }
      }
    }
    if(!contributes){
      input.layers.pop_back();
      input.entries.resize(first_entry);
      if(material)material->entries.resize(first_entry);
    }
  }
  stop();
  return input;
}

emission_raster::Input describe_raster(const PaintedMesh &paint,std::span<const int32_t> padding,
    std::array<float,3> background,float strength,bool authoring_blend,const std::function<bool()> &cancelled,
    emission_raster::MaterialInput *material=nullptr,bool include_emission=true){
  return describe_raster(paint.layers(),paint.baked().get(),paint.resolution(),padding,background,
                        strength,authoring_blend,cancelled,material,include_emission,true);
}

void describe_source(emission_raster::Input &input,const PaintSourceTexture *source,
                     const MToonTexture *typed,uint32_t extent){
  if(!source||!source->image)return;
  auto products=typed?typed_products(*source,*typed,extent,input.source_sampler.typed_transform):
                      source_texture_products(*source,extent,extent);
  input.source=source->image->rgba;
  input.source_sampler.coordinates={nullptr,nullptr,nullptr,products.transform,
      source->image->width,source->image->height,source->mag_filter==9728};
  if(input.source_sampler.typed_transform)for(unsigned k=0;k<source->uv_transform.size();++k)
    input.source_sampler.sampler_affine[k]=std::bit_cast<uint32_t>(source->uv_transform[k]);
  input.columns=std::move(products.columns);
  input.rows=std::move(products.rows);
}

void describe_finite_grid(emission_raster::Input &input,uint32_t resolution,float offset){
  const int last=int(input.resolution)-1;
  input.finite_axis.reserve(resolution);
  for(uint32_t i=0;i<resolution;++i){
    const float uv=(float(i)+.5f)/resolution-offset;
    const float at=std::clamp(uv,0.f,1.f)*input.resolution-.5f;
    const int first=int(std::floor(at));
    input.finite_axis.push_back({std::clamp(first,0,last),std::clamp(first+1,0,last),at-first});
  }
}

void plan_material_upload(const emission_raster::Input &input,emission_raster::MaterialInput &material,
    uint32_t resolution,size_t limit){
  using namespace emission_raster;
  // Account for the active raster owners on the device and their transient
  // planning/upload storage. Texture sources share one allocation per image.
  size_t resident_bytes=input.padding.size_bytes()+sizeof(paint_compute::Quantization)+
      resolution*sizeof(SourceTextureAxis)+input.layers.size()*sizeof(Layer)+input.entries.size()*sizeof(Entry)+
      material.entries.size()*sizeof(MaterialEntry)+input.common.size()*tile_pixels*sizeof(CommonPixel)+
      material.common.size()*tile_pixels*sizeof(PaintPixel)+material.pbr.size()*sizeof(PaintColorTile)+
      input.legacy_bake.size()+input.bake_coverage.size()+input.bake.dense.size_bytes()+
      input.bake.varying.size_bytes()+input.bake.binary_coverage.size_bytes()+
      material.bake_color.size()+material.bake_orm.size()+material.bake_coverage.size();
  size_t physical_words=0;
  for(const auto &tile:input.physical)physical_words+=tile.samples.size();
  resident_bytes+=physical_words*sizeof(float)+input.physical.size()*sizeof(PackedField);
  const size_t planning_bytes=input.layers.capacity()*sizeof(Layer)+input.entries.capacity()*sizeof(Entry)+
      material.entries.capacity()*sizeof(MaterialEntry)+resolution*sizeof(SourceTextureAxis)+
      (input.common.capacity()+material.common.capacity()+material.pbr.capacity())*sizeof(void *)+
      input.physical.capacity()*sizeof(PhysicalTile);
  const size_t upload_bytes=std::max(std::min(input.common.size(),size_t(32))*tile_pixels*sizeof(CommonPixel),
      std::min(physical_words,size_t(65536))*sizeof(float)+input.physical.size()*sizeof(PackedField));
  const auto fixed=resident_bytes+planning_bytes+upload_bytes;
  size_t image_bytes=0,active=0;
  std::array<const Image *,paint_compute::SourceRoleCount> images{};
  for(size_t role=0;role<images.size();++role)if(material.roles[role]){
    ++active;
    images[role]=material.roles[role]->image.get();
    if(std::find(images.begin(),images.begin()+role,images[role])==images.begin()+role){
      const auto bytes=images[role]->rgba.size();
      image_bytes+=bytes;
      material.largest_image=std::max(material.largest_image,bytes);
    }
  }
  constexpr auto coordinate_bytes=sizeof(SourceUvSampler::Coordinate);
  const auto sample_bytes=2*(sizeof(uint32_t)+sizeof(paint_compute::Output)+coordinate_bytes)+
      active*4*sizeof(uint32_t);
  const auto preferred=std::min(size_t(resolution)*resolution,SourceUvSampler::max_batch);
  if(fixed>=limit||material.largest_image>=limit-fixed)
    throw std::length_error("Portable source material exceeds raster sampling working budget");
  const auto remaining=limit-fixed;
  material.resident=image_bytes==material.largest_image||
      (image_bytes<=remaining&&preferred<=(remaining-image_bytes)/sample_bytes);
  const auto source_bytes=material.resident?image_bytes:material.largest_image;
  material.capacity=std::min(preferred,(remaining-source_bytes)/sample_bytes);
  if(!material.capacity)throw std::length_error("Portable source material lacks sampler batch storage");
}
}

emission_raster::Input emission_raster::describe_bake_overlay(const PaintOverlayPacket &packet,
    MaterialInput &material,bool separate_pbr,bool include_emission,const std::function<bool()> &cancelled){
  const auto *bake=packet.baked().get();
  if(bake){
    material.bake_color=bake->maps.color.rgba;
    material.bake_orm=bake->maps.metal_rough.rgba;
    material.bake_coverage=bake->coverage;
  }
  return describe_raster(packet.layers(),bake,packet.resolution(),*packet.padding(),{},1,true,
                         cancelled,&material,include_emission,separate_pbr);
}

std::unique_ptr<SourceMaterialRaster> create_source_material_raster(const PaintedMesh &paint,
    const Material &material,const SourceMaterialRaster::Roles &roles,float occlusion_strength,
    uint32_t resolution,size_t working_bytes,const std::function<bool()> &cancelled,bool include_emission,
    std::optional<std::array<float,3>> shared_shade){
  if(!resolution||resolution>8192)throw std::invalid_argument("Invalid portable source material grid");
  emission_raster::MaterialInput raster;
  if(shared_shade){
    if(include_emission||!roles[paint_compute::SourceColor]||std::ranges::any_of(*shared_shade,
        [](float x){return !std::isfinite(x)||x<0||x>1;}))
      throw std::invalid_argument("Shared shade requires the original color image and the spare emission output");
    raster.shared_shade=true;
    raster.shade_factor=*shared_shade;
  }
  raster.roles=roles;
  for(const auto *role:roles)if(role&&!role->image)
    throw std::invalid_argument("Portable source material needs actual role images");
  std::copy(material.color.begin(),material.color.end(),raster.factors.color);
  std::copy(material.emissive.begin(),material.emissive.end(),raster.factors.emission);
  raster.factors.roughness=material.roughness;
  raster.factors.metallic=material.metallic;
  raster.occlusion_strength=occlusion_strength;
  if(const auto &bake=paint.baked()){
    raster.bake_coverage=bake->coverage;
    raster.bake_color=(bake->color_interpretation==BakedColorInterpretation::ResolvedPbr?
        bake->maps.pbr_color:bake->maps.color).rgba;
    raster.bake_orm=bake->maps.metal_rough.rgba;
  }
  auto input=describe_raster(paint,EmissionExportAccess::writable_padding(paint),{},1,true,cancelled,&raster,include_emission);
  describe_finite_grid(input,resolution,0);
  plan_material_upload(input,raster,resolution,working_bytes);
  return emission_raster::upload_material(input,raster,cancelled);
}

std::unique_ptr<EmissionRasterDevice> create_emission_raster_device(
    const PaintedMesh &paint,std::span<const int32_t> padding,const PaintSourceTexture *source,
    const MToonTexture *typed,std::array<float,3> background,float strength,const std::function<bool()> &cancelled){
  auto input=describe_raster(paint,padding,background,strength,false,cancelled);
  describe_source(input,source,typed,input.resolution);
  return emission_raster::upload(input,cancelled);
}

std::unique_ptr<EmissionRasterDevice> create_bake_uv_emission_device(const BakedEmissionField &bake,
    size_t capacity,const std::function<bool()> &cancelled){
  paint_compute::checkpoint(cancelled);
  if(!capacity||capacity>EmissionRasterDevice::mapped_batch||capacity>bake.sample_count())
    throw std::invalid_argument("Invalid GPU Bake UV filter batch capacity");
  emission_raster::Input input;
  input.resolution=bake.resolution();
  input.physical_bake=true;
  input.bake=bake.storage();
  input.bake_uv_capacity=capacity;
  return emission_raster::upload(input,cancelled);
}

namespace {
void validate_uv_source(EmissionRasterDevice::BakeUvSource source,const std::function<bool()> &cancelled){
  for(size_t at=0;at<source.triangles.size();++at){
    if((at&1023)==0)paint_compute::checkpoint(cancelled);
    for(const auto &uv:source.triangles[at].uv)for(const auto coordinate:uv)
      if(!std::isfinite(coordinate)||coordinate<0||coordinate>1)
        throw std::invalid_argument("GPU Bake UV correspondence has an invalid source corner");
  }
  for(size_t at=0;at<source.padding.size();++at){
    if((at&4095)==0)paint_compute::checkpoint(cancelled);
    if(source.padding[at]>=0&&size_t(source.padding[at])>=source.padding.size())
      throw std::invalid_argument("GPU Bake UV correspondence has an invalid padding owner");
  }
}
}
std::unique_ptr<EmissionRasterDevice> create_bake_uv_maps_device(const BakedTextures &bake,
    uint32_t resolution,std::span<const EmissionRasterDevice::BakeUvFrame> frames,
    size_t capacity,const std::function<bool()> &cancelled,EmissionRasterDevice::BakeUvSource source){
  paint_compute::checkpoint(cancelled);
  bake.validate(resolution);
  if(!capacity||capacity>EmissionRasterDevice::mapped_batch||capacity>bake.coverage.size())
    throw std::invalid_argument("Invalid GPU Bake UV filter batch capacity");
  if(frames.empty()!=bake.maps.normal.rgba.empty())
    throw std::invalid_argument("GPU Bake UV normal planes require ordered tangent frames");
  if(source.triangles.empty()!=source.padding.empty()||
     (!source.triangles.empty()&&(source.padding.size()!=bake.coverage.size()||
         (!frames.empty()&&source.triangles.size()!=frames.size()))))
    throw std::invalid_argument("GPU Bake UV correspondence requires its matching source atlas");
  validate_uv_source(source,cancelled);
  emission_raster::Input input;
  input.resolution=resolution;
  input.bake_uv_capacity=capacity;
  input.bake_uv_maps=&bake;
  input.bake_uv_frames=frames;
  input.bake_uv_source=source;
  if(bake.emission_field){
    input.physical_bake=true;
    input.bake=bake.emission_field->storage();
  }
  return emission_raster::upload(input,cancelled);
}

size_t resident_uv_layer_bytes(const PaintLayer &layer,uint32_t resolution,size_t triangles,size_t capacity){
  using namespace emission_raster;
  size_t common=0,pbr=0,words=0,physical=0;
  for(const auto &tile:layer.tiles)if(tile&&!PaintTile::is_shared_default(tile.get()))++common;
  for(const auto &tile:layer.pbr_tiles)if(tile)++pbr;
  for(const auto &tile:layer.emission_tiles)if(tile){
    ++physical;
    words+=EmissionExportAccess::emission_tile(*tile).samples.size()*sizeof(float);
  }
  return sizeof(paint_grid::Tables)+triangles*2*sizeof(EmissionRasterDevice::BakeUvTriangle)+
      size_t(resolution)*resolution*sizeof(int32_t)+
      capacity*2*(sizeof(EmissionRasterDevice::BakeUvPoint)+sizeof(paint_grid::Pixel))+
      layer.tiles.size()*(2*sizeof(UvLayerEntry)+sizeof(PaintTile *)+
          (layer.pbr_tiles.empty()?0:sizeof(PaintColorTile *))+
          (layer.emission_tiles.empty()?0:sizeof(PhysicalTile)))+
      common*sizeof(PaintTile)+pbr*sizeof(PaintColorTile)+physical*2*sizeof(PackedField)+words+
      std::min(common,size_t(16))*sizeof(PaintTile)+std::min(pbr,size_t(16))*sizeof(PaintColorTile)+
      std::min(words,size_t(65536)*sizeof(float));
}

std::unique_ptr<EmissionRasterDevice> create_resident_uv_layer(const PaintLayer &layer,uint32_t resolution,
    EmissionRasterDevice::BakeUvSource source,size_t capacity,const paint_grid::Tables &tables,
    const std::function<bool()> &cancelled){
  using namespace emission_raster;
  paint_compute::checkpoint(cancelled);
  const auto n=size_t(resolution/32)*(resolution/32);
  if(resolution<64||resolution>2048||resolution%32||!capacity||
     capacity>EmissionRasterDevice::mapped_batch||capacity>size_t(resolution)*resolution||
     source.triangles.empty()||source.padding.size()!=size_t(resolution)*resolution||
     layer.tiles.size()!=n||(!layer.pbr_tiles.empty()&&layer.pbr_tiles.size()!=n)||
     (!layer.emission_tiles.empty()&&layer.emission_tiles.size()!=n))
    throw std::invalid_argument("Invalid resident GPU UV layer source");
  validate_uv_source(source,cancelled);
  UvLayerInput uv;
  uv.entries.resize(n);
  uv.prepared=!layer.pbr_tiles.empty();
  uv.tables=&tables;
  Input input;
  input.bake_uv_capacity=capacity;
  input.bake_uv_source=source;
  input.resolution=resolution;
  input.uv_layer=&uv;
  input.common.reserve(n);
  if(uv.prepared)uv.pbr.reserve(n);
  if(!layer.emission_tiles.empty())input.physical.reserve(n);
  size_t offset=0;
  for(size_t at=0;at<n;++at){
    if((at&255)==0)paint_compute::checkpoint(cancelled);
    auto &entry=uv.entries[at];
    if(const auto *tile=layer.tiles[at].get();tile&&!PaintTile::is_shared_default(tile)){
      entry.common=uint32_t(input.common.size());
      input.common.push_back(tile);
    }
    if(uv.prepared&&layer.pbr_tiles[at]){
      entry.pbr=uint32_t(uv.pbr.size());
      uv.pbr.push_back(layer.pbr_tiles[at].get());
    }
    if(!layer.emission_tiles.empty()&&layer.emission_tiles[at]){
      entry.physical=uint32_t(input.physical.size());
      auto &field=input.physical.emplace_back(EmissionExportAccess::emission_tile(*layer.emission_tiles[at]));
      if(offset>UINT32_MAX||field.samples.size()>UINT32_MAX-offset)
        throw std::length_error("Resident UV paint coefficients exceed addressable storage");
      field.field.offset=uint32_t(offset);
      offset+=field.samples.size();
    }
  }
  return upload(input,cancelled);
}

std::unique_ptr<EmissionRasterDevice> create_finite_emission_raster_device(
    const PaintedMesh &paint,const PaintSourceTexture *source,std::array<float,3> background,
    uint32_t resolution,float offset,const std::function<bool()> &cancelled){
  if(!resolution||resolution>8192||!std::isfinite(offset)||(source&&offset!=0))
    throw std::invalid_argument("Invalid GPU finite emission sampling grid");
  auto input=describe_raster(paint,EmissionExportAccess::writable_padding(paint),background,1,true,cancelled);
  describe_finite_grid(input,resolution,offset);
  describe_source(input,source,nullptr,resolution);
  return emission_raster::upload(input,cancelled);
}

std::unique_ptr<EmissionRasterDevice> create_mapped_emission_raster_device(
    const PaintedMesh &paint,const PaintSourceTexture *source,std::array<float,3> background,
    uint32_t resolution,size_t capacity,const std::function<bool()> &cancelled){
  if(!resolution||resolution>8192||!capacity||capacity>EmissionRasterDevice::mapped_batch)
    throw std::invalid_argument("Invalid mapped GPU emission grid or batch capacity");
  auto input=describe_raster(paint,EmissionExportAccess::writable_padding(paint),background,1,true,cancelled);
  describe_finite_grid(input,resolution,0);
  input.mapped_capacity=capacity;
  if(source){
    if(!source->image)throw std::invalid_argument("Mapped GPU emission needs an actual image");
    source->image->validate();
    input.source=source->image->rgba;
    input.mapped_source=source;
  }
  return emission_raster::upload(input,cancelled);
}

std::unique_ptr<EmissionRasterDevice> create_mapped_emission_field_device(
    const PaintedMesh &paint,const PaintSourceTexture &source,std::array<float,3> background,
    float legacy_strength,size_t capacity,const std::function<bool()> &cancelled){
  if(!paint.source_maps()||!source.image||!capacity||capacity>EmissionRasterDevice::mapped_batch)
    throw std::invalid_argument("Invalid mapped HDR emission field");
  source.validate(paint.source_maps()->coordinate_count());
  auto input=describe_raster(paint,EmissionExportAccess::writable_padding(paint),background,legacy_strength,false,cancelled);
  input.mapped_capacity=capacity;
  input.mapped_field=true;
  input.source=source.image->rgba;
  input.mapped_source=&source;
  return emission_raster::upload(input,cancelled);
}

std::unique_ptr<EmissionRasterDevice> create_bake_radiance_device(const PaintOverlayPacket *packet,
    const PaintSourceTexture *source,std::array<float,3> background,float legacy_strength,size_t capacity,
    const std::function<bool()> &cancelled,const std::function<void(size_t)> &reserve,bool summarize){
  if(!capacity||capacity>EmissionRasterDevice::mapped_batch)
    throw std::invalid_argument("Invalid GPU Bake radiance capacity");
  if(packet&&packet->empty())packet=nullptr;
  const auto layers=packet?packet->layers():std::span<const PaintOverlayLayer>{};
  const auto padding=packet?std::span<const int32_t>(*packet->padding()):std::span<const int32_t>{};
  auto input=describe_raster(layers,packet?packet->baked().get():nullptr,packet?packet->resolution():0,
      padding,background,legacy_strength,true,cancelled,nullptr,true,false);
  input.bake_radiance_capacity=capacity;
  input.summarize_bake_radiance=summarize;
  if(source){
    source->validate(8);
    if(!source->image)throw std::invalid_argument("GPU Bake radiance needs its actual source image");
    input.mapped_source=source;
    input.source=source->image->rgba;
  }
  return emission_raster::upload(input,cancelled,reserve);
}

std::unique_ptr<EmissionRasterDevice> create_texture_set_emission_device(
    const PaintedMesh &paint,const PaintSourceTexture *source,const Image *aligned_mtoon,
    std::array<float,3> background,float strength,const std::function<bool()> &cancelled){
  auto input=describe_raster(paint,EmissionExportAccess::writable_padding(paint),background,strength,true,cancelled);
  input.texture_set=true;
  if(aligned_mtoon){
    aligned_mtoon->validate();
    if(source||aligned_mtoon->width!=input.resolution||aligned_mtoon->height!=input.resolution)
      throw std::invalid_argument("Invalid aligned MToon emission grid");
    static const auto linear=[] {
      std::array<float,256> table{};
      for(size_t i=0;i<table.size();++i)table[i]=mtoon_srgb_to_linear(float(i)/255.f);
      return table;
    }();
    input.source=aligned_mtoon->rgba;
    input.aligned_mtoon_linear=linear;
  }else describe_source(input,source,nullptr,input.resolution);
  return emission_raster::upload(input,cancelled);
}
}
