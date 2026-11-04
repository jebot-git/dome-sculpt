// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/normal-preparation.hpp"
#include "dome/color-paint-sampling.hpp"
#include "normal-projection-identity.hpp"
#include "baked-normal-field.hpp"
#include "canonical-normal-uv.hpp"
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
  if(binding&&binding->canonical_uv){
    if(!paint||appearance||binding->canonical_uv->origin()!=binding->canonical_attributes)
      throw std::invalid_argument("Retained canonical UV paint lost its original native authority");
    if(deep)binding->canonical_uv->validate_current(paint->mesh());
  }
  if(paint&&paint->baked()&&paint->baked()->normal_field){
    const auto &field=paint->baked()->normal_field;field->validate();
    if(!binding||!binding->canonical_attributes||field->origin()!=binding->canonical_attributes||normal||normal_recipe)
      throw std::invalid_argument("Baked normal field lost its actual separate attribute origin");
    if(deep&&!binding->canonical_uv){
      const auto &before=field->origin()->before_paint()->mesh(),&current=paint->mesh();
      if(current.indices.size()!=before.indices.size())throw std::invalid_argument("Independent baked normals require their actual ordered surface domain");
      for(size_t c=0;c<current.indices.size();++c){
        const auto a=current.indices[c],b=before.indices[c];
        const auto x=current.positions.at(a),y=before.positions.at(b);
        if(current.uv.at(a)!=before.uv.at(b)||x.x!=y.x||x.y!=y.y||x.z!=y.z)
          throw std::invalid_argument("Independent baked normal geometry/UV edits require an owning sampling-domain adapter");
      }
    }
  }
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
