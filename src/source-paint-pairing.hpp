// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/normal-preparation.hpp"
#include "dome/color-paint-sampling.hpp"
#include "normal-projection-identity.hpp"
#include "color-bound-memo.hpp"
#include "native-paint-sampling.hpp"
namespace dome::detail {
// Call after holder validation. A retained paint needs its OWN interpretation;
// a current object or a neighboring Origin cannot authenticate that before-state.
inline void source_paint_pairing(const PaintedMesh *paint,const SourceMaterialBindings *binding,
    const CreatorAppearance *appearance,bool deep){
  const auto color=paint?paint->color_projection():nullptr;
  const auto normal=paint?paint->normal_projection():nullptr;
  const auto color_recipe=binding?binding->color_preparation:nullptr;
  const auto normal_recipe=binding?binding->normal_preparation:nullptr;
  if(paint&&paint->baked()&&paint->baked()->normal_authority&&
     (!normal||normal->version!=2||!normal_recipe||!normal_recipe->sampling_domain))
    throw std::invalid_argument("Source normal detail identity requires its own complete field replay");
  if(bool(color)!=bool(color_recipe)||bool(normal)!=bool(normal_recipe))throw std::invalid_argument("Source paint and its own prepared material authority differ");
  if(paint&&bool(paint->paint_sampling_domain())!=bool((color_recipe&&color_recipe->paint_sampling)||(binding&&binding->native_paint_sampling)))
    throw std::invalid_argument("Retained paint domain requires its own current-field material authority");
  if(color_recipe&&color_recipe->paint_sampling&&!color_recipe->paint_sampling->matches_current(*paint))
    throw std::invalid_argument("Retained paint and material lack the exact same geometry attachment");
  if(binding&&binding->native_paint_sampling){if(!paint)throw std::invalid_argument("Native paint owner requires its current Paint");if(deep)binding->native_paint_sampling->validate_current(*paint);else if(!binding->native_paint_sampling->matches_current(*paint))throw std::invalid_argument("Native paint lacks its own exact geometry attachment");}
  if(color){if(color->vertex_rgba!=color_recipe->projection->vertex_rgba||color->maximum_error!=color_recipe->projection->maximum_error||color->quality_cells!=color_recipe->projection->quality_cells)throw std::invalid_argument("Source PBR color field differs from its own retained receipt");if(deep){ColorBoundPaintScope context(*paint,color_recipe.get());color_recipe->validate_current(paint->mesh(),appearance);}}
  if(normal){if(!normal_projection_identity(*normal,*normal_recipe->projection))throw std::invalid_argument("Source normal planes differ from their own retained interpretation");if(normal_recipe->sampling_domain)normal_recipe->sampling_domain->validate_paint(*paint,*binding,deep);if(deep)normal_recipe->validate_current(paint->mesh(),appearance);}
}
} // namespace dome::detail
