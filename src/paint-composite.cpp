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
 for(const auto &source:in.sources){
  if(source.rgba.empty()){
   if(source.width||source.height||!source.columns.empty()||!source.rows.empty())
    throw std::invalid_argument("Source texture coordinates have no image owner");
   continue;
  }
  if(!source.width||!source.height||source.width>p.resolution||source.height>p.resolution||
     source.rgba.size()!=4*size_t(source.width)*source.height||
     source.columns.size()!=p.resolution||source.rows.size()!=p.resolution)
   throw std::invalid_argument("Source texture inputs do not fit the compositor grid");
  const auto check_axis=[&](std::span<const detail::SourceTextureAxis> taps,uint32_t extent){
   for(const auto &tap:taps){
    if(uint32_t(tap.first)>=extent||uint32_t(tap.second)>=extent||!std::isfinite(tap.fraction)||tap.fraction<0||tap.fraction>1)
     throw std::out_of_range("Source texture tap is outside the declared image");
   }
  };
  checkpoint(cancelled);
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
