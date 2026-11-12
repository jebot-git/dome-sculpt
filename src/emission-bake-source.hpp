// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
namespace dome::detail {
// Private immutable raw source/overlay view for asset-worker Bake. Empty keeps
// the legacy UV0 dense-composite route unchanged.
std::shared_ptr<const TextureMaps> raw_bake_source(const SculptObject &,
    const std::function<bool()> &cancelled={});
// Actual immutable source radiance evaluated at the source projection hit.
// Empty means the unchanged legacy Bake path is sufficient. No RGB8 composite
// is accepted as physical radiance authority.
std::function<std::array<float,3>(uint32_t,std::array<float,3>)>
emission_bake_source(const SculptObject &,const std::function<bool()> &cancelled={});
}
