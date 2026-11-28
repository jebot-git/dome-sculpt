// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "paint-source-coordinate.hpp"
#include <cstdint>
#include <cstddef>
#include <bit>
#include <cmath>
#if defined(__SYCL_DEVICE_ONLY__)
#include <sycl/ext/intel/math/imf_rounding_math.hpp>
#endif
#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_PC_FN __host__ __device__ inline
#else
#define DOME_PC_FN inline
#endif
namespace dome::paint_compute {
struct Pixel { uint8_t data[14]{}; };
struct ColorPixel { uint8_t rgba[4]{}; };
struct Output { uint8_t color[4]{},orm[4]{},emission[4]{};uint8_t color_coverage=0,emission_coverage=0,transparent=0,reserved=0; };
inline constexpr uint8_t common_transparency=1,prepared_transparency=2;
enum OutputChannel : unsigned { Color=1, Orm=2, Emission=4, Normal=8, Pbr=16 };
inline constexpr unsigned material_channels=Color|Orm|Emission;
struct Extent {
 uint32_t width=0,height=0;
 DOME_PC_FN size_t size() const {return size_t(width)*height;}
 bool operator==(const Extent &) const=default;
};
struct OutputGrid {
 Extent extent;
 unsigned channels=0;
 DOME_PC_FN bool writes(unsigned channel) const {return channels&channel;}
};
struct ColorInfo {uint8_t coverage,transparency;};
struct OutputView {
 Output *combined=nullptr;
 uint8_t *images[3]{};
 ColorInfo *color_info=nullptr;
 uint8_t *emission_coverage=nullptr,*pbr=nullptr,*normal=nullptr;
};
struct Layer {float opacity=1;uint32_t first_tile=0;};
struct Parameters {float color[4]{},emission[3]{},metallic=0,roughness=0;uint32_t resolution=0,layers=0,tiles=0;};
struct Bake {
 const uint8_t *coverage=nullptr,*color=nullptr,*orm=nullptr,*emission=nullptr,*normal=nullptr;
};
enum SourceRole : unsigned { SourceColor, SourceOrm, SourceEmission, SourceOcclusion, SourceRoleCount };
inline constexpr unsigned source_channels[]{Color,Orm,Emission,Orm};
struct SourceTexture {
 const uint8_t *rgba=nullptr;
 const detail::SourceTextureAxis *columns=nullptr,*rows=nullptr;
 uint32_t width=0;
 bool nearest=false;
 const SourcePixelTaps *mapped=nullptr;
};
struct SourceMaterial {
 SourceTexture textures[SourceRoleCount];
 float occlusion_strength=1;
 uint32_t first=0;
};
struct PreparedColor {
 const uint8_t *vertex=nullptr,*baked=nullptr;
 const uint32_t *tiles=nullptr;
 const ColorPixel *pixels=nullptr;
 SourceTexture sampled_vertex;
};
static_assert(SourceRoleCount==source_coordinate_roles);
struct Quantization {float unit[256]{},srgb[256]{};uint32_t threshold[255]{};uint32_t reserved=0;};
inline constexpr uint32_t absent=UINT32_MAX;
// Volatile float expression boundaries prohibit contraction without changing
// brush compilation flags or relying on the device's transcendental library.
DOME_PC_FN float mul(float a,float b){volatile float r=a*b;return r;}
DOME_PC_FN float add(float a,float b){volatile float r=a+b;return r;}
DOME_PC_FN float sub(float a,float b){volatile float r=a-b;return r;}
DOME_PC_FN uint32_t bits(float value){
#if defined(__CUDA_ARCH__) || defined(__HIP_DEVICE_COMPILE__)
 return __float_as_uint(value);
#else
 return std::bit_cast<uint32_t>(value);
#endif
}
DOME_PC_FN uint8_t linear_byte(float v){
 if(v<=0)return 0;if(v>=1)return 255;
 float scaled=mul(v,255);uint32_t integer=uint32_t(scaled);
 return uint8_t(integer+(sub(scaled,float(integer))>=.5f));
}
DOME_PC_FN uint8_t srgb_byte(float v,const Quantization &q){
 if(v<=0)return 0;if(v>=1)return 255;
 const auto word=bits(v);uint32_t lo=0,hi=255;
 while(lo<hi){const auto mid=(lo+hi)/2;if(word>=q.threshold[mid])lo=mid+1;else hi=mid;}
 return uint8_t(lo);
}
DOME_PC_FN float blend(float base,float paint,float a){return add(mul(base,sub(1,a)),mul(paint,a));}
DOME_PC_FN void sample_source_taps(const uint8_t *image,uint32_t width,bool nearest,SourcePixelTaps taps,bool srgb,
                                   const Quantization &q,float (&result)[4]){
 const auto column=taps.column,row=taps.row;
 const auto decode=[&](const uint8_t *rgba,unsigned c){return srgb&&c<3?q.srgb[rgba[c]]:q.unit[rgba[c]];};
 if(nearest){
  const auto *rgba=image+(size_t(row.first)*width+column.first)*4;
  for(unsigned c=0;c<4;++c)result[c]=decode(rgba,c);
  return;
 }
 for(auto &channel:result)channel=0;
 for(unsigned tap=0;tap<4;++tap){
  const bool right=tap&1,bottom=tap&2;
  const auto *rgba=image+(size_t(bottom?row.second:row.first)*width+(right?column.second:column.first))*4;
  const float weight=mul(right?column.fraction:sub(1,column.fraction),bottom?row.fraction:sub(1,row.fraction));
  for(unsigned c=0;c<4;++c)result[c]=add(result[c],mul(weight,decode(rgba,c)));
 }
}
DOME_PC_FN void sample_source(SourceTexture texture,uint32_t x,uint32_t y,uint32_t index,bool srgb,
                              const Quantization &q,float (&result)[4]){
 if(!texture.rgba){
  for(auto &channel:result)channel=1;
  return;
 }
 const auto taps=texture.mapped?texture.mapped[index]:SourcePixelTaps{texture.columns[x],texture.rows[y]};
 sample_source_taps(texture.rgba,texture.width,texture.nearest,taps,srgb,q,result);
}
// Native normals normalize in two separate binary32 steps. Fast device
// reciprocals can cross a final byte-rounding boundary even one ULP away.
DOME_PC_FN float normal_length(float squared){
#if defined(__SYCL_DEVICE_ONLY__)
 return sycl::ext::intel::math::fsqrt_rn(squared);
#elif defined(__HIP_DEVICE_COMPILE__)
 // The wider HIP operation leaves room around the binary32 midpoint.
 return float(std::sqrt(double(squared)));
#elif defined(__CUDA_ARCH__)
 return __fsqrt_rn(squared);
#else
 return std::sqrt(squared);
#endif
}
DOME_PC_FN float normal_reciprocal(float length){
#if defined(__SYCL_DEVICE_ONLY__)
 return sycl::ext::intel::math::frcp_rn(length);
#elif defined(__HIP_DEVICE_COMPILE__)
 return float(1.0/double(length));
#elif defined(__CUDA_ARCH__)
 return __frcp_rn(length);
#else
 return 1.f/length;
#endif
}
DOME_PC_FN void compose_normal(int32_t owner,Bake bake,const Quantization &q,uint8_t *out){
 out[0]=out[1]=128;
 out[2]=out[3]=255;
 if(owner<0)return;
 const float coverage=q.unit[bake.coverage[owner]];
 const auto *normal=bake.normal+size_t(owner)*4;
 const auto component=[&](uint8_t value){return mul(sub(mul(q.unit[value],2),1),coverage);};
 const float x=component(normal[0]),y=component(normal[1]);
 const float z=add(component(normal[2]),sub(1,coverage));
 const float length=normal_length(add(add(mul(x,x),mul(y,y)),mul(z,z)));
 if(length<1e-8f)return;
 const float scale=normal_reciprocal(length);
 out[0]=linear_byte(add(mul(mul(x,scale),.5f),.5f));
 out[1]=linear_byte(add(mul(mul(y,scale),.5f),.5f));
 out[2]=linear_byte(add(mul(mul(z,scale),.5f),.5f));
}
DOME_PC_FN uint32_t paint_coordinate(uint32_t pixel,uint32_t extent,uint32_t resolution){
 // Native power-of-two resolution scales the rounded center exactly. An
 // integral cell boundary is representable before scaling, and other centers
 // are at least 1/extent away, beyond half a binary32 ULP at this resolution.
 return ((2*pixel+1)*resolution)/(2*extent);
}
DOME_PC_FN Output compose(uint32_t at,OutputGrid grid,const Parameters &p,const Layer *layers,const uint32_t *tiles,
                         const Pixel *pixels,const int32_t *padding,Bake bake,PreparedColor prepared,
                         const SourceMaterial &source,const Quantization &q,uint8_t *prepared_out){
 const uint32_t column=at%grid.extent.width,row=at/grid.extent.width;
 const uint32_t index=at-source.first;
 const bool write_color=grid.writes(Color),write_orm=grid.writes(Orm),write_emission=grid.writes(Emission);
 float color[4]{},pbr[4]{},emission[4]{};
 float ao=0,rough=0,metal=0,cc=0,ec=0;
 if(write_color){
  sample_source(source.textures[SourceColor],column,row,index,true,q,color);
  for(unsigned c=0;c<4;++c)color[c]=mul(color[c],p.color[c]);
  if(prepared_out){
   if(prepared.sampled_vertex.rgba){
    sample_source(prepared.sampled_vertex,column,row,index,false,q,pbr);
    for(unsigned c=0;c<4;++c)pbr[c]=mul(color[c],pbr[c]);
   }else{
    const auto *vertex=prepared.vertex+size_t(at)*4;
    for(unsigned c=0;c<4;++c)pbr[c]=mul(color[c],q.unit[vertex[c]]);
   }
  }
 }
 if(write_orm){
  float orm[4],occlusion[4];
  sample_source(source.textures[SourceOrm],column,row,index,false,q,orm);
  sample_source(source.textures[SourceOcclusion],column,row,index,false,q,occlusion);
  ao=sub(1,mul(source.occlusion_strength,sub(1,occlusion[0])));
  rough=mul(p.roughness,orm[1]);metal=mul(p.metallic,orm[2]);
 }
 if(write_emission){
  sample_source(source.textures[SourceEmission],column,row,index,true,q,emission);
  for(unsigned c=0;c<3;++c)emission[c]=mul(p.emission[c],emission[c]);
 }
 const bool native=grid.extent.width==p.resolution&&grid.extent.height==p.resolution;
 const auto owner=padding[native?at:paint_coordinate(row,grid.extent.height,p.resolution)*p.resolution+
                                  paint_coordinate(column,grid.extent.width,p.resolution)];
 if(owner>=0){
  if(bake.coverage){
   const auto offset=size_t(owner)*4;
   const float weight=q.unit[bake.coverage[owner]];
   cc=ec=weight;
   if(write_color){
    for(unsigned c=0;c<3;++c)color[c]=blend(color[c],q.srgb[bake.color[offset+c]],weight);
    color[3]=blend(color[3],q.unit[bake.color[offset+3]],weight);
   }
   if(write_emission)for(unsigned c=0;c<3;++c)emission[c]=blend(emission[c],q.srgb[bake.emission[offset+c]],weight);
   if(prepared_out)for(unsigned c=0;c<4;++c){
    const auto value=prepared.baked[offset+c];
    pbr[c]=blend(pbr[c],c<3?q.srgb[value]:q.unit[value],weight);
   }
   if(write_orm){
    ao=blend(ao,q.unit[bake.orm[offset]],weight);
    rough=blend(rough,q.unit[bake.orm[offset+1]],weight);
    metal=blend(metal,q.unit[bake.orm[offset+2]],weight);
   }
  }
  const auto x=uint32_t(owner)%p.resolution,y=uint32_t(owner)/p.resolution;
  const auto tile=(y/32)*((p.resolution+31)/32)+x/32,offset=(y%32)*32+x%32;
  for(uint32_t li=0;li<p.layers;++li){
   const auto id=tiles[layers[li].first_tile+tile];if(id==absent)continue;
   const auto &v=pixels[size_t(id)*1024+offset];const auto mask=mul(q.unit[v.data[13]],layers[li].opacity);
   if(write_color){
    const auto a=mul(mask,q.unit[v.data[9]]);
    cc=add(mul(cc,sub(1,a)),a);
    for(uint32_t c=0;c<4;++c)color[c]=blend(color[c],c<3?q.srgb[v.data[c]]:q.unit[v.data[c]],a);
    if(prepared_out){
     const auto pbr_id=prepared.tiles?prepared.tiles[id]:absent;
     const auto *rgba=pbr_id==absent?v.data:prepared.pixels[size_t(pbr_id)*1024+offset].rgba;
     for(unsigned c=0;c<4;++c)pbr[c]=blend(pbr[c],c<3?q.srgb[rgba[c]]:q.unit[rgba[c]],a);
    }
   }
   if(write_orm){
    rough=blend(rough,q.unit[v.data[5]],mul(mask,q.unit[v.data[11]]));
    metal=blend(metal,q.unit[v.data[4]],mul(mask,q.unit[v.data[10]]));
   }
   if(write_emission){
    const auto a=mul(mask,q.unit[v.data[12]]);
    ec=add(mul(ec,sub(1,a)),a);
    for(uint32_t c=0;c<3;++c)emission[c]=blend(emission[c],q.srgb[v.data[6+c]],a);
   }
  }
 }
 Output out;
 if(write_color){
  for(unsigned c=0;c<3;++c)out.color[c]=srgb_byte(color[c],q);
  out.color[3]=linear_byte(color[3]);out.color_coverage=linear_byte(cc);
  if(color[3]<.999f)out.transparent=common_transparency;
 }
 if(write_orm){out.orm[0]=linear_byte(ao);out.orm[1]=linear_byte(rough);out.orm[2]=linear_byte(metal);out.orm[3]=255;}
 if(write_emission){
  for(unsigned c=0;c<3;++c)out.emission[c]=srgb_byte(emission[c],q);
  out.emission[3]=255;out.emission_coverage=linear_byte(ec);
 }
 if(prepared_out){
  for(unsigned c=0;c<3;++c)prepared_out[c]=srgb_byte(pbr[c],q);
  prepared_out[3]=linear_byte(pbr[3]);
  if(pbr[3]<.999f)out.transparent|=prepared_transparency;
 }
 return out;
}
DOME_PC_FN void write_output(OutputView view,OutputGrid grid,uint32_t index,uint32_t at,const Parameters &p,
    const Layer *layers,const uint32_t *tiles,const Pixel *pixels,const int32_t *padding,Bake bake,
    const PreparedColor &prepared,const SourceMaterial &source,const Quantization &q){
 if(view.normal)compose_normal(padding[at],bake,q,view.normal+size_t(index)*4);
 if(!(grid.channels&material_channels))return;
 const auto value=compose(at,grid,p,layers,tiles,pixels,padding,bake,prepared,source,q,
                         view.pbr?view.pbr+size_t(index)*4:nullptr);
 if(view.combined){view.combined[index]=value;return;}
 const uint8_t *colors[]{value.color,value.orm,value.emission};
 for(unsigned plane=0;plane<3;++plane)if(view.images[plane])
  for(unsigned c=0;c<4;++c)view.images[plane][size_t(index)*4+c]=colors[plane][c];
 if(view.color_info)view.color_info[index]={value.color_coverage,value.transparent};
 if(view.emission_coverage)view.emission_coverage[index]=value.emission_coverage;
}
static_assert(sizeof(Pixel)==14&&sizeof(ColorPixel)==4&&sizeof(Output)==16&&sizeof(Layer)==8&&sizeof(Parameters)==48&&sizeof(Quantization)==3072);
}
#undef DOME_PC_FN
