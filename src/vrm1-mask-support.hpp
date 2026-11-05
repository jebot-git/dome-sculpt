// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "vrm1-expression-layer.hpp"
#include "paint-overlay-sample.hpp"
#include "source-paint-pairing.hpp"
#include "source-extra-uv-authority.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <span>
namespace dome::detail {
namespace mask_support {
using Point=std::array<double,2>;
inline double cross(Point a,Point b,Point c){return (b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]);}
inline size_t hull(std::array<Point,12> p,std::array<Point,24> &out){
 std::sort(p.begin(),p.end());const auto end=std::unique(p.begin(),p.end());const size_t n=size_t(end-p.begin());if(n<2){out[0]=p[0];return n;}
 size_t k=0;for(size_t i=0;i<n;++i){while(k>1&&cross(out[k-2],out[k-1],p[i])<=0)--k;out[k++]=p[i];}const size_t lower=k;
 for(size_t i=n-1;i-->0;){while(k>lower&&cross(out[k-2],out[k-1],p[i])<=0)--k;out[k++]=p[i];}return k-1;
}
inline bool intersects(std::span<const Point> polygon,const std::array<double,4> &box){
 constexpr double eps=1e-12;double xmin=polygon[0][0],xmax=xmin,ymin=polygon[0][1],ymax=ymin;
 for(auto p:polygon){xmin=std::min(xmin,p[0]);xmax=std::max(xmax,p[0]);ymin=std::min(ymin,p[1]);ymax=std::max(ymax,p[1]);}
 if(xmax<box[0]-eps||xmin>box[2]+eps||ymax<box[1]-eps||ymin>box[3]+eps)return false;
 for(size_t i=0;polygon.size()>1&&i<polygon.size();++i){const auto a=polygon[i],b=polygon[(i+1)%polygon.size()];const Point normal{b[1]-a[1],a[0]-b[0]};double low=normal[0]*a[0]+normal[1]*a[1],high=low;
  for(auto p:polygon){const double q=normal[0]*p[0]+normal[1]*p[1];low=std::min(low,q);high=std::max(high,q);}
  const double lo=normal[0]*(normal[0]<0?box[2]:box[0])+normal[1]*(normal[1]<0?box[3]:box[1]),hi=normal[0]*(normal[0]<0?box[0]:box[2])+normal[1]*(normal[1]<0?box[1]:box[3]);if(high<lo-eps||low>hi+eps)return false;
 }return true;
}
}
// Derived only from real immutable source/writable owners. No caller seal or
// persisted flag grants paint permission across a changing source cutout.
inline void require_vrm1_mask_paint_support(const nlohmann::json &expressions,
 const SculptObject &object,const PaintOverlayPacket &packet,uint32_t output,
 double alpha_low,double cutoff,Vrm1ExpressionLayer &stats,
 const SourceUvBakeOptions &limits,size_t reserved_remaining,
 const std::function<bool()> &cancelled){
 using namespace mask_support;auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("VRM1 mask support compilation cancelled");};stop();
 auto charge=[&](size_t count,size_t width){if(stats.sample_operations>limits.max_sample_operations||reserved_remaining>limits.max_sample_operations-stats.sample_operations||count>(limits.max_sample_operations-stats.sample_operations-reserved_remaining)/width)throw std::length_error("VRM1 mask support prospective work limit exceeded");stats.sample_operations+=count*width;};
 if(stats.working_bytes>limits.max_working_bytes||2048>limits.max_working_bytes-stats.working_bytes)throw std::length_error("VRM1 mask support prospective working limit exceeded");stats.working_bytes+=2048;
 const auto &paint=*object.painted;const auto &binding=*object.material_bindings;const auto &texture=binding.material.textures[0];const auto &role=binding.roles[0];
 if(paint.paint_sampling_domain()||!paint.valid_source_sampling_attachment()||texture.rotation!=0||!role.image||!(role.min_filter==9728||role.min_filter==9729)||!(role.mag_filter==9728||role.mag_filter==9729))throw std::invalid_argument("VRM1 masked source can change fragment membership: source sampler/domain support adapter required");
 source_paint_pairing(&paint,&binding,object.appearance.get(),false);extra_source_uv_pair(paint,*object.appearance,cancelled);
 const auto before=paint.source_sampling_original();const auto vertices=paint.source_sampling_vertices();const auto &mesh=paint.mesh();if(!before||vertices.size()!=mesh.positions.size()||mesh.uv.size()!=mesh.positions.size())throw std::invalid_argument("VRM1 mask support lost its actual source correspondence");
 const int coordinate=texture.transform_texcoord>=0?texture.transform_texcoord:int(texture.texcoord);std::span<const std::array<float,2>> original_uv;std::shared_ptr<const std::vector<std::array<float,2>>> uv1;
 if(coordinate==0)original_uv=before->mesh().uv;else if(coordinate==1){uv1=paint.source_sampling_authored_uv1();if(!uv1&&before->source_maps())uv1=before->source_maps()->uv1;if(uv1)original_uv=*uv1;}
 if(original_uv.size()!=before->mesh().positions.size())throw std::invalid_argument("VRM1 mask support has no owning original coordinate field");
 const size_t n=packet.resolution();const double halo=1./n+1./output+1./(n*512);const auto &image=*role.image;
 size_t definitions=0,binds=0;for(const char *table:{"preset","custom"})if(expressions.contains(table))for(const auto &e:expressions.at(table)){++definitions;if(e.contains("textureTransformBinds"))binds+=e.at("textureTransformBinds").size();}
 for(size_t face=0;face<mesh.indices.size();face+=3){stop();std::array<Point,3> triangle;double xmin=1,xmax=0,ymin=1,ymax=0;
  for(size_t k=0;k<3;++k){const auto uv=mesh.uv.at(mesh.indices.at(face+k));triangle[k]={uv[0],uv[1]};xmin=std::min(xmin,double(uv[0]));xmax=std::max(xmax,double(uv[0]));ymin=std::min(ymin,double(uv[1]));ymax=std::max(ymax,double(uv[1]));}
  const int x0=std::max(0,int(std::ceil((xmin-halo)*n-.5))),x1=std::min(int(n)-1,int(std::floor((xmax+halo)*n-.5))),y0=std::max(0,int(std::ceil((ymin-halo)*n-.5))),y1=std::min(int(n)-1,int(std::floor((ymax+halo)*n-.5)));
  if(x1<x0||y1<y0)continue;
  charge(size_t(x1-x0+1)*size_t(y1-y0+1),32+packet.layers().size());bool reached=false;double maximum_coverage=0,minimum_paint_alpha=1;
  for(int y=y0;y<=y1;++y){stop();for(int x=x0;x<=x1;++x){const auto v=sample_overlay_texel_values(packet,x,y,false);const double c=v[3][0];if(c<=0)continue;const double u=(x+.5)/n,t=(y+.5)/n;if(!intersects(triangle,{u-halo,t-halo,u+halo,t+halo}))continue;reached=true;maximum_coverage=std::max(maximum_coverage,c);minimum_paint_alpha=std::min(minimum_paint_alpha,double(v[0][3])/c);}}
  if(!reached)continue;
  // Every point at every unit expression weight is in the convex hull of
  // these conservative vertex boxes. Same-expression binds share one weight.
  charge(definitions+binds+1,48);std::array<Point,12> points;std::array<Point,24> polygon;
  for(size_t k=0;k<3;++k){const auto uv=original_uv[vertices[mesh.indices[face+k]]];Point low{uv[0]*double(texture.scale[0])+texture.offset[0],uv[1]*double(texture.scale[1])+texture.offset[1]},high=low;double magnitude=1+std::abs(uv[0]*double(texture.scale[0]))+std::abs(uv[1]*double(texture.scale[1]))+std::abs(texture.offset[0])+std::abs(texture.offset[1]);
   for(const char *table:{"preset","custom"})if(expressions.contains(table))for(const auto &e:expressions.at(table)){Point ds{},offset{};if(e.contains("textureTransformBinds"))for(const auto &bind:e.at("textureTransformBinds"))if(bind.at("material").get<int>()==object.appearance->source_material){const auto scale=bind.value("scale",std::array<double,2>{1,1}),at=bind.value("offset",std::array<double,2>{});for(size_t axis=0;axis<2;++axis){ds[axis]+=scale[axis]-texture.scale[axis];offset[axis]+=at[axis]-texture.offset[axis];}}
    for(size_t axis=0;axis<2;++axis){const double delta=uv[axis]*ds[axis]+offset[axis];low[axis]+=std::min(0.,delta);high[axis]+=std::max(0.,delta);magnitude+=std::abs(delta);}}
   const double error=32*std::numeric_limits<float>::epsilon()*magnitude;for(size_t axis=0;axis<2;++axis){low[axis]-=error;high[axis]+=error;}
   points[k*4]=low;points[k*4+1]={low[0],high[1]};points[k*4+2]=high;points[k*4+3]={high[0],low[1]};
  }
  const auto count=hull(points,polygon);double umin=1,umax=0,vmin=1,vmax=0;for(size_t i=0;i<count;++i){const auto p=polygon[i];if(!std::isfinite(p[0])||!std::isfinite(p[1]))throw std::invalid_argument("VRM1 mask support source float footprint is nonfinite");umin=std::min(umin,p[0]);umax=std::max(umax,p[0]);vmin=std::min(vmin,p[1]);vmax=std::max(vmax,p[1]);}
  if(umin<1./image.width||umax>1-1./image.width||vmin<1./image.height||vmax>1-1./image.height)throw std::invalid_argument("VRM1 masked source can change fragment membership: wrapped source footprint adapter required");
  const int px0=std::max(0,int(std::floor(umin*image.width-2))),px1=std::min(int(image.width)-1,int(std::ceil(umax*image.width+1))),py0=std::max(0,int(std::floor(vmin*image.height-2))),py1=std::min(int(image.height)-1,int(std::ceil(vmax*image.height+1)));charge(size_t(px1-px0+1)*size_t(py1-py0+1),64);unsigned minimum=255;
  for(int y=py0;y<=py1;++y){stop();for(int x=px0;x<=px1;++x){const double u=(x+.5)/image.width,v=(y+.5)/image.height;if(intersects(std::span<const Point>(polygon.data(),count),{u-1./image.width,v-1./image.height,u+1./image.width,v+1./image.height}))minimum=std::min(minimum,unsigned(image.rgba[(size_t(y)*image.width+size_t(x))*4+3]));}}
  const double source_alpha=alpha_low*(minimum/255.),combined=source_alpha+std::min(0.,minimum_paint_alpha-source_alpha)*maximum_coverage;
  if(source_alpha<cutoff+1e-5||combined<cutoff+1e-5)throw std::invalid_argument("VRM1 masked source can change fragment membership within actual paint support");
 }
 stop();
}
}
