// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake-material-device.hpp"
#include "dome/bake.hpp"
#include "dome/source-maps.hpp"
#include "paint-composite-backend.hpp"
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
  const paint_compute::MaterialAtlas *atlas=nullptr;
  TransferLayout transfer{};

  MaterialInput(const BakeMaterial &material,bool separate_pbr,const Image *normal,bool sample_normals=false,
                const PaintSourceTexture *retained=nullptr,const paint_compute::MaterialAtlas *resident=nullptr,
                bool retained_radiance=false)
      :retained_normal(retained),atlas(resident) {
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
    }else if(material.maps&&!atlas){
      const auto &maps=*material.maps;
      images[Color]={&maps.color,nullptr,material.repeat_uv};
      images[Orm]={&maps.metal_rough,nullptr,material.repeat_uv};
      if(!material.emission_batch&&!maps.emission.rgba.empty())
        images[Emission]={&maps.emission,nullptr,material.repeat_uv};
      if(separate_pbr&&!maps.pbr_color.rgba.empty())images[PbrColor]={&maps.pbr_color};
    }
    if(retained_normal)images[TangentNormal]={retained_normal->image.get(),retained_normal};
    else if(normal)images[TangentNormal]={normal,nullptr,material.repeat_uv};
    else if(sample_normals&&source&&source->normal.image&&source->normal_scale>0&&material.normal_strength>0)
      images[TangentNormal]={source->normal.image.get(),&source->normal};
    parameters.include_baked_normal=sample_normals&&covered_normal_image(material);
    auto &layout=transfer.coordinates;
    const auto add=[&](uint8_t domain){
      const auto end=domains.begin()+layout.count;
      if(const auto found=std::find(domains.begin(),end,domain);found!=end)
        return uint8_t(found-domains.begin());
      const auto column=layout.count++;
      domains[column]=domain;
      return column;
    };
    for(unsigned i=0;i<RoleCount;++i){
      if(!images[i].image&&(!atlas||i>Emission||!atlas->plane(i)))continue;
      auto domain=uint8_t(i==TangentNormal?High:Paint);
      if(i==TangentNormal&&retained_normal)domain=RetainedNormal;
      else if(const auto *original=images[i].original){
        if(unsigned(original->texcoord)>=Paint)throw std::invalid_argument("Bake texture coordinate set exceeds bounds");
        domain=original->texcoord;
      }
      layout.roles[i]=add(domain);
    }
    if(overlay)layout.paint=add(Paint);
    if(parameters.include_baked_normal)layout.high=add(High);

    auto &output=transfer.results;
    output.emission=!retained_radiance&&!material.resolved_emission;
    output.pbr=images[PbrColor].image||(overlay&&parameters.separate_pbr);
    output.normal=bool(images[TangentNormal].image);
    output.baked_normal=parameters.include_baked_normal;
    output.stride=output.channel_count();
  }

  bool needs_sampling() const {return transfer.coordinates.count!=0;}

  void coordinates(const Mesh &mesh,uint32_t triangle,std::array<float,3> bary,
                   Uv paint_uv,Uv high_uv,Uv normal_uv,std::span<Uv> row) const {
    for(size_t i=0;i<row.size();++i){
      const auto domain=domains[i];
      switch(domain){
        case Paint: row[i]=paint_uv;break;
        case High: row[i]=high_uv;break;
        case RetainedNormal: row[i]=normal_uv;break;
        default: row[i]=source_texture_uv(mesh,*source,domain,triangle,bary);break;
      }
    }
  }
private:
  // glTF coordinate sets occupy the eight lower domain IDs.
  enum : uint8_t { Paint=8,High,RetainedNormal };
  std::array<uint8_t,RoleCount+2> domains{};
};
} // namespace dome::detail::bake_material
