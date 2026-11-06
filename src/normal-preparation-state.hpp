// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/normal-preparation.hpp"
#include "normal-projection-identity.hpp"
#include "normal-sampling-access.hpp"
#include "baked-normal-field.hpp"
#include "canonical-normal-uv.hpp"
#include "polygon-surface-preview.hpp"
namespace dome::detail {
struct NormalCoordinateView {
  const std::vector<std::array<float,2>> &coordinates,&mtoon_coordinates;
  const std::vector<uint32_t> &indices;
  void validate(const std::function<bool()> &cancelled={}) const {
    if(cancelled&&cancelled())throw std::runtime_error("Retained normal coordinate view cancelled");
    if(coordinates.empty()||indices.empty()||coordinates.size()>2'000'000||indices.size()>6'000'000)
      throw std::invalid_argument("Invalid retained normal coordinate view");
  }
};
inline std::optional<NormalCoordinateView> retained_normal_coordinates(
    const SourceMaterialBindings *binding,const TextureMaps *maps){
  if(binding&&binding->canonical_uv&&maps&&maps->baked_base&&maps->baked_base->normal_field){
    const auto &uv=*binding->canonical_uv;return NormalCoordinateView{uv.coordinates(),uv.coordinates(),uv.mesh()->indices};
  }
  if(maps&&maps->normal_projection&&maps->normal_projection->version==2){
    const auto &d=*maps->normal_projection->sampling_domain;return NormalCoordinateView{d.coordinates,d.mtoon_coordinates,d.indices};
  }
  return {};
}
inline bool prepared_normal_state(const SourceMaterialBindings *binding,const TextureMaps *maps,bool deep=false){
  if(binding&&binding->polygon_surface_transfer){
    if(!maps||!maps->polygon_surface||!maps->polygon_surface->matches_binding(*binding))throw std::invalid_argument("Polygon normal preview lost its actual current field owner");
    maps->polygon_surface->validate_maps(*maps);if(deep)binding->validate();
    return maps->polygon_surface->model()==PolygonSurfaceNormalModel::PreparedCorePbr;
  }
  if(maps&&maps->polygon_surface)throw std::invalid_argument("Polygon normal preview cannot borrow a legacy material");
  if(maps&&maps->baked_base&&maps->baked_base->normal_field){
    const auto &field=maps->baked_base->normal_field;field->validate();
    if(!binding||!binding->canonical_attributes||binding->canonical_attributes!=field->origin()||
       maps->normal_projection||binding->normal_preparation||maps->source_only||
       maps->normal.rgba!=field->pbr()->rgba||maps->mtoon_normal.rgba!=field->mtoon()->rgba||
       !maps->baked_normal.rgba.empty()||!maps->baked_coverage.rgba.empty())
      throw std::invalid_argument("Independent baked preview differs from its actual owning resolved field");
    if(deep)binding->validate(true);return true;
  }
  const auto recipe=binding?binding->normal_preparation:nullptr;
  const auto projection=maps?maps->normal_projection:nullptr;
  if(bool(recipe)!=bool(projection))throw std::invalid_argument("Normal preview/export lost its owning prepared interpretation");
  if(!projection)return false;
  recipe->validate(deep);const bool sealed=!deep&&recipe->sampling_domain&&recipe->sampling_domain->has_sealed_metadata();projection->validate(recipe->receipt.resolution,!sealed);
  if(!normal_projection_identity(*projection,*recipe->projection)||maps->source_only||maps->normal.rgba.empty()||maps->mtoon_normal.rgba.empty()||!maps->baked_normal.rgba.empty()||!maps->baked_coverage.rgba.empty())
    throw std::invalid_argument("Prepared normal preview/export has contradictory current planes or historical coverage semantics");
  maps->normal.validate();maps->mtoon_normal.validate();
  if(projection->version==2){
    const auto &domain=*projection->sampling_domain;
    const bool detail=maps->baked_base&&!maps->baked_base->maps.normal.rgba.empty();
    if(detail&&!same_normal_detail(domain.detail.get(),maps->baked_base.get(),recipe->receipt.resolution,deep))
      throw std::invalid_argument("Retained normal preview lost its captured detail interpretation");
    const auto pbr=detail?domain.resolved_pbr:projection->pbr;
    const auto toon=detail?domain.resolved_mtoon:projection->mtoon;
    if(deep&&(maps->normal.rgba!=pbr->rgba||maps->mtoon_normal.rgba!=toon->rgba))
      throw std::invalid_argument("Retained normal preview planes differ from the explicit resolved/intrinsic field");
  }
  if(maps->baked_base&&maps->baked_base->normal_interpretation!=BakedNormalInterpretation::PreparedAttributes)
    throw std::invalid_argument("Prepared normal preview cannot consume a historical canonical bake");
  return true;
}
} // namespace dome::detail
