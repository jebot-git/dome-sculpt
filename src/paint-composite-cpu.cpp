// SPDX-License-Identifier: GPL-3.0-or-later
#include "paint-composite-backend.hpp"
#include "emission-raster-device.hpp"
#include "paint-source-coordinate-kernel.hpp"
#include "paint-grid-filter.hpp"
#include "portable-normal-raster.hpp"
#include "source-uv-sampler-kernel.hpp"
#include "dome/source-maps.hpp"
#include <cstring>
namespace dome::detail {
namespace {
class ReferenceSourceUvSampler final : public SourceUvSampler {
  PaintSourceTexture source_;
  bool srgb_;
  Output format_;
public:
  ReferenceSourceUvSampler(const PaintSourceTexture &source, bool srgb, Output format, size_t capacity,
                           Transform transform)
      : SourceUvSampler(capacity, format,source.image->rgba.size()/4), source_(source), srgb_(srgb), format_(format) {
    source_.texcoord = 0;
    if(transform==Transform::ResolvedCoordinates)source_.uv_transform = {1, 0, 0, 1, 0, 0};
  }
private:
  void dispatch(std::span<const Coordinate> pending, std::span<uint32_t> result) override {
    if (format_ == Output::LinearAssociatedRgba) {
      const auto image=mapped_source_image(source_,source_.image->rgba.data(),srgb_,format_,Transform::SourceAffine);
      for(size_t i=0;i<pending.size();++i)
        sample_mapped_source(image,pending[i],paint_compute::quantization(),result.data(),i);
      return;
    }
    for (size_t i = 0; i < pending.size(); ++i) {
      const auto color = sample_source_texture(source_, pending[i].u, pending[i].v, srgb_);
      if (format_ == Output::EncodedRgba) {
        uint32_t rgba = 0;
        for (unsigned c = 0; c < 4; ++c) {
          const float value = srgb_ && c < 3 ? linear_to_srgb(color[c]) : color[c];
          const auto byte = uint32_t(std::lround(std::clamp(value, 0.f, 1.f) * 255));
          rgba |= byte << (8 * c);
        }
        result[i] = rgba;
      } else if (format_ == Output::LinearRgba) {
        for (unsigned c = 0; c < 4; ++c) result[4 * i + c] = std::bit_cast<uint32_t>(color[c]);
      } else {
        result[i] = std::bit_cast<uint32_t>(color[unsigned(format_)]);
      }
    }
  }
  void rewrite_texels(size_t first,const TexelAffine &affine,std::span<uint32_t> result) override {
    const auto &pixels=source_.image->rgba;
    for(size_t i=0;i<result.size();++i){
      uint32_t packed=0;
      for(unsigned c=0;c<4;++c){
        auto value=pixels[4*(first+i)+c];
        if(affine.channels&(1u<<c)){
          const bool nonlinear=srgb_&&c<3;
          const float sample=nonlinear?srgb_to_linear(value/255.f):value/255.f;
          const float mapped=sample*affine.scale[c]+affine.offset[c];
          value=uint8_t(std::lround(std::clamp(nonlinear?linear_to_srgb(mapped):mapped,0.f,1.f)*255));
        }
        packed|=uint32_t(value)<<(8*c);
      }
      result[i]=packed;
    }
  }
};
}
std::unique_ptr<SourceUvSampler> upload_source_uv_sampler(
    const PaintSourceTexture &source, bool srgb, SourceUvSampler::Output format, size_t capacity,
    SourceUvSampler::Transform transform) {
  return std::make_unique<ReferenceSourceUvSampler>(source, srgb, format, capacity, transform);
}
}
namespace dome::detail::portable_normal {
std::unique_ptr<Raster> upload(const Image &,const std::function<bool()> &){
 throw std::logic_error("Select a GPU backend for portable normal reprojection");
}
}
namespace dome::detail::emission_raster {
std::unique_ptr<EmissionRasterDevice> upload(const Input &,const std::function<bool()> &,
    const std::function<void(size_t)> &){
 throw std::logic_error("Select a GPU backend for device emission compositing");
}
std::unique_ptr<SourceMaterialRaster> upload_material(const Input &,const MaterialInput &,
    const std::function<bool()> &){
 throw std::logic_error("Select a GPU backend for mapped material compositing");
}
}
namespace dome::paint_compute {
std::shared_ptr<const MaterialAtlas> MaterialAtlas::compile(const Input &,const std::function<bool()> &){
 throw std::logic_error("Select a GPU backend for resident Bake material atlases");
}
Result composite(const Input &in,const std::function<bool()> &cancelled){
 validate(in,cancelled);const auto &q=quantization();
 Result out(in);
 std::vector<Pixel> borrowed;
 if(!in.native_tiles.empty()){
  borrowed.resize(in.packed_texels());
  visit_native_tile_uploads(in,[&](size_t first,std::span<const std::byte> bytes){
   std::memcpy(borrowed.data()+first,bytes.data(),bytes.size());
  },cancelled);
 }
 const auto *pixels=borrowed.empty()?in.pixels.data():borrowed.data();
 const auto padding=in.padding_view();
 const auto bake=in.bake.view();
 const auto prepared=in.prepared_view(in.projected_color.data(),in.baked_pbr_color.data(),
     in.color_tiles.empty()?nullptr:in.color_tiles.data(),in.color_pixels.data(),in.projected_columns.data(),in.projected_rows.data());
 for(const auto grid:in.output_grids()){
  if(!grid.channels)break;
  auto source=in.source_views();
  SourceCoordinateBatch batch;
  SourcePixelTaps mapped[SourceRoleCount];
  for(size_t role=0;role<in.sources.size();++role){
   const auto &texture=in.sources[role];
   if(!texture.coupled()||!grid.writes(source_channels[role]))continue;
   source.textures[role].mapped=&mapped[batch.size];
   batch.jobs[batch.size]=texture.coordinate_job(texture.column_terms.data(),texture.row_terms.data(),&mapped[batch.size]);
   ++batch.size;
  }
  const auto view=out.view(grid,0);
  for(uint32_t at=0;at<grid.extent.size();++at){
   if((at&4095)==0)checkpoint(cancelled);
   source.first=at;
   for(unsigned j=0;j<batch.size;++j)
    mapped[j]=source_coordinates(batch.jobs[j],at%grid.extent.width,at/grid.extent.width);
   write_output(view,grid,at,at,in.parameters,in.layers.data(),in.tiles.data(),pixels,padding.data(),bake,prepared,source,q);
  }
 }
 checkpoint(cancelled);return out;
}
}

