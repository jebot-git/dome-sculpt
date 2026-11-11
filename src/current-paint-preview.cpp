// SPDX-License-Identifier: GPL-3.0-or-later
#include "current-paint-preview.hpp"
#include "dome/polygon-scene.hpp"
#include "dome/document.hpp"
#include "texture-set-range.hpp"
#include <bit>
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace dome::detail {
namespace {
bool plain(const PaintedMesh&p)noexcept{
  return !p.source_maps()&&!p.baked()&&!p.color_projection()&&!p.normal_projection()&&
    !p.paint_sampling_domain()&&!p.paint_sampling_attachment()&&!p.source_sampling_prepared()&&
    p.surface_diagnostics().policy==PaintUvPolicy::Strict;
}
}
bool CurrentGridPaintPreview::eligible(const SculptObject&o)noexcept{
  if(!o.id||!o.mesh_active||!o.painted||o.appearance||o.material_bindings||o.bake_recipe||
     (o.polygon&&o.polygon->active))return false;
  return plain(*o.painted);
}
std::shared_ptr<const TextureMaps> CurrentGridPaintPreview::for_render(
    const SculptObject&o,const std::function<bool()>&cancelled){
  if(!eligible(o))return {};
  const auto captured=PaintOverlayFactory::capture_current_grid(*o.painted,o.material,cancelled);
  auto packet=std::shared_ptr<CurrentGridPaintPreview>(new CurrentGridPaintPreview(*captured->current_grid_preview));
  packet->object_=o.id;packet->contribution_owner_=captured->current_grid_preview;packet->self_=packet;
  auto maps=std::make_shared<TextureMaps>(*captured);maps->current_grid_preview=std::move(packet);return maps;
}
bool CurrentGridPaintPreview::eligible_field(uint64_t object,const TextureSetView&field)noexcept{
  return object&&field.painted&&!field.source&&!field.appearance&&!field.material_bindings&&
    !field.bake_recipe&&plain(*field.painted);
}
std::shared_ptr<const TextureMaps> CurrentGridPaintPreview::for_field(uint64_t object,
    const TextureSetView&field,const std::function<bool()>&cancelled){
  if(!eligible_field(object,field))return {};
  const auto captured=PaintOverlayFactory::capture_current_grid(*field.painted,field.material,cancelled);
  auto packet=std::shared_ptr<CurrentGridPaintPreview>(new CurrentGridPaintPreview(*captured->current_grid_preview));
  packet->object_=object;packet->contribution_owner_=captured->current_grid_preview;packet->self_=packet;
  auto maps=std::make_shared<TextureMaps>(*captured);maps->current_grid_preview=std::move(packet);return maps;
}
void CurrentGridPaintPreview::authenticate_range(const Mesh&mesh,const SurfaceRange&r,
    bool check_coordinates)const{
  const auto maps=r.textures;
  if(!maps||!maps->current_grid_preview)throw std::invalid_argument("Current-grid range omitted its actual packet");
  authenticate_owner(maps->current_grid_preview);
  if(object_!=r.object||!matches_material(r.material)||r.appearance||r.material_bindings||
     r.paint_sampling_attachment||r.normal_preview_proof||r.source_frame||r.canonical_frame||
     maps->source_maps||maps->source_only||maps->overlay||maps->baked_base||maps->normal_projection||
     maps->color_projection||maps->polygon_surface)
    throw std::invalid_argument("Current-grid range changed its actual object/material/plain-field context");
  for(const auto*image:{&maps->color,&maps->metal_rough,&maps->emission,&maps->color_coverage,
      &maps->normal,&maps->baked_normal,&maps->baked_coverage,&maps->emission_coverage,&maps->pbr_color,&maps->mtoon_normal})
    if(image->width||image->height||!image->rgba.empty())throw std::invalid_argument("Render-only current-grid range mixed CPU image payloads");
  if(r.texture_set_proof){const auto&expected=r.texture_set_proof->plan()->geometry();
    if(geometry_!=expected||geometry_.owner_before(expected)||expected.owner_before(geometry_))
      throw std::invalid_argument("Current-grid texture set borrowed a foreign geometry owner");
  }else if(r.count!=geometry_->indices.size())throw std::invalid_argument("Current-grid range omitted actual logical corners");
  if(check_coordinates)for(size_t c=0;c<r.count;++c){
    const auto logical=r.texture_set_proof?size_t(r.texture_set_proof->logical_corner(c,r.paint_winding_reversed)):
      r.paint_winding_reversed&&c%3?c+(c%3==1?1:-1):c;
    const auto vertex=mesh.indices.at(r.first+c);
    if(mesh.uv.at(vertex)!=geometry_->uv.at(geometry_->indices.at(logical)))
      throw std::invalid_argument("Current-grid range lost exact ordered native UV coordinates");
  }
}
void CurrentGridPaintPreview::authenticate_owner(
    const std::shared_ptr<const CurrentGridPaintPreview>&owner)const{
  const auto self=self_.lock();
  if(!self||owner!=self||owner.owner_before(self)||self.owner_before(owner))
    throw std::invalid_argument("Current-grid preview borrowed a foreign owner control block");
}
bool CurrentGridPaintPreview::matches_material(const Material&m)const noexcept{
  const std::array<float,9> expected{m.color[0],m.color[1],m.color[2],m.color[3],
    m.emissive[0],m.emissive[1],m.emissive[2],m.metallic,m.roughness};
  return std::bit_cast<std::array<uint32_t,9>>(expected)==
    std::bit_cast<std::array<uint32_t,9>>(factors_);
}
float CurrentGridPaintPreview::alpha_texel(uint32_t x,uint32_t y)const{
  const auto r=layers_->resolution();
  if(x>=r||y>=r)throw std::out_of_range("Current-grid alpha texel is outside its exact field");
  float alpha=factors_[3];const auto owner=layers_->padding()->at(size_t(y)*r+x);
  if(owner>=0){const auto px=uint32_t(owner)%r,py=uint32_t(owner)/r;
    const auto tile=size_t(py/32)*(r/32)+px/32,within=size_t(py%32)*32+px%32;
    for(const auto&layer:layers_->layers())if(const auto&q=layer.tiles.at(tile)){
      const auto&p=q->pixels[within];const float mask=float(p.data[13])/255*layer.opacity;
      const float a=mask*(float(p.data[9])/255);
      alpha=alpha*(1-a)+(float(p.data[3])/255)*a;
    }
  }
  return alpha;
}
float CurrentGridPaintPreview::sample_alpha(float u,float v)const{
  if(!std::isfinite(u)||!std::isfinite(v))throw std::invalid_argument("Invalid current-grid alpha coordinate");
  const auto r=layers_->resolution();const float x=std::clamp(u,0.f,1.f)*r-.5f,
    y=std::clamp(v,0.f,1.f)*r-.5f;const auto ix=int(std::floor(x)),iy=int(std::floor(y));
  const float fx=x-ix,fy=y-iy;
  auto sample=[&](int px,int py){const auto a=alpha_texel(uint32_t(std::clamp(px,0,int(r)-1)),uint32_t(std::clamp(py,0,int(r)-1)));
    return float(std::lround(std::clamp(a,0.f,1.f)*255))/255;};
  float result=0;for(int k=0;k<4;++k){const float weight=((k&1)?fx:1-fx)*((k>>1)?fy:1-fy);
    result+=weight*sample(ix+(k&1),iy+(k>>1));}return result;
}
} // namespace dome::detail
