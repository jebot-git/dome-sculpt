// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/normal-preparation.hpp"
#include "normal-projection-identity.hpp"
namespace dome::detail {
// Call after holder validation. A retained paint needs its OWN interpretation;
// a current object or a neighboring Origin cannot authenticate that before-state.
inline void source_paint_pairing(const PaintedMesh *paint,const SourceMaterialBindings *binding,
    const CreatorAppearance *appearance,bool deep){
  const auto color=paint?paint->color_projection():nullptr;
  const auto normal=paint?paint->normal_projection():nullptr;
  const auto color_recipe=binding?binding->color_preparation:nullptr;
  const auto normal_recipe=binding?binding->normal_preparation:nullptr;
  if(bool(color)!=bool(color_recipe)||bool(normal)!=bool(normal_recipe))throw std::invalid_argument("Source paint and its own prepared material authority differ");
  if(color){if(color->vertex_rgba!=color_recipe->projection->vertex_rgba||color->maximum_error!=color_recipe->projection->maximum_error||color->quality_cells!=color_recipe->projection->quality_cells)throw std::invalid_argument("Source PBR color field differs from its own retained receipt");if(deep)color_recipe->validate_current(paint->mesh(),appearance);}
  if(normal){if(!normal_projection_identity(*normal,*normal_recipe->projection))throw std::invalid_argument("Source normal planes differ from their own retained interpretation");if(deep)normal_recipe->validate_current(paint->mesh(),appearance);}
}
} // namespace dome::detail
