// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake-material-device.hpp"
#include "dome/bake.hpp"
#include "dome/source-maps.hpp"
#include "paint-overlay-packet.hpp"

namespace dome::detail::bake_material {
inline const Image *covered_normal_image(const BakeMaterial &material){
  if(material.normal_strength<=0||!material.maps||!material.maps->source_only||
     !material.maps->overlay||!material.maps->baked_base)return nullptr;
  const auto &image=material.maps->baked_base->maps.normal;
  return image.rgba.empty()?nullptr:&image;
}

struct MaterialInput {
  Parameters parameters{};
  ImageInput images[RoleCount]{};
  const PaintSourceMaps *source=nullptr;
  const PaintOverlayPacket *overlay=nullptr;
  const PaintSourceTexture *retained_normal=nullptr;

  MaterialInput(const BakeMaterial &material,bool separate_pbr,const Image *normal,bool sample_normals=false,
                const PaintSourceTexture *retained=nullptr):retained_normal(retained) {
    const auto &factor=material.material;
    std::copy_n(factor.color.begin(),4,parameters.color);
    std::copy_n(factor.emissive.begin(),3,parameters.emission);
    parameters.roughness=factor.roughness;
    parameters.metallic=factor.metallic;
    parameters.composited=material.maps_are_composited;
    if(material.maps)source=material.maps->source_maps.get();
    if(material.maps&&material.maps->source_only) {
      if(material.maps->overlay&&!material.maps->overlay->empty())overlay=material.maps->overlay.get();
      parameters.original=true;
      parameters.separate_pbr=separate_pbr;
      parameters.include_overlay_emission=!material.resolved_emission;
      parameters.occlusion_strength=source->occlusion_strength;
      const auto set=[&](Role role,const PaintSourceTexture &texture){
        if(texture.image)images[role]={texture.image.get(),&texture};
      };
      set(Color,source->color);
      set(Orm,source->metal_rough);
      if(!material.emission_batch)set(Emission,source->emission);
      set(Occlusion,source->occlusion);
    }else if(material.maps){
      const auto &maps=*material.maps;
      images[Color]={&maps.color,nullptr,material.repeat_uv};
      images[Orm]={&maps.metal_rough,nullptr,material.repeat_uv};
      if(!material.emission_batch)images[Emission]={&maps.emission,nullptr,material.repeat_uv};
      if(separate_pbr&&!maps.pbr_color.rgba.empty())images[PbrColor]={&maps.pbr_color};
    }
    if(retained_normal)images[TangentNormal]={retained_normal->image.get(),retained_normal};
    else if(normal)images[TangentNormal]={normal,nullptr,material.repeat_uv};
    else if(sample_normals&&source&&source->normal.image&&source->normal_scale>0&&material.normal_strength>0)
      images[TangentNormal]={source->normal.image.get(),&source->normal};
    parameters.include_baked_normal=sample_normals&&covered_normal_image(material);
  }

  bool needs_sampling() const {
    return overlay||std::ranges::any_of(images,[](const auto &role){return role.image!=nullptr;});
  }

  Coordinates coordinates(const Mesh &mesh,uint32_t triangle,std::array<float,3> bary,
                          std::array<float,2> paint_uv,std::array<float,2> high_uv,
                          std::array<float,2> normal_uv={}) const {
    Coordinates result{};
    std::copy_n(paint_uv.begin(),2,result.paint_uv);
    if(parameters.include_baked_normal)std::copy_n(high_uv.begin(),2,result.high_uv);
    std::array<std::array<float,2>,8> source_uv;
    unsigned ready=0;
    for(unsigned i=0;i<RoleCount;++i){
      if(!images[i].image)continue;
      auto uv=i==TangentNormal?high_uv:paint_uv;
      if(i==TangentNormal&&retained_normal)uv=normal_uv;
      else if(const auto *original=images[i].original){
        const unsigned set=original->texcoord;
        if(!(ready&(1u<<set))){
          source_uv[set]=source_texture_uv(mesh,*source,int(set),triangle,bary);
          ready|=1u<<set;
        }
        uv=source_uv[set];
      }
      result.uv[i][0]=uv[0];result.uv[i][1]=uv[1];
    }
    return result;
  }
};
} // namespace dome::detail::bake_material
