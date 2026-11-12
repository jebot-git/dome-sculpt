// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/source-paint.hpp"
namespace dome::detail {
// Asset-worker adoption onto an authenticated Source4 current atlas. The
// ordinary raw/Source4 install_bake guards grant no such authority.
std::shared_ptr<PaintedMesh> install_source_bake(const PaintedMesh &,BakedTextures,
    const SourcePaintOptions & = {});
}
