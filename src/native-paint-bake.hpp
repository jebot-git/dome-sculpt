// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "dome/paint-sampling.hpp"

namespace dome::detail {
// A temporary indexed corner mesh in the writable atlas. The evaluated
// positions and normals can differ from the sealed REST attachment, but
// callers must preserve its ordered corner identities.
Mesh native_paint_bake_mesh(const Mesh &,const PaintSamplingDomain &,
    const std::function<bool()> &cancelled);
}
