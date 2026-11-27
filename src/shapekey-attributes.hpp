// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/normal-frames.hpp"
#include "morph-normal-basis.hpp"
#include <span>
namespace dome::detail {
NormalFrameTarget author_key_attributes(const Mesh &,const NormalFrames &,
    const NormalFrameTarget *,std::span<const Vec3>,std::span<const Vec3>,uint64_t,
    const std::function<bool()> &,MorphNormalBasis = MorphNormalBasis::Geometry);
} // namespace dome::detail
