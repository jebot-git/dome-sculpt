// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/material-bindings.hpp"
#include "dome/scene.hpp"
#include <set>
namespace dome::detail {
struct VrmPaintedMaterial {
  MToonMaterial material;
  std::array<PaintSourceTexture, size_t(MToonSlot::Count)> roles;
  // Expressions on an altered factor need an explicit mapping; retaining the
  // old target while changing its texture/factor product is not preservation.
  std::set<std::string> rewritten_properties;
  size_t texels = 0;
};
inline float vrm_native_factor(float typed, float original, float current) {
  if (current == original) return typed;
  if (original != 0) return typed * current / original;
  return current == 0 ? typed : current;
}
inline bool vrm_role_alias(const PaintSourceTexture &a, const PaintSourceTexture &b,
                           const MToonTexture &ta, const MToonTexture &tb) {
  return ta.index >= 0 && ta.index == tb.index && a.image && a.image == b.image && a.wrap_s == b.wrap_s && a.wrap_t == b.wrap_t &&
      a.min_filter == b.min_filter && a.mag_filter == b.mag_filter &&
      a.uv_transform == b.uv_transform && a.texcoord == b.texcoord &&
      ta.texcoord == tb.texcoord && ta.transform_texcoord == tb.transform_texcoord;
}
// Compile the fixed-UV paint overlay into standard VRM0 main/shade/emission
// maps, using the same coverage algebra as the native renderer. Caller owns
// image registration and all original material/expression identities.
inline VrmPaintedMaterial vrm_painted_material(
    const ObjectSurface &object, MToonMaterial material,
    std::array<PaintSourceTexture, size_t(MToonSlot::Count)> roles,
    const SourceNativeMaterial &reference, uint32_t resolution,
    const std::function<bool()> &cancelled) {
  auto stop = [&] { if (cancelled && cancelled()) throw std::runtime_error("VRM paint compilation cancelled"); };
  stop();
  if (material.version() != MToonVersion::V0 || !object.textures || object.textures->source_only ||
      !object.textures->source_maps)
    throw std::invalid_argument("VRM0 painted material requires explicit source views and derived maps");
  material.validate(); object.material.validate(); reference.validate();
  const auto &maps = *object.textures;
  maps.source_maps->validate(); maps.color.validate(); maps.emission.validate(); maps.color_coverage.validate(); maps.emission_coverage.validate();
  for(const auto *image:{&maps.color,&maps.color_coverage,&maps.emission,&maps.emission_coverage})
    if(size_t(image->width)*image->height>16*1024*1024)
      throw std::length_error("VRM0 paint input image pixel budget exceeded");
  if (material.parameters.scroll != std::array<float,2>{} || material.parameters.rotation_speed != 0)
    throw std::invalid_argument("VRM0 fixed-UV paint requires an explicitly frozen animated material");
  if (material.textures[0].offset != std::array<float,2>{} || material.textures[0].scale != std::array<float,2>{1,1})
    throw std::invalid_argument("VRM0 shared source ST requires all-slot source UV baking before painted export");
  for (const auto &role : roles) {
    role.validate();
    if (role.texcoord || role.uv_transform!=std::array<float,6>{1,0,0,1,0,0})
      throw std::invalid_argument("VRM0 resolved role views must leave UV semantics in the typed material");
  }
  for (auto uv : object.mesh.uv) if (uv[0] < 0 || uv[0] > 1 || uv[1] < 0 || uv[1] > 1)
    throw std::invalid_argument("VRM0 painted export requires an explicit unit UV atlas");
  std::array<float,4> effective{};
  for (size_t c=0;c<4;++c) {
    effective[c]=vrm_native_factor(material.parameters.base_color[c],reference.color[c],object.material.color[c]);
    if (!std::isfinite(effective[c]) || effective[c]<0 || effective[c]>1)
      throw std::invalid_argument("VRM0 main color factor exceeds the standard range");
  }
  std::array<float,3> emission_factor{};
  std::array<float,3> typed_emission{material.parameters.emission.x,material.parameters.emission.y,material.parameters.emission.z};
  for(size_t c=0;c<3;++c) {
    emission_factor[c]=vrm_native_factor(typed_emission[c],reference.emission[c],object.material.emissive[c]);
    if(!std::isfinite(emission_factor[c]) || emission_factor[c]<0)throw std::invalid_argument("VRM0 emission factor is nonfinite");
  }
  auto sampler=[] { PaintSourceTexture v; v.wrap_s=v.wrap_t=33071; v.min_filter=v.mag_filter=9729; return v; };
  auto composite_view=[&](const Image &image,const PaintSourceTexture &source) {
    auto view=source;if(!view.image)view=sampler();view.texcoord=0;view.uv_transform={1,0,0,1,0,0};
    view.image=std::shared_ptr<const Image>(object.textures,&image);return view;
  };
  auto color=composite_view(maps.color,maps.source_maps->color);
  auto coverage=composite_view(maps.color_coverage,maps.source_maps->color);
  auto emission=composite_view(maps.emission,maps.source_maps->emission);
  auto emission_coverage=composite_view(maps.emission_coverage,maps.source_maps->emission);
  bool paint_emission=false;
  for(size_t i=0;i<maps.emission_coverage.rgba.size();i+=4)paint_emission|=maps.emission_coverage.rgba[i]!=0;
  auto size=std::max({maps.color.width,maps.color.height,maps.emission.width,maps.emission.height,
                     maps.color_coverage.width,maps.color_coverage.height,maps.emission_coverage.width,maps.emission_coverage.height});
  for (auto role : {size_t(MToonSlot::Base),size_t(MToonSlot::Shade)}) if (roles[role].image)
    size=std::max({size,roles[role].image->width,roles[role].image->height});
  if(paint_emission && roles[3].image)size=std::max({size,roles[3].image->width,roles[3].image->height});
  if (resolution) size=resolution;
  if (size<1 || size>4096 || size_t(size)*size>16*1024*1024)
    throw std::length_error("VRM0 painted texture pixel budget exceeded");
  bool alias=vrm_role_alias(roles[0],roles[1],material.textures[0],material.textures[1]);
  auto base=std::make_shared<Image>(Image{size,size,std::vector<uint8_t>(size_t(size)*size*4)});
  auto shade=alias?std::make_shared<Image>(Image{size,size,std::vector<uint8_t>(size_t(size)*size*4)}):nullptr;
  auto emitted=paint_emission?std::make_shared<Image>(Image{size,size,std::vector<uint8_t>(size_t(size)*size*4)}):nullptr;
  std::array<float,3> shade_scale{1,1,1};
  std::array<float,3> emission_scale{1,1,1};
  auto values=[&](float u,float v) {
    auto composed=sample_source_texture(color,u,v,true);
    auto mask=sample_source_texture(coverage,u,v,false)[0];
    auto pbr=sample_source_texture(maps.source_maps->color,u,v,true);
    auto original=sample_source_texture(roles[0],u,v,true);
    std::array<float,4> main{};
    std::array<float,3> dark{};
    for (size_t c=0;c<4;++c)
      main[c]=mask==0?effective[c]*original[c]:std::max(0.f,composed[c]+(effective[c]*original[c]-object.material.color[c]*pbr[c])*(1-mask));
    main[3]=std::clamp(main[3],0.f,1.f);
    if (alias) {
      auto original_shade=sample_source_texture(roles[1],u,v,true);
      for (size_t c=0;c<3;++c) {
        auto paint=mask==0?0.f:std::max(0.f,composed[c]-object.material.color[c]*pbr[c]*(1-mask));
        dark[c]=std::max(0.f,original_shade[c]*(1-mask)+(object.material.color[c]!=0?paint/object.material.color[c]:paint));
        if (!std::isfinite(dark[c])) throw std::invalid_argument("VRM0 painted shade is nonfinite");
      }
    }
    return std::pair{main,dark};
  };
  auto emission_values=[&](float u,float v) {
    auto composed=sample_source_texture(emission,u,v,true);
    auto mask=sample_source_texture(emission_coverage,u,v,false)[0];
    auto pbr=sample_source_texture(maps.source_maps->emission,u,v,true);
    auto original=sample_source_texture(roles[3],u,v,true);
    std::array<float,3> result{};
    for(size_t c=0;c<3;++c) {
      result[c]=mask==0?emission_factor[c]*original[c]:
        std::max(0.f,composed[c]+(emission_factor[c]*original[c]-object.material.emissive[c]*pbr[c])*(1-mask));
      if(!std::isfinite(result[c]))throw std::invalid_argument("VRM0 painted emission is nonfinite");
    }
    return result;
  };
  auto encode=[](float value) {
    if (!std::isfinite(value) || value < -1e-5f || value>1+1e-5f)
      throw std::invalid_argument("VRM0 painted main map cannot encode HDR values");
    return uint8_t(std::lround(std::clamp(value,0.f,1.f)*255));
  };
  // Shade samples can exceed one after native-factor compensation. Normalize
  // their texture range into an explicit (HDR-capable VRM0) shade factor.
  for (uint32_t y=0;y<size;++y) {
    stop();
    for (uint32_t x=0;x<size;++x) {
      auto [main,dark]=values((x+.5f)/size,(y+.5f)/size); auto at=(size_t(y)*size+x)*4;
      for (size_t c=0;c<4;++c) base->rgba[at+c]=encode(c<3?linear_to_srgb(main[c]):main[c]);
      if (alias) for (size_t c=0;c<3;++c) shade_scale[c]=std::max(shade_scale[c],dark[c]);
      if(paint_emission) {
        auto value=emission_values((x+.5f)/size,(y+.5f)/size);
        for(size_t c=0;c<3;++c)emission_scale[c]=std::max(emission_scale[c],value[c]);
      }
    }
  }
  if (alias || paint_emission) for (uint32_t y=0;y<size;++y) {
    stop();
    for (uint32_t x=0;x<size;++x) {
      auto at=(size_t(y)*size+x)*4;
      if(alias) {
        auto dark=values((x+.5f)/size,(y+.5f)/size).second;
        for (size_t c=0;c<3;++c) shade->rgba[at+c]=encode(linear_to_srgb(dark[c]/shade_scale[c]));
        shade->rgba[at+3]=255;
      }
      if(paint_emission) {
        auto value=emission_values((x+.5f)/size,(y+.5f)/size);
        for(size_t c=0;c<3;++c)emitted->rgba[at+c]=encode(linear_to_srgb(value[c]/emission_scale[c]));
        emitted->rgba[at+3]=255;
      }
    }
  }
  VrmPaintedMaterial result{material,roles,{"_Color"},size_t(size)*size};
  result.material.parameters.base_color={1,1,1,1};
  result.roles[0]=color;result.roles[0].image=std::move(base);result.material.textures[0].index=0;
  if(paint_emission) {
    result.roles[3]=emission;result.roles[3].image=std::move(emitted);result.material.textures[3].index=0;
    result.material.parameters.emission={emission_scale[0],emission_scale[1],emission_scale[2]};
    result.rewritten_properties.insert("_EmissionColor");
  } else result.material.parameters.emission={emission_factor[0],emission_factor[1],emission_factor[2]};
  if (alias) {
    result.roles[1]=roles[1];result.roles[1].image=std::move(shade);result.material.textures[1].index=0;
    auto old=result.material.parameters.shade_color;
    result.material.parameters.shade_color={old.x*shade_scale[0],old.y*shade_scale[1],old.z*shade_scale[2]};
    if (shade_scale!=std::array<float,3>{1,1,1}) result.rewritten_properties.insert("_ShadeColor");
  }
  result.material.validate(); stop(); return result;
}
} // namespace dome::detail
