// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"

namespace dome::detail {
// The avatar and legacy surface compilers admit constant factor rewrites.
// The Standard GLB file worker can also emit a proved writable atlas.
void source_paint_export(const PaintedMesh &,ObjectSurface &,const std::function<bool()> &cancelled={},
                         bool allow_paint_atlas=false);
std::vector<ObjectSurface> prepare_standard_glb_surfaces(const Scene &,const std::function<bool()> &);
}
