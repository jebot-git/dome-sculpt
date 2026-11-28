// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/paint.hpp"
namespace dome {struct SourceMaterialBindings;}
namespace dome::detail {
namespace emission_raster {struct PhysicalTile;}
struct EmissionExportAccess {
 // Borrow the sealed, immutable paint grid for an offline per-texel export.
 static std::span<const int32_t> writable_padding(const PaintedMesh &);
 // Actual original ordinal coordinates in the current vertex domain.
 static std::shared_ptr<const std::vector<std::array<float,2>>> prepared_role_uv(const PaintedMesh &,int);
 struct WritableSample { uint32_t face; std::array<float,3> bary; };
 static WritableSample writable_sample(const PaintedMesh &,size_t texel);
 // Interpolate the original UV at the paint texel's sealed triangle sample.
 // Padding borrows the same source coordinate and paint as its owning texel.
 static std::array<float,2> writable_source_uv(const PaintedMesh &,
     std::span<const std::array<float,2>>,size_t texel);
 static emission_raster::PhysicalTile emission_tile(const PaintEmissionTile &);
 static bool requires_preview(const PaintedMesh &,const Material &,const SourceMaterialBindings *);
 static std::shared_ptr<const TextureMaps> preview(const PaintedMesh &,const Material &,const std::function<bool()> &);
 static std::shared_ptr<const TextureMaps> preview_texture_set_bake(const PaintedMesh &,const Material &,
     const std::function<bool()> &);
 static std::shared_ptr<PaintedMesh> without_emission(const PaintedMesh &);
 static std::shared_ptr<const TextureMaps> other_channels(const PaintedMesh &,Material,const std::function<bool()> &);
};
}
