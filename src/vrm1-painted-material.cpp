// SPDX-License-Identifier: GPL-3.0-or-later
#include "vrm1-painted-material.hpp"
#include "paint-overlay-sample.hpp"
#include "vrm-painted.hpp"
#include <algorithm>
#include <cmath>
namespace dome::detail {
namespace {
float linear(float v){return v<=.04045f?v/12.92f:std::pow((v+.055f)/1.055f,2.4f);}
float srgb(float v){return v<=.0031308f?12.92f*v:1.055f*std::pow(v,1/2.4f)-.055f;}
bool shade_alias(const SourceMaterialBindings &binding){
 const auto &a=binding.roles[size_t(MToonSlot::Base)],&b=binding.roles[size_t(MToonSlot::Shade)];
 const auto &x=binding.material.textures[size_t(MToonSlot::Base)],&y=binding.material.textures[size_t(MToonSlot::Shade)];
 return a.image&&a.image==b.image&&a.wrap_s==b.wrap_s&&a.wrap_t==b.wrap_t&&a.min_filter==b.min_filter&&a.mag_filter==b.mag_filter&&mtoon_coordinate(x)==mtoon_coordinate(y)&&x.offset==y.offset&&x.scale==y.scale&&x.rotation==y.rotation;
}
PaintSourceTexture finite_image(std::shared_ptr<const Image> image){PaintSourceTexture v;v.image=std::move(image);v.wrap_s=v.wrap_t=33071;v.min_filter=v.mag_filter=9729;return v;}
}
Vrm1PaintedMaterial compose_vrm1_paint(const ObjectSurface &object,MToonMaterial material,
 std::array<PaintSourceTexture,size_t(MToonSlot::Count)> roles,
 const SourceMaterialBindings &authored,const PaintOverlayPacket &packet,
 uint32_t resolution,size_t maximum_image_bytes,const std::function<bool()> &cancelled){
 auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("VRM1 painted material cancelled");};stop();
 if(material.version()!=MToonVersion::V1||!authored.has_mtoon||authored.material.version()!=MToonVersion::V1)
  throw std::invalid_argument("VRM1 paint compilation requires genuine typed V1 material owners");
 authored.validate();material.validate();object.material.validate();
 if(material.parameters.scroll!=std::array<float,2>{}||material.parameters.rotation_speed!=0)
  throw std::invalid_argument("VRM1 painted sampling requires explicit frozen UV animation");
 // The caller supplies genuinely sampled typed roles, never a finite-PBR
 // material substituted for MToon. All contribution values stay linear until
 // this explicit portable PNG encoding; the original Scene remains exact.
 uint32_t size=resolution?resolution:packet.resolution();if(!resolution)for(auto role:{size_t(MToonSlot::Base),size_t(MToonSlot::Shade),size_t(MToonSlot::Emission)})if(roles[role].image)size=std::max({size,roles[role].image->width,roles[role].image->height});
 if(size<64||size>4096)throw std::length_error("VRM1 painted texture size exceeds64..4096");
 const size_t pixels=size_t(size)*size;if(pixels>16*1024*1024)throw std::length_error("VRM1 painted pixel budget exceeded");
 bool color=false,emission=false;
 for(uint32_t y=0;y<size;++y){stop();for(uint32_t x=0;x<size;++x){const auto v=sample_overlay_values(packet,(x+.5f)/size,(y+.5f)/size,false);color|=v[3][0]!=0;emission|=v[3][3]!=0;
  for(size_t c=0;c<3;++c)if(v[4][c]!=0)throw std::invalid_argument("VRM1 physical paint requires its precision-qualified typed material adapter");
 }}
 const bool alias=color&&shade_alias(authored);const size_t count=size_t(color)+size_t(alias)+size_t(emission);
 if(count&&pixels>maximum_image_bytes/(count*4))throw std::length_error("VRM1 painted images exceed prospective output budget");
 for(size_t role=0;role<roles.size();++role)if(roles[role].image&&role!=size_t(MToonSlot::Matcap)){
  const auto &t=material.textures[role];if(roles[role].texcoord||roles[role].uv_transform!=std::array<float,6>{1,0,0,1,0,0}||mtoon_coordinate(t)||t.offset!=std::array<float,2>{}||t.scale!=std::array<float,2>{1,1}||t.rotation!=0)
   throw std::invalid_argument("VRM1 painted roles require their actual resolved unit sampling domain");
 }
 std::array<float,4> effective{};for(size_t k=0;k<4;++k)effective[k]=vrm_native_factor(material.parameters.base_color[k],authored.native_reference.color[k],object.material.color[k]);
 const std::array<float,3> old_emission{material.parameters.emission.x,material.parameters.emission.y,material.parameters.emission.z};std::array<float,3> effective_emission{};
 for(size_t k=0;k<3;++k)effective_emission[k]=vrm_native_factor(old_emission[k],authored.native_reference.emission[k],object.material.emissive[k]);
 if(emission&&material.parameters.emission_strength!=1)throw std::invalid_argument("VRM1 painted authored emission strength requires the precision-qualified typed adapter");
 auto image=[&]{return std::make_shared<Image>(Image{size,size,std::vector<uint8_t>(pixels*4)});};
 auto base=color?image():nullptr,dark=alias?image():nullptr,glow=emission?image():nullptr;
 Vrm1PaintedMaterial result{material,roles,{},count*pixels*4,{}};
 auto encode=[&](float v,bool rgb,size_t channel){
  if(!std::isfinite(v)||v<0||v>1+1e-6f)throw std::invalid_argument("VRM1 painted value exceeds standard unit representation; exact native data remains owned");
  const float value=std::clamp(v,0.f,1.f);const auto q=uint8_t(std::clamp(std::lround((rgb?srgb(value):value)*255),0l,255l));const float restored=rgb?linear(float(q)/255):float(q)/255;result.maximum_quantization_error[channel]=std::max(result.maximum_quantization_error[channel],std::abs(v-restored));return q;
 };
 for(uint32_t y=0;y<size;++y){stop();for(uint32_t x=0;x<size;++x){const float u=(x+.5f)/size,v=(y+.5f)/size;const auto at=(size_t(y)*size+x)*4;const auto paint=sample_overlay_values(packet,u,v,false);
  if(color){const auto original=sample_source_texture(roles[0],u,v,true);const float coverage=paint[3][0];
   for(size_t k=0;k<4;++k){float value=effective[k]*original[k]*(1-coverage)+paint[0][k];if(k==3)value=std::clamp(value,0.f,1.f);base->rgba[at+k]=encode(value,k<3,0);}
   if(alias){const auto original_shade=sample_source_texture(roles[1],u,v,true);const std::array<float,3> factor{material.parameters.shade_color.x,material.parameters.shade_color.y,material.parameters.shade_color.z};
    for(size_t k=0;k<3;++k){const float contribution=object.material.color[k]!=0?paint[0][k]/object.material.color[k]:paint[0][k];dark->rgba[at+k]=encode(factor[k]*(original_shade[k]*(1-coverage)+contribution),true,1);}dark->rgba[at+3]=255;
   }
  }
  if(emission){const auto original=sample_source_texture(roles[3],u,v,true);const float coverage=paint[3][3];
   for(size_t k=0;k<3;++k)glow->rgba[at+k]=encode(effective_emission[k]*original[k]*(1-coverage)+paint[2][k],true,2);
   glow->rgba[at+3]=255;
  }
 }}
 if(color){result.material.parameters.base_color={1,1,1,1};result.roles[0]=finite_image(base);result.material.textures[0].index=0;result.rewritten_properties.insert("color");}else result.material.parameters.base_color=effective;
 if(alias){result.material.parameters.shade_color={1,1,1};result.roles[1]=finite_image(dark);result.material.textures[1].index=0;result.rewritten_properties.insert("shadeColor");}
 if(emission){result.material.parameters.emission={1,1,1};result.roles[3]=finite_image(glow);result.material.textures[3].index=0;result.rewritten_properties.insert("emissionColor");}else result.material.parameters.emission={effective_emission[0],effective_emission[1],effective_emission[2]};
 result.material.validate();stop();return result;
}
}
