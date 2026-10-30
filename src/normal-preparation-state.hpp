// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/normal-preparation.hpp"
#include "normal-projection-identity.hpp"
namespace dome::detail {
inline bool prepared_normal_state(const SourceMaterialBindings *binding,const TextureMaps *maps,bool deep=false){
  const auto recipe=binding?binding->normal_preparation:nullptr;
  const auto projection=maps?maps->normal_projection:nullptr;
  if(bool(recipe)!=bool(projection))throw std::invalid_argument("Normal preview/export lost its owning prepared interpretation");
  if(!projection)return false;
  recipe->validate(deep);projection->validate(recipe->receipt.resolution);
  if(!normal_projection_identity(*projection,*recipe->projection)||maps->source_only||maps->normal.rgba.empty()||maps->mtoon_normal.rgba.empty()||!maps->baked_normal.rgba.empty()||!maps->baked_coverage.rgba.empty())
    throw std::invalid_argument("Prepared normal preview/export has contradictory current planes or historical coverage semantics");
  maps->normal.validate();maps->mtoon_normal.validate();
  if(maps->baked_base&&maps->baked_base->normal_interpretation!=BakedNormalInterpretation::PreparedAttributes)
    throw std::invalid_argument("Prepared normal preview cannot consume a historical canonical bake");
  return true;
}
} // namespace dome::detail
