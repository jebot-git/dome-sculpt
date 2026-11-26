// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "baked-emission-access.hpp"
#include "emission-paint-access.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <span>

namespace dome::detail {

// An offline view of the immutable writable raster. As in the viewport, filter
// the independent premultiplied contributions after composing each texel.
class PaintEmissionSampler {
public:
  explicit PaintEmissionSampler(const PaintedMesh &paint,
                                const std::function<bool()> &cancelled={})
      : padding_(EmissionExportAccess::writable_padding(paint)), baked_(paint.baked().get()),
        resolution_(paint.resolution()) {
    for(const auto &layer:paint.layers()){
      if(!layer.visible||layer.opacity<=0)continue;
      if(cancelled&&cancelled())throw std::runtime_error("Native paint emission capture cancelled");
      index_work_+=layer.tiles.size();
      if(std::any_of(layer.tiles.begin(),layer.tiles.end(),[](const auto &tile){return bool(tile);}))
        layers_[layer_count_++]=&layer;
    }
  }
  size_t work_per_sample() const noexcept { return 36+48*layer_count_; }
  size_t index_work() const noexcept { return index_work_; }

  BakedEmissionSample sample_texel(uint32_t x,uint32_t y) const {
    return texel(int(x),int(y));
  }
  size_t work_per_texel() const noexcept { return 7+12*layer_count_; }
  BakedEmissionSample sample(float u,float v) const {
    if(!std::isfinite(u)||!std::isfinite(v))
      throw std::invalid_argument("Nonfinite native paint emission sample");
    const float x=std::clamp(u,0.f,1.f)*resolution_-.5f;
    const float y=std::clamp(v,0.f,1.f)*resolution_-.5f;
    const int ix=int(std::floor(x)),iy=int(std::floor(y));
    const float fx=x-ix,fy=y-iy;
    const auto a=texel(ix,iy),b=texel(ix+1,iy),c=texel(ix,iy+1),d=texel(ix+1,iy+1);
    const auto filter=[&](float p,float q,float r,float s){
      return std::lerp(std::lerp(p,q,fx),std::lerp(r,s,fx),fy);
    };
    BakedEmissionSample result;
    for(size_t k=0;k<3;++k){
      result.legacy[k]=filter(a.legacy[k],b.legacy[k],c.legacy[k],d.legacy[k]);
      result.absolute[k]=filter(a.absolute[k],b.absolute[k],c.absolute[k],d.absolute[k]);
    }
    result.coverage=filter(a.coverage,b.coverage,c.coverage,d.coverage);
    return result;
  }

private:
  BakedEmissionSample texel(int x,int y) const {
    x=std::clamp(x,0,int(resolution_)-1);
    y=std::clamp(y,0,int(resolution_)-1);
    const auto owner=padding_[size_t(y)*resolution_+x];
    BakedEmissionSample value;
    if(owner<0)return value;
    const size_t at=size_t(owner),px=at%resolution_,py=at/resolution_;
    const size_t tile=(py/32)*(resolution_/32)+px/32,within=(py%32)*32+px%32;
    if(baked_)value=baked_emission_texel(*baked_,at);
    const auto linear=[](float x){
      return x<=.04045f?x/12.92f:std::pow((x+.055f)/1.055f,2.4f);
    };
    for(size_t i=0;i<layer_count_;++i){
      const auto &layer=*layers_[i];
      const auto &common=layer.tiles[tile];
      if(!common)continue;
      const auto &q=common->pixels[within].data;
      const float mask=q[13]/255.f*layer.opacity;
      const auto *emission=layer.emission_tiles.empty()?nullptr:layer.emission_tiles[tile].get();
      const auto *physical=emission?&emission->pixels[within]:nullptr;
      const float coverage=physical?mask*physical->coverage:q[12]/255.f*mask;
      for(size_t k=0;k<3;++k){
        const float relative=physical?physical->legacy[k]:linear(q[6+k]/255.f);
        const float absolute=physical?physical->physical[k]:0.f;
        value.legacy[k]=value.legacy[k]*(1-coverage)+relative*coverage;
        value.absolute[k]=value.absolute[k]*(1-coverage)+absolute*coverage;
      }
      value.coverage=value.coverage*(1-coverage)+coverage;
    }
    return value;
  }

  std::span<const int32_t> padding_;
  std::array<const PaintLayer*,16> layers_{};
  size_t layer_count_=0,index_work_=0;
  const BakedTextures *baked_;
  uint32_t resolution_;
};

} // namespace dome::detail