namespace dome::paint_grid {
struct Filter::Impl { Tables tables; size_t mapped=0; };
Filter::Filter(const Tables &tables,size_t count):impl_(std::make_unique<Impl>(Impl{tables,mapped_capacity(count)})){}
Filter::~Filter()=default;
void Filter::run(const Batch &batch,const Layout &layout,std::span<const Pixel> input,
                 std::span<Pixel> output,const std::function<bool()> &cancelled) {
  validate(batch,layout,input.size(),output.size());
  if(impl_->mapped)throw std::logic_error("Expected a paint-grid rectangle filter");
  for(size_t i=0;i<output.size();++i) {
    if((i&255)==0)checkpoint(cancelled);
    output[i]=filter(i,batch,layout,impl_->tables,input.data());
  }
  checkpoint(cancelled);
}
void Filter::run_mapped(const Layout &layout,std::span<const Pixel> input,std::span<const MappedTaps> taps,
                        std::span<Pixel> output,const std::function<bool()> &cancelled){
  validate_mapped(layout,taps,input.size(),output.size(),impl_->mapped,cancelled);
  for(size_t i=0;i<output.size();++i){
    if((i&255)==0)checkpoint(cancelled);
    output[i]=filter_mapped(taps[i],layout,impl_->tables,input.data());
  }
  checkpoint(cancelled);
}
}
