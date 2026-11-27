// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "dome/texture-sets.hpp"

namespace dome::detail {
bool native_hdr_grid(const PaintedMesh &) noexcept;
bool retained_hdr_grid(const TextureSetView &) noexcept;
size_t retained_hdr_normal_bytes(const TextureSetView &) noexcept;
size_t native_hdr_grid_bytes(const PaintedMesh &,bool keep_retained_normal=false) noexcept;
void charge_native_hdr_grid_work(const PaintedMesh &,size_t &,const TextureSetLimits &,
    bool keep_retained_normal=false);
std::shared_ptr<TextureMaps> compile_native_hdr_grid(const PaintedMesh &,const Material &,
    size_t &,const TextureSetLimits &,bool keep_retained_normal=false);
} // namespace dome::detail
