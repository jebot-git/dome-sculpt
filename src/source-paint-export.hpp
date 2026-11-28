// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "dome/source-maps.hpp"

namespace dome::detail {
enum class SourcePaintExport { Factors, Standard, StandardHdr };
// StandardHdr compiles the non-emission channels. The HDR field supplies its
// own portable emissive image and binding after core material compilation.
void source_paint_export(const PaintedMesh &,ObjectSurface &,
    const std::function<bool()> &cancelled={},SourcePaintExport mode=SourcePaintExport::Factors);
PaintSourceTexture source_baked_hdr_export(const SculptObject &,ObjectSurface &,const std::function<bool()> &);
std::vector<ObjectSurface> prepare_standard_glb_surfaces(const Scene &,const std::function<bool()> &);
}
