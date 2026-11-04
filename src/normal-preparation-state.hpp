// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/normal-preparation.hpp"
#include "normal-projection-identity.hpp"
#include "normal-sampling-access.hpp"
#include "baked-normal-field.hpp"
namespace dome::detail {
inline bool prepared_normal_state(const SourceMaterialBindings *binding,const TextureMaps *maps,bool deep=false){
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
