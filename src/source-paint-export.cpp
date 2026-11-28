// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-paint-export.hpp"
#include "dome/material-visibility.hpp"
#include "paint-overlay-sample.hpp"
#include <limits>

namespace dome::detail {
namespace {
[[noreturn]] void portable_required() {
  throw std::invalid_argument("Painted independent source sampling requires an explicit portable material bake before standard export; the complete native project is retained");
}

struct ProofWork {
  const std::function<bool()> &cancelled;
  size_t remaining=128u*1024u*1024u;

  void stop() const {
    if(cancelled&&cancelled())throw std::runtime_error("Source paint factor proof cancelled");
  }
  void charge(size_t amount) {
    if(amount>remaining)throw std::length_error("Source paint factor proof exceeds sampling work budget");
    remaining-=amount;
  }
};

PaintOverlayValues constant_overlay(const Mesh &mesh,const PaintOverlayPacket &packet,ProofWork &work) {
  const int r=int(packet.resolution());
  // The bilinear footprint of one texel is a two-texel-wide rectangle.
  // Include a conservative margin for the viewport's FLOAT UV interpolation.
  const double radius=1+double(r)*32*std::numeric_limits<float>::epsilon();
  std::vector<bool> examined(packet.padding()->size()+1);
  std::optional<PaintOverlayValues> constant;
  struct Axis { double x,y,limit; };
  for(size_t face=0;face<mesh.indices.size();face+=3) {
    work.stop();
    std::array<std::array<double,2>,3> q;
    for(size_t k=0;k<3;++k){
      const auto uv=mesh.uv[mesh.indices[face+k]];
      q[k]={double(uv[0])*r-.5,double(uv[1])*r-.5};
    }
    const auto &a=q[0],&b=q[1],&c=q[2];
    const double area=(b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);
    if(!area)portable_required();
    const double sign=area>0?1:-1;
    std::array<Axis,3> axes;
    for(size_t k=0;k<3;++k){
      const auto &p=q[k],&next=q[(k+1)%3];
      const double x=sign*(next[1]-p[1]),y=sign*(p[0]-next[0]);
      axes[k]={x,y,x*p[0]+y*p[1]+radius*(std::abs(x)+std::abs(y))};
    }
    const int left=std::max(0,int(std::ceil(std::min({a[0],b[0],c[0]})-radius)));
    const int right=std::min(r-1,int(std::floor(std::max({a[0],b[0],c[0]})+radius)));
    const int bottom=std::max(0,int(std::ceil(std::min({a[1],b[1],c[1]})-radius)));
    const int top=std::min(r-1,int(std::floor(std::max({a[1],b[1],c[1]})+radius)));
    for(int y=bottom;y<=top;++y){
      work.stop();
      work.charge(size_t(right-left+1)*3);
      for(int x=left;x<=right;++x){
        if(std::ranges::any_of(axes,[&](const auto &axis){return axis.x*x+axis.y*y>axis.limit;}))continue;
        const int32_t owner=packet.padding()->at(size_t(y)*r+x);
        const size_t key=size_t(owner+1);
        if(examined.at(key))continue;
        examined[key]=true;
        const auto sample=owner<0?PaintOverlayTexel{}:sample_overlay_texel_owner(packet,size_t(owner),true);
        work.charge(sample.work+1);
        if(!constant)constant=sample.values;
        else if(*constant!=sample.values)portable_required();
      }
    }
  }
  if(!constant)portable_required();
  return *constant;
}

float constant_channel(const Image &image,unsigned channel,bool srgb,ProofWork &work) {
  const auto first=image.rgba[channel];
  work.charge(image.rgba.size()/4);
  for(size_t at=channel;at<image.rgba.size();at+=4){
    if((at&16383)==channel)work.stop();
    if(image.rgba[at]!=first)portable_required();
  }
  return srgb?overlay_linear_byte(first):first/255.f;
}

template<size_t N>
void fold_color(PaintSourceTexture &texture,std::array<float,N> &factor,
    const std::array<float,4> &addition,float coverage,ProofWork &work) {
  if(std::ranges::all_of(std::span(addition).first(N),[](float value){return value==0;})){
    for(auto &value:factor)value*=1-coverage;
    return;
  }
  for(unsigned channel=0;channel<N;++channel){
    const float original=texture.image&&coverage!=1&&factor[channel]!=0
      ?constant_channel(*texture.image,channel,channel<3,work):1;
    factor[channel]=factor[channel]*original*(1-coverage)+addition[channel];
  }
  texture.image.reset();
}

void fold_orm(PaintSourceTexture &texture,Material &material,const PaintOverlayValues &value,ProofWork &work) {
  std::array<bool,4> neutral{};
  const auto channel=[&](unsigned index,float &factor,float coverage,float addition){
    if(addition==0){factor*=1-coverage;return;}
    const float original=texture.image&&coverage!=1&&factor!=0
      ?constant_channel(*texture.image,index,false,work):1;
    factor=factor*original*(1-coverage)+addition;
    neutral[index]=true;
  };
  channel(1,material.roughness,value[3][2],value[1][1]);
  channel(2,material.metallic,value[3][1],value[1][2]);
  if(!texture.image)return;
  if(neutral[1]&&neutral[2])texture.image.reset();
  else if(neutral[1]||neutral[2]){
    work.charge(texture.image->rgba.size()/4);
    auto copy=std::make_shared<Image>(*texture.image);
    const size_t component=neutral[1]?1:2;
    for(size_t at=component;at<copy->rgba.size();at+=4){
      if((at&16383)==component)work.stop();
      copy->rgba[at]=255;
    }
    texture.image=std::move(copy);
  }
}
}

void fold_source_paint_factors(const PaintedMesh &paint,const ObjectSurface &object,
    const PaintOverlayPacket &packet,PaintSourceMaps &sources,Material &material,
    const std::function<bool()> &cancelled) {
  if(packet.baked()||object.mesh.uv!=paint.mesh().uv||object.mesh.indices!=paint.mesh().indices)
    portable_required();
  ProofWork work{cancelled};
  const auto value=constant_overlay(paint.mesh(),packet,work);
  if(value==PaintOverlayValues{})return;
  if((object.material_bindings&&object.material_bindings->has_mtoon)||
      retained_mtoon(object.appearance.get())||paint.color_projection()||paint.normal_projection()||
      overlay_uses_float_emission(packet)||value[1][3]!=0)portable_required();
  fold_color(sources.color,material.color,value[0],value[3][0],work);
  fold_color(sources.emission,material.emissive,value[2],value[3][3],work);
  if(std::ranges::any_of(material.emissive,[](float channel){return channel>1;}))portable_required();
  fold_orm(sources.metal_rough,material,value,work);
  material.validate();
  work.stop();
}
} // namespace dome::detail
