// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "paint-overlay-packet.hpp"
#include <algorithm>
#include <cmath>
namespace dome::detail {
struct PaintOverlayAlpha {float premultiplied=0,coverage=0;};
// Four sparse point evaluations match linear sampling of the compiled float
// contribution planes. No dense CPU image is constructed for alpha picking.
inline PaintOverlayAlpha sample_overlay_alpha(const PaintOverlayPacket &packet,float u,float v,bool pbr) {
  if(!std::isfinite(u)||!std::isfinite(v))throw std::invalid_argument("Nonfinite paint overlay sample");
  const auto r=packet.resolution();
  auto pixel=[&](int x,int y){
    x=std::clamp(x,0,int(r)-1);y=std::clamp(y,0,int(r)-1);
    const auto owner=packet.padding()->at(size_t(y)*r+size_t(x));PaintOverlayAlpha out;
    if(owner<0)return out;
    const size_t at=size_t(owner),tx=at%r,ty=at/r,tile=(ty/32)*(r/32)+tx/32,within=(ty%32)*32+tx%32;
    if(const auto &baked=packet.baked()){
      out.coverage=float(baked->coverage.at(at))/255;
      const auto &image=pbr&&baked->color_interpretation==BakedColorInterpretation::ResolvedPbr?baked->maps.pbr_color:baked->maps.color;
      out.premultiplied=float(image.rgba.at(at*4+3))/255*out.coverage;
    }
    for(const auto &layer:packet.layers())if(const auto &common=layer.tiles.at(tile)){
      const auto &data=common->pixels[within].data;
      const float a=float(data[9])/255*(float(data[13])/255)*layer.opacity;
      float alpha=float(data[3])/255;
      if(pbr&&!layer.pbr_tiles.empty())if(const auto &colour=layer.pbr_tiles.at(tile))alpha=float(colour->pixels[within][3])/255;
      out.premultiplied=out.premultiplied*(1-a)+alpha*a;
      out.coverage=out.coverage*(1-a)+a;
    }
    return out;
  };
  const float x=std::clamp(u,0.f,1.f)*r-.5f,y=std::clamp(v,0.f,1.f)*r-.5f;
  const int ix=int(std::floor(x)),iy=int(std::floor(y));const float fx=x-ix,fy=y-iy;
  const auto a=pixel(ix,iy),b=pixel(ix+1,iy),c=pixel(ix,iy+1),d=pixel(ix+1,iy+1);
  return {std::lerp(std::lerp(a.premultiplied,b.premultiplied,fx),std::lerp(c.premultiplied,d.premultiplied,fx),fy),
    std::lerp(std::lerp(a.coverage,b.coverage,fx),std::lerp(c.coverage,d.coverage,fx),fy)};
}
}
