// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "texture-set-field-source.hpp"
#include "dome/tangents.hpp"
namespace dome::detail {
struct PortableTextureSetCorner {
 Vec3 normal;
 Tangent tangent;
 std::array<float,2>paint,role;
};
struct PortableTextureSetField {
 std::vector<PortableTextureSetCorner>corners; // Actual domain triangle order.
 std::shared_ptr<const TextureMaps>maps;
 bool normal_role=false;
};
// Compiles a genuine retained field only after proving that one constant
// before role frame on each triangle has a standard glTF representation.
// Smooth N-only fields also admit an exactly conformal current world matrix.
// Current geometry, hierarchy and UV0 stay untouched; independent paint/role
// coordinates remain separate arrays. No texture/normal sampling approximation.
PortableTextureSetField portable_texture_set_field(const TextureSetView&,
 const AffineTransform&,const TextureSetLimits& = {});
}
