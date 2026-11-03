// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "normal-key-derivation.hpp"
namespace dome::detail {
// Owning decoder/worker only. Shares an authenticated unchanged immutable
// raster geometry; current layers, source maps, detail/event and caches remain
// current. Later genuinely different UV/domain geometry is never relabelled.
std::shared_ptr<PaintedMesh> adopt_key_paint_geometry(const PaintedMesh &,
    const SourceMaterialBindings &,const CreatorAppearance *,
    const DeformationState *,const std::function<bool()> &cancelled={});
}
