// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"

namespace dome::detail {
enum class SourcePaintExport { Factors, Standard, StandardHdr };
// StandardHdr compiles the non-emission channels. The HDR field supplies its
// own portable emissive image and binding after core material compilation.
void source_paint_export(const PaintedMesh &,ObjectSurface &,
    const std::function<bool()> &cancelled={},SourcePaintExport mode=SourcePaintExport::Factors);
std::vector<ObjectSurface> prepare_standard_glb_surfaces(const Scene &,const std::function<bool()> &);
}
