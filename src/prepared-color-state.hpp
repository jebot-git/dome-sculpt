// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/color-preparation.hpp"
namespace dome::detail {
inline bool prepared_color_state(const SourceMaterialBindings *binding,
                                 const TextureMaps *maps,bool deep=true) {
  const bool prepared=binding && bool(binding->color_preparation);
  if(prepared!=bool(maps && maps->color_projection))
    throw std::invalid_argument("Prepared color pixels and their retained provenance differ");
  if(!prepared) {
    if(maps && (maps->pbr_color.width || maps->pbr_color.height || !maps->pbr_color.rgba.empty()))
      throw std::invalid_argument("Resolved PBR colors require an explicit color preparation");
    return false;
  }
  const auto &receipt=*binding->color_preparation;receipt.validate(deep);
  if(maps->source_only || (maps->color_projection->vertex_rgba!=receipt.projection->vertex_rgba || maps->color_projection->maximum_error!=receipt.projection->maximum_error || maps->color_projection->quality_cells!=receipt.projection->quality_cells))
    throw std::invalid_argument("Current PBR color projection differs from its preparation receipt");
  maps->pbr_color.validate();
  if(maps->pbr_color.width!=maps->color.width || maps->pbr_color.height!=maps->color.height)
    throw std::invalid_argument("Prepared PBR and common color grids differ");
  return true;
}
} // namespace dome::detail
