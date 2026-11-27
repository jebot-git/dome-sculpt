// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/rig.hpp"

namespace dome::detail {

enum class MorphNormalBasis { Geometry, PreserveRestForUnchanged };

Mesh morph_target_surface(const Mesh &rest, const MorphSet &morphs, size_t target,
                         MorphNormalBasis basis,
                         const std::function<bool()> &cancelled);

} // namespace dome::detail
