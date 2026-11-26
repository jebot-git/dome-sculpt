// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "native-paint-sampling.hpp"

namespace dome::detail {
std::shared_ptr<PaintedMesh> retain_native_baked_grid(const PaintedMesh &,const Mesh &,
    std::shared_ptr<const PaintSamplingDomain>,const std::shared_ptr<const NativePaintSampling> &,
    const PaintSamplingLimits &);
std::shared_ptr<PaintedMesh> retain_native_baked_grid(const PaintedMesh &,Mesh &&,
    std::shared_ptr<const PaintSamplingDomain>,const std::shared_ptr<const NativePaintSampling> &,
    const PaintSamplingLimits &);
}
