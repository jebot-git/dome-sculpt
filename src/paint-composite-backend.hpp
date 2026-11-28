// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "paint-composite-kernel.hpp"
#include <array>
#include <functional>
#include <span>
#include <vector>
namespace dome::paint_compute {
struct BakeImages {
  std::span<const uint8_t> coverage,color,orm,emission,normal;
  size_t size_bytes() const noexcept {
    return coverage.size()+color.size()+orm.size()+emission.size()+normal.size();
  }
  Bake view() const noexcept {
    return coverage.empty()?Bake{}:Bake{coverage.data(),color.data(),orm.data(),emission.data(),
                                      normal.empty()?nullptr:normal.data()};
  }
  Bake device_view(const uint8_t *buffer) const noexcept {
    if(coverage.empty())return {};
    const auto *colors=buffer+coverage.size();
    const auto *orms=colors+color.size();
    const auto *emissions=orms+orm.size();
    return {buffer,colors,orms,emissions,normal.empty()?nullptr:emissions+emission.size()};
  }
};
struct TextureInput {
  std::span<const uint8_t> rgba;
  std::span<const detail::SourceTextureAxis> columns,rows;
  uint32_t width=0,height=0;
  bool nearest=false;
  std::span<const SourceTerms> column_terms,row_terms;
  SourceTransform transform;
  bool coupled() const noexcept { return !column_terms.empty(); }
  SourceCoordinateJob coordinate_job(const SourceTerms *a,const SourceTerms *b,SourcePixelTaps *output) const noexcept {
    return {a,b,output,transform,width,height,nearest};
  }
};
struct Input {
  static constexpr size_t tile_texels=1024,tile_bytes=tile_texels*sizeof(Pixel),native_upload_tiles=16;
  Parameters parameters;
  std::vector<Layer> layers;
  std::vector<uint32_t> tiles;
  std::vector<Pixel> pixels;
  std::vector<int32_t> padding;
  std::span<const int32_t> native_padding;
  std::vector<std::span<const std::byte>> native_tiles;
  BakeImages bake;
  std::span<const uint8_t> projected_color,baked_pbr_color;
  std::span<const detail::SourceTextureAxis> projected_columns,projected_rows;
  // One optional PBR tile index per packed common tile.
  std::vector<uint32_t> color_tiles;
  std::vector<ColorPixel> color_pixels;
  std::array<TextureInput,SourceRoleCount> sources;
  float occlusion_strength=1;
  unsigned native_channels=0;
  bool raw_bake_normal=false;
  std::span<const int32_t> padding_view() const noexcept {return native_padding.empty()?std::span(padding):native_padding;}
  size_t packed_texels() const noexcept {return native_tiles.empty()?pixels.size():native_tiles.size()*tile_texels;}
  Extent native_extent() const noexcept {return {parameters.resolution,parameters.resolution};}
  Extent extent(unsigned channels) const noexcept {
    auto result=native_extent();
    for(unsigned i=0;i<sources.size();++i)if(channels&source_channels[i]){
      if(sources[i].width>result.width)result.width=sources[i].width;
      if(sources[i].height>result.height)result.height=sources[i].height;
    }
    return result;
  }
  bool expanded() const noexcept {return extent(material_channels)!=native_extent();}
  bool split_output() const noexcept {return native_channels||expanded();}
  std::array<OutputGrid,4> output_grids() const noexcept;
  size_t bake_and_projection_bytes() const noexcept {
    return bake.size_bytes()+projected_color.size()+baked_pbr_color.size();
  }
  size_t image_bytes() const noexcept {
    auto bytes=bake_and_projection_bytes();
    for(const auto &source:sources)bytes+=source.rgba.size();
    return bytes;
  }
  size_t axis_count() const noexcept {
    size_t count=projected_columns.size()+projected_rows.size();
    for(const auto &source:sources)count+=source.columns.size()+source.rows.size();
    return count;
  }
  size_t term_count() const noexcept {
    size_t count=0;
    for(const auto &source:sources)count+=source.column_terms.size()+source.row_terms.size();
    return count;
  }
  size_t coupled_source_count() const noexcept {
    size_t count=0;
    for(const auto &source:sources)count+=source.coupled();
    return count;
  }
  template<class Copy>
  void visit_images(Copy copy) const {
    for(const auto plane:{bake.coverage,bake.color,bake.orm,bake.emission,bake.normal,projected_color,baked_pbr_color})
      if(!plane.empty())copy(plane);
    for(const auto &source:sources)if(!source.rgba.empty())copy(source.rgba);
  }
  template<class Copy>
  void visit_axes(Copy copy) const {
    if(!projected_columns.empty()){
      copy(projected_columns);
      copy(projected_rows);
    }
    for(const auto &source:sources)if(!source.columns.empty()){
      copy(source.columns);
      copy(source.rows);
    }
  }
  template<class Copy>
  void visit_terms(Copy copy) const {
    for(const auto &source:sources)if(source.coupled()){
      copy(source.column_terms);
      copy(source.row_terms);
    }
  }
  SourceMaterial source_views() const noexcept {
    SourceMaterial result;
    result.occlusion_strength=occlusion_strength;
    for(size_t role=0;role<sources.size();++role){
      const auto &source=sources[role];
      if(!source.rgba.empty())result.textures[role]={source.rgba.data(),source.columns.data(),source.rows.data(),source.width,source.nearest};
    }
    return result;
  }
  SourceMaterial device_sources(const uint8_t *images,const detail::SourceTextureAxis *axes) const noexcept {
    SourceMaterial result;
    result.occlusion_strength=occlusion_strength;
    if(!projected_columns.empty())axes+=projected_columns.size()+projected_rows.size();
    size_t offset=bake_and_projection_bytes();
    for(size_t role=0;role<sources.size();++role){
      const auto &source=sources[role];
      if(source.rgba.empty())continue;
      auto &texture=result.textures[role];
      texture={images+offset,nullptr,nullptr,source.width,source.nearest};
      if(!source.columns.empty()){
        texture.columns=axes;
        texture.rows=axes+source.columns.size();
        axes+=source.columns.size()+source.rows.size();
      }
      offset+=source.rgba.size();
    }
    return result;
  }
  SourceCoordinateBatch device_coordinate_jobs(const SourceTerms *terms,SourcePixelTaps *output,size_t stride,
                                               SourceMaterial &material,unsigned channels) const noexcept {
    SourceCoordinateBatch batch;
    for(size_t role=0;role<sources.size();++role){
      const auto &source=sources[role];
      if(!source.coupled())continue;
      if(channels&source_channels[role]){
        auto *job_output=output+batch.size*stride;
        batch.jobs[batch.size++]=source.coordinate_job(terms,terms+source.column_terms.size(),job_output);
        material.textures[role].mapped=job_output;
      }
      terms+=source.column_terms.size()+source.row_terms.size();
    }
    return batch;
  }
  PreparedColor device_colors(const uint8_t *images,const uint32_t *ids,const ColorPixel *colors,
                              const detail::SourceTextureAxis *axes) const noexcept {
    if(projected_color.empty())return {};
    const auto *vertex=images+bake.size_bytes();
    return prepared_view(vertex,baked_pbr_color.empty()?nullptr:vertex+projected_color.size(),ids,colors,
                         axes,projected_columns.empty()?nullptr:axes+projected_columns.size());
  }
  PreparedColor prepared_view(const uint8_t *vertex,const uint8_t *baked,const uint32_t *ids,const ColorPixel *colors,
                              const detail::SourceTextureAxis *columns,const detail::SourceTextureAxis *rows) const noexcept {
    PreparedColor view{vertex,baked,ids,colors,{}};
    if(!projected_columns.empty())view.sampled_vertex={vertex,columns,rows,parameters.resolution,false};
    return view;
  }
};
struct Result {
  std::vector<Output> pixels;
  std::array<std::vector<uint8_t>,3> planes;
  std::vector<ColorInfo> color_info;
  std::vector<uint8_t> emission_coverage;
  std::vector<uint8_t> normals,pbr_colors,bake_coverage;
  explicit Result(const Input &);
  OutputView view(OutputGrid,size_t first);
};
const Quantization &quantization();
void validate(const Input &,const std::function<bool()> &cancelled);
void checkpoint(const std::function<bool()> &cancelled);
// Each callback consumes its bytes before the staging buffer is reused.
void visit_native_tile_uploads(const Input &,
    const std::function<void(size_t,std::span<const std::byte>)> &,const std::function<bool()> &);
// Selected backend only: all image results remain detached until every chunk
// and the final cancellation check succeed. No renderer/model memory aliases.
Result composite(const Input &,const std::function<bool()> &cancelled={});
inline constexpr size_t chunk_pixels=65536,maximum_pixels=4*1024*1024,maximum_tile_bytes=128*1024*1024;
inline constexpr size_t maximum_output_pixels=16*1024*1024;
inline constexpr uint32_t maximum_extent=8192;
inline constexpr size_t output_scratch_bytes=chunk_pixels*(sizeof(Output)+8);
OutputView scratch_view(uint8_t *,OutputGrid,bool combined);
template<class Copy>
void read_output(OutputView target,OutputView scratch,size_t count,Copy copy){
  if(target.combined)copy(target.combined,scratch.combined,count*sizeof(Output));
  for(unsigned plane=0;plane<3;++plane)if(target.images[plane])
    copy(target.images[plane],scratch.images[plane],count*4);
  if(target.color_info)copy(target.color_info,scratch.color_info,count*sizeof(ColorInfo));
  if(target.emission_coverage)copy(target.emission_coverage,scratch.emission_coverage,count);
  if(target.normal)copy(target.normal,scratch.normal,count*4);
  if(target.pbr)copy(target.pbr,scratch.pbr,count*4);
  if(target.bake_coverage)copy(target.bake_coverage,scratch.bake_coverage,count);
}
}
