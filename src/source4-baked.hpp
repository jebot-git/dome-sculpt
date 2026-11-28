// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/source-paint.hpp"
namespace dome {struct NormalFrames;}
namespace dome::detail {
// Asset-worker adoption onto an authenticated Source4 current atlas. The
// ordinary raw/Source4 install_bake guards grant no such authority.
std::shared_ptr<PaintedMesh> install_source_bake(const PaintedMesh &,BakedTextures,
    const SourcePaintOptions & = {});
// Rebuild the editable atlas from ordered rest corners while keeping the
// original material sampling and all independent Bake/paint channels.
std::shared_ptr<PaintedMesh> transfer_source_bake_uv(const PaintedMesh &,Mesh,
    std::span<const uint32_t>,const NormalFrames *old_canonical,const SourcePaintOptions & = {});
}
