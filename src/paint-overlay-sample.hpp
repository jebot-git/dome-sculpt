// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "paint-overlay-packet.hpp"
#include "dome/source-maps.hpp"
#include "baked-emission-access.hpp"
#include <algorithm>
#include <cmath>
namespace dome::detail {
using PaintOverlayValues=std::array<std::array<float,4>,5>;
// Asset-worker point evaluation of the five independent linear contribution
// fields. Filtering follows composition, matching the live GPU planes. There
// is no dense float image, mutable paint access or GPU readback.
inline PaintOverlayValues sample_overlay_texel_values(const PaintOverlayPacket &packet,int x,int y,bool pbr){
 const auto r=packet.resolution();
 auto linear=[](float x){return x<=.04045f?x/12.92f:std::pow((x+.055f)/1.055f,2.4f);};

  PaintOverlayValues value{};x=std::clamp(x,0,int(r)-1);y=std::clamp(y,0,int(r)-1);
  const auto owner=packet.padding()->at(size_t(y)*r+size_t(x));if(owner<0)return value;
  const size_t at=size_t(owner),px=at%r,py=at/r,tile=(py/32)*(r/32)+px/32,within=(py%32)*32+px%32;
  if(const auto &baked=packet.baked()){
   const float a=baked->coverage.at(at)/255.f;const auto &colour=pbr&&baked->color_interpretation==BakedColorInterpretation::ResolvedPbr?baked->maps.pbr_color:baked->maps.color;
   for(size_t c=0;c<4;++c){const float sample=colour.rgba.at(at*4+c)/255.f;value[0][c]=(c<3?linear(sample):sample)*a;value[3][c]=a;}
   const auto emission=baked_emission_texel(*baked,at);
   for(size_t c=0;c<3;++c){value[1][c]=baked->maps.metal_rough.rgba.at(at*4+c)/255.f*a;value[2][c]=emission.legacy[c];value[4][c]=emission.absolute[c];}
   value[3][3]=emission.coverage;
   value[1][3]=a;
  }
  for(const auto &layer:packet.layers())if(const auto &common=layer.tiles.at(tile)){
   const auto &q=common->pixels[within].data;const float mask=q[13]/255.f*layer.opacity;
   std::array<float,4> coverage;for(size_t c=0;c<4;++c)coverage[c]=q[9+c]/255.f*mask;
   for(size_t c=0;c<4;++c){float sample=q[c]/255.f;if(pbr&&!layer.pbr_tiles.empty())if(const auto &colour=layer.pbr_tiles.at(tile))sample=colour->pixels[within][c]/255.f;value[0][c]=value[0][c]*(1-coverage[0])+(c<3?linear(sample):sample)*coverage[0];}
   value[1][1]=value[1][1]*(1-coverage[2])+q[5]/255.f*coverage[2];value[1][2]=value[1][2]*(1-coverage[1])+q[4]/255.f*coverage[1];
   const auto emission=!layer.emission_tiles.empty()?layer.emission_tiles.at(tile):nullptr;
   if(emission)coverage[3]=mask*emission->pixels[within].coverage;
   for(size_t c=0;c<3;++c){const float legacy=emission?emission->pixels[within].legacy[c]:linear(q[6+c]/255.f),physical=emission?emission->pixels[within].physical[c]:0;value[2][c]=value[2][c]*(1-coverage[3])+legacy*coverage[3];value[4][c]=value[4][c]*(1-coverage[3])+physical*coverage[3];}
   for(size_t c=0;c<4;++c)value[3][c]=value[3][c]*(1-coverage[c])+coverage[c];
  }
  value[2][3]=value[4][3]=value[3][3];return value;
}
inline PaintOverlayValues sample_overlay_values(const PaintOverlayPacket &packet,float u,float v,bool pbr){
 if(!std::isfinite(u)||!std::isfinite(v))throw std::invalid_argument("Nonfinite paint overlay sample");
 const auto r=packet.resolution();
 auto pixel=[&](int x,int y){return sample_overlay_texel_values(packet,x,y,pbr);};
 const float x=std::clamp(u,0.f,1.f)*r-.5f,y=std::clamp(v,0.f,1.f)*r-.5f;const int ix=int(std::floor(x)),iy=int(std::floor(y));const float fx=x-ix,fy=y-iy;
 const auto a=pixel(ix,iy),b=pixel(ix+1,iy),c=pixel(ix,iy+1),d=pixel(ix+1,iy+1);PaintOverlayValues out;
 for(size_t plane=0;plane<5;++plane)for(size_t k=0;k<4;++k)out[plane][k]=std::lerp(std::lerp(a[plane][k],b[plane][k],fx),std::lerp(c[plane][k],d[plane][k],fx),fy);
 return out;
}
} // namespace dome::detail
