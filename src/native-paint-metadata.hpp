// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native-paint-sampling.hpp"
namespace dome::detail {
// Only the private, factory/decoder-validated immutable owner can use this
// publication path. No geometry-value equivalence or mutable cross-call cache.
void native_paint_metadata(const std::shared_ptr<const NativePaintSampling> &);
bool native_frame_preview_metadata(const std::shared_ptr<const SourceMaterialBindings> &,
    const std::shared_ptr<const PaintSamplingAttachment> &);
} // namespace dome::detail
