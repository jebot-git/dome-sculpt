// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "dome/texture-sets.hpp"

namespace dome::paint_compute {class MaterialAtlas;}
namespace dome::detail {
enum class TextureSetGridMode { NativeHdr, NativeLdr, RetainedHdr, RetainedLdr, NativeMToonHdr };

bool native_hdr_grid(const PaintedMesh &) noexcept;
bool retained_material_grid(const TextureSetView &) noexcept;
bool retained_hdr_grid(const TextureSetView &) noexcept;
TextureSetGridMode retained_grid_mode(const TextureSetView &) noexcept;
size_t retained_hdr_normal_bytes(const TextureSetView &) noexcept;
size_t native_material_grid_bytes(const PaintedMesh &,TextureSetGridMode) noexcept;
size_t native_material_grid_scratch_bytes(const PaintedMesh &,TextureSetGridMode) noexcept;
void charge_native_material_grid_work(const PaintedMesh &,size_t &,const TextureSetLimits &,TextureSetGridMode);
std::shared_ptr<TextureMaps> compile_native_material_grid(const PaintedMesh &,const Material &,
    size_t &,const TextureSetLimits &,TextureSetGridMode);
std::shared_ptr<const paint_compute::MaterialAtlas> compile_resident_bake_grid(const PaintedMesh &,const Material &,
    size_t &,const TextureSetLimits &,TextureSetGridMode);
} // namespace dome::detail
