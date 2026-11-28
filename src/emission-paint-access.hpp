// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/paint.hpp"
namespace dome {struct SourceMaterialBindings;}
namespace dome::detail {
namespace emission_raster {struct PhysicalTile;}
struct EmissionExportAccess {
 // Borrow the sealed, immutable paint grid for an offline per-texel export.
 static std::span<const int32_t> writable_padding(const PaintedMesh &);
 static emission_raster::PhysicalTile emission_tile(const PaintEmissionTile &);
 static bool requires_preview(const PaintedMesh &,const Material &,const SourceMaterialBindings *);
 static std::shared_ptr<const TextureMaps> preview(const PaintedMesh &,const Material &,const std::function<bool()> &);
 static std::shared_ptr<const TextureMaps> preview_texture_set_bake(const PaintedMesh &,const Material &,
     const std::function<bool()> &);
 static std::shared_ptr<PaintedMesh> without_emission(const PaintedMesh &);
 static std::shared_ptr<const TextureMaps> other_channels(const PaintedMesh &,Material,const std::function<bool()> &);
};
}
