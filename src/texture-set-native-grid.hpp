// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "dome/texture-sets.hpp"

namespace dome::detail {
bool native_hdr_grid(const PaintedMesh &) noexcept;
size_t native_hdr_grid_bytes(const PaintedMesh &) noexcept;
void charge_native_hdr_grid_work(const PaintedMesh &,size_t &,const TextureSetLimits &);
std::shared_ptr<TextureMaps> compile_native_hdr_grid(const PaintedMesh &,const Material &,
    size_t &,const TextureSetLimits &);
} // namespace dome::detail
