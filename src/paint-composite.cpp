// SPDX-License-Identifier: GPL-3.0-or-later
#include "paint-composite-backend.hpp"
#include "dome/paint.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace dome::paint_compute {
void checkpoint(const std::function<bool()> &cancelled){if(cancelled&&cancelled())throw std::runtime_error("Paint compositing cancelled");}
const Quantization &quantization(){
 static const auto table=[] {
  Quantization q;for(uint32_t i=0;i<256;++i){q.unit[i]=float(i)/255;q.srgb[i]=srgb_to_linear(q.unit[i]);}
  auto byte=[](float v){return uint8_t(std::lround(std::clamp(linear_to_srgb(v),0.f,1.f)*255));};
  for(uint32_t k=1;k<=255;++k){uint32_t lo=0,hi=std::bit_cast<uint32_t>(1.f);while(lo<hi){auto mid=lo+(hi-lo)/2;if(byte(std::bit_cast<float>(mid))>=k)hi=mid;else lo=mid+1;}q.threshold[k-1]=lo;
   if(byte(std::bit_cast<float>(lo))<k||(lo&&byte(std::bit_cast<float>(lo-1))>=k))throw std::logic_error("Canonical paint quantization boundary is invalid");
  }return q;
 }();return table;
}
std::array<OutputGrid,4> Input::output_grids() const noexcept {
 std::array<OutputGrid,4> grids{};
 auto add=[&](Extent extent,unsigned channel){
  for(auto &grid:grids)if(!grid.channels||grid.extent==extent){
   grid.extent=extent;
   grid.channels|=channel;
   return;
  }
 };
 add(extent(Color),Color|(projected_color.empty()?0u:Pbr));
 add(extent(Orm),Orm);
 add(extent(Emission),Emission);
 if(!bake.normal.empty())add(native_extent(),Normal);
 return grids;
}
Result::Result(const Input &input){
 if(input.expanded()){
  for(unsigned plane=0;plane<planes.size();++plane)planes[plane].resize(input.extent(1u<<plane).size()*4);
  color_info.resize(input.extent(Color).size());
  emission_coverage.resize(input.extent(Emission).size());
 }else pixels.resize(input.native_extent().size());
 if(!input.projected_color.empty())pbr_colors.resize(input.extent(Color).size()*4);
 if(!input.bake.normal.empty())normals.resize(input.native_extent().size()*4);
}
OutputView Result::view(OutputGrid grid,size_t first){
 OutputView view;
 if(!pixels.empty())view.combined=pixels.data()+first;
 else{
  for(unsigned plane=0;plane<planes.size();++plane)if(grid.writes(1u<<plane))
   view.images[plane]=planes[plane].data()+4*first;
  if(grid.writes(Color))view.color_info=color_info.data()+first;
  if(grid.writes(Emission))view.emission_coverage=emission_coverage.data()+first;
 }
 if(grid.writes(Pbr))view.pbr=pbr_colors.data()+4*first;
 if(grid.writes(Normal))view.normal=normals.data()+4*first;
 return view;
}
OutputView scratch_view(uint8_t *buffer,OutputGrid grid,bool combined){
 static_assert(alignof(Output)==1&&alignof(ColorInfo)==1);
 auto reserve=[&](size_t bytes){auto *slice=buffer;buffer+=bytes*chunk_pixels;return slice;};
 OutputView view;
 if(combined)view.combined=reinterpret_cast<Output *>(reserve(sizeof(Output)));
 else{
  for(unsigned plane=0;plane<3;++plane)if(grid.writes(1u<<plane))view.images[plane]=reserve(4);
  if(grid.writes(Color))view.color_info=reinterpret_cast<ColorInfo *>(reserve(sizeof(ColorInfo)));
  if(grid.writes(Emission))view.emission_coverage=reserve(1);
 }
 if(grid.writes(Pbr))view.pbr=reserve(4);
 if(grid.writes(Normal))view.normal=reserve(4);
 return view;
}
void validate(const Input &in,const std::function<bool()> &cancelled){
 checkpoint(cancelled);const auto &p=in.parameters;const uint64_t count=uint64_t(p.resolution)*p.resolution,tiles=uint64_t((p.resolution+31)/32)*((p.resolution+31)/32);
 if(p.resolution<32||p.resolution>2048||p.resolution%32||count>maximum_pixels||p.layers>128||p.layers!=in.layers.size()||p.tiles!=tiles||in.tiles.size()!=tiles*p.layers||in.padding.size()!=count||in.pixels.size()%1024||in.pixels.size()>maximum_tile_bytes/sizeof(Pixel)||count*std::max(1u,p.layers)>128*1024*1024)throw std::length_error("Paint composite packet exceeds bounded dimensions/work");
 const auto &bake=in.bake;
 const size_t baked_pixels=bake.coverage.empty()?0:count;
 if(bake.coverage.size()!=baked_pixels||bake.color.size()!=4*baked_pixels||
    bake.orm.size()!=4*baked_pixels||bake.emission.size()!=4*baked_pixels||
    (!bake.normal.empty()&&bake.normal.size()!=4*baked_pixels))
  throw std::invalid_argument("Baked paint compositor images must match the native grid");
 const bool prepared=!in.projected_color.empty();
 if(in.projected_color.size()!=(prepared?4*count:0)||in.baked_pbr_color.size()!=(prepared?4*baked_pixels:0)||
    (!in.color_tiles.empty()&&(!prepared||in.color_tiles.size()!=in.pixels.size()/1024))||
    in.color_pixels.size()%1024||(!in.color_pixels.empty()&&in.color_tiles.empty()))
  throw std::invalid_argument("Prepared color compositor inputs must match the native grid and common tiles");
 if(in.color_pixels.size()>(maximum_tile_bytes-in.pixels.size()*sizeof(Pixel))/sizeof(ColorPixel))
  throw std::length_error("Prepared color compositor tiles exceed the shared storage budget");
 for(auto id:in.color_tiles)
  if(id!=absent&&id>=in.color_pixels.size()/1024)
   throw std::out_of_range("Prepared color tile owner is invalid");
 if(!std::isfinite(in.occlusion_strength)||in.occlusion_strength<0||in.occlusion_strength>1)
  throw std::invalid_argument("Invalid source texture occlusion strength");
 for(unsigned role=0;role<in.sources.size();++role){
  const auto &source=in.sources[role];
  if(source.rgba.empty()){
   if(source.width||source.height||!source.columns.empty()||!source.rows.empty()||
      !source.column_terms.empty()||!source.row_terms.empty())
    throw std::invalid_argument("Source texture coordinates have no image owner");
   continue;
  }
  if(!source.width||!source.height||source.width>maximum_extent||source.height>maximum_extent||
     source.rgba.size()!=4*size_t(source.width)*source.height)
   throw std::invalid_argument("Source texture image exceeds the compositor limits");
 }
 for(const auto &grid:in.output_grids())if(grid.extent.size()>maximum_output_pixels)
  throw std::length_error("Composited original texture exceeds output pixel budget");
 if(in.expanded()&&!std::has_single_bit(p.resolution))
  throw std::invalid_argument("Expanded material grids require a native power-of-two paint grid");
 const auto check_axis=[](std::span<const detail::SourceTextureAxis> taps,uint32_t extent){
  for(const auto &tap:taps){
   if(uint32_t(tap.first)>=extent||uint32_t(tap.second)>=extent||!std::isfinite(tap.fraction)||tap.fraction<0||tap.fraction>1)
    throw std::out_of_range("Source texture tap is outside the declared image");
  }
 };
 const auto color_grid=in.extent(Color);
 const bool sample_projection=prepared&&color_grid!=in.native_extent();
 if(in.projected_columns.size()!=(sample_projection?color_grid.width:0)||
    in.projected_rows.size()!=(sample_projection?color_grid.height:0))
  throw std::invalid_argument("Prepared vertex color coordinates do not fit the expanded grid");
 check_axis(in.projected_columns,p.resolution);
 check_axis(in.projected_rows,p.resolution);
 for(unsigned role=0;role<in.sources.size();++role){
  const auto &source=in.sources[role];
  if(source.rgba.empty())continue;
  const auto extent=in.extent(source_channels[role]);
  checkpoint(cancelled);
  if(source.coupled()){
   const auto valid_mode=[](int mode){return mode==33071||mode==33648||mode==10497;};
   const auto valid_term=[](uint64_t v){return (v&0x7fffffffffffffffull)<=std::bit_cast<uint64_t>(10000.);};
   if(!source.columns.empty()||!source.rows.empty()||source.column_terms.size()!=extent.width||
      source.row_terms.size()!=extent.height||!valid_mode(source.transform.wrap_s)||!valid_mode(source.transform.wrap_t)||
      !valid_term(source.transform.offset_u)||!valid_term(source.transform.offset_v))
    throw std::invalid_argument("Invalid coupled source texture transform");
   for(const auto terms:{source.column_terms,source.row_terms})
    for(const auto &term:terms)if(!valid_term(term.u)||!valid_term(term.v))
     throw std::invalid_argument("Coupled source texture term exceeds the transform bounds");
   continue;
  }
  if(source.columns.size()!=extent.width||source.rows.size()!=extent.height||!source.row_terms.empty())
   throw std::invalid_argument("Source texture coordinates do not fit the compositor grid");
  check_axis(source.columns,source.width);
  check_axis(source.rows,source.height);
 }
 for(float v:p.color)if(!std::isfinite(v)||v<0||v>1)throw std::invalid_argument("Invalid paint composite color");
 for(float v:p.emission)if(!std::isfinite(v)||v<0||v>100)throw std::invalid_argument("Invalid paint composite emission");
 for(float v:{p.metallic,p.roughness})if(!std::isfinite(v)||v<0||v>1)throw std::invalid_argument("Invalid paint composite material");
 for(size_t i=0;i<in.layers.size();++i)if(!std::isfinite(in.layers[i].opacity)||in.layers[i].opacity<=0||in.layers[i].opacity>1||in.layers[i].first_tile!=i*tiles)throw std::invalid_argument("Invalid paint composite layer");
 for(size_t i=0;i<in.tiles.size();++i){if(in.tiles[i]!=absent&&in.tiles[i]>=in.pixels.size()/1024)throw std::out_of_range("Paint composite tile owner is invalid");if((i&4095)==0)checkpoint(cancelled);}
 for(size_t i=0;i<in.padding.size();++i){if(in.padding[i]<-1||int64_t(in.padding[i])>=int64_t(count))throw std::out_of_range("Paint composite padding owner is invalid");if((i&4095)==0)checkpoint(cancelled);}
 checkpoint(cancelled);
}
}
