// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "dome/texture-sets.hpp"

namespace dome::detail {
bool bake_target_supported(const SculptObject &);
bool bake_source_supported(const SculptObject &);
bool texture_set_bake_present(const TextureSetView &) noexcept;
void clear_texture_set_bake(Scene &,const TextureSetLimits &);

void stage_texture_set_bake(
    SculptObject &target, const std::shared_ptr<const TextureSetCollection> &membership,
    const TextureSetView &recipient, std::shared_ptr<PaintedMesh> paint,
    std::shared_ptr<const ObjectBakeRecipe> recipe,const TextureSetLimits &,
    std::shared_ptr<const Image> native_normal = {});
} // namespace dome::detail
