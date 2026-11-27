// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "paint-overlay-packet.hpp"
#include "dome/source-maps.hpp"
#include "baked-emission-access.hpp"
#include <algorithm>
#include <cmath>
namespace dome::detail {
using PaintOverlayValues=std::array<std::array<float,4>,5>;
struct PaintOverlayTexel {
 PaintOverlayValues values{};
 // Charge the directory probes and the channel groups actually composed.
 size_t work=0;
};

inline float overlay_linear_byte(uint8_t value){
 static const auto table=[] {
  std::array<float,256> result{};
  for(size_t i=0;i<result.size();++i){
   const float x=float(i)/255.f;
   result[i]=x<=.04045f?x/12.92f:std::pow((x+.055f)/1.055f,2.4f);
  }
  return result;
 }();
 return table[value];
}

// Asset-worker point evaluation of the five independent linear contribution
// fields. Filtering follows composition, matching the live GPU planes. There
// is no dense float image, mutable paint access or GPU readback.
inline PaintOverlayTexel sample_overlay_texel_owner(const PaintOverlayPacket &packet,
                                                   size_t at,bool pbr){
 const size_t r=packet.resolution(),stride=PaintTile::size;
 const size_t x=at%r,y=at/r,tile=(y/stride)*(r/stride)+x/stride;
 const size_t within=(y%stride)*stride+x%stride;
 PaintOverlayTexel sample;
 auto &value=sample.values;
 if(const auto &baked=packet.baked()){
  sample.work+=32;
  const float a=baked->coverage.at(at)/255.f;
  const auto &color=pbr&&baked->color_interpretation==BakedColorInterpretation::ResolvedPbr
      ? baked->maps.pbr_color : baked->maps.color;
  for(size_t c=0;c<4;++c){
   const auto channel=color.rgba.at(at*4+c);
   value[0][c]=(c<3?overlay_linear_byte(channel):channel/255.f)*a;
   value[3][c]=a;
  }
  const auto emission=baked_emission_texel(*baked,at);
  for(size_t c=0;c<3;++c){
   value[1][c]=baked->maps.metal_rough.rgba.at(at*4+c)/255.f*a;
   value[2][c]=emission.legacy[c];
   value[4][c]=emission.absolute[c];
  }
  value[3][3]=emission.coverage;
  value[1][3]=a;
 }
 const auto blend=[](float &destination,float incoming,float coverage){
  destination=destination*(1-coverage)+incoming*coverage;
 };
 for(const auto &layer:packet.layers()){
  ++sample.work;
  const auto &common=layer.tiles.at(tile);
  if(!common)continue;
  ++sample.work;
  const auto &q=common->pixels[within].data;
  const float mask=q[13]/255.f*layer.opacity;
  if(mask==0)continue;
  sample.work+=3;
  const float color_coverage=q[9]/255.f*mask;
  if(color_coverage!=0){
   sample.work+=8;
   const auto *color=q.data();
   if(pbr&&!layer.pbr_tiles.empty())
    if(const auto &secondary=layer.pbr_tiles.at(tile))
     color=secondary->pixels[within].data();
   for(size_t c=0;c<4;++c)
    blend(value[0][c],c<3?overlay_linear_byte(color[c]):color[c]/255.f,color_coverage);
   value[3][0]=value[3][0]*(1-color_coverage)+color_coverage;
  }
  for(size_t c=1;c<=2;++c){
   const float coverage=q[9+c]/255.f*mask;
   if(coverage==0)continue;
   sample.work+=2;
   blend(value[1][3-c],q[3+c]/255.f,coverage);
   value[3][c]=value[3][c]*(1-coverage)+coverage;
  }
  const auto *emission=layer.emission_tiles.empty()?nullptr:layer.emission_tiles.at(tile).get();
  const auto *light=emission?&emission->pixels[within]:nullptr;
  const float coverage=light?mask*light->coverage:q[12]/255.f*mask;
  if(coverage!=0){
   sample.work+=14;
   for(size_t c=0;c<3;++c){
    blend(value[2][c],light?light->legacy[c]:overlay_linear_byte(q[6+c]),coverage);
    blend(value[4][c],light?light->physical[c]:0,coverage);
   }
   value[3][3]=value[3][3]*(1-coverage)+coverage;
  }
 }
 value[2][3]=value[4][3]=value[3][3];
 return sample;
}
inline PaintOverlayValues sample_overlay_texel_values(const PaintOverlayPacket &packet,
                                                      int x,int y,bool pbr){
 const auto r=packet.resolution();
 x=std::clamp(x,0,int(r)-1);
 y=std::clamp(y,0,int(r)-1);
 const auto owner=packet.padding()->at(size_t(y)*r+size_t(x));
 return owner<0?PaintOverlayValues{}:sample_overlay_texel_owner(packet,size_t(owner),pbr).values;
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
