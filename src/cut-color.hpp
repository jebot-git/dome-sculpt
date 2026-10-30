// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/cut-authoring.hpp"
namespace dome::detail {
// Cheap admission only. The detached publisher authenticates full geometry,
// current field, material/paint and clipping provenance before committing.
bool cut_color_supported(const SculptObject &) noexcept;
void require_cut_color_supported(const SculptObject &);
CutPublishReport publish_colored_cut(Scene &, const CutPreview &,
                                     const BakeSource &, uint32_t,
                                     const std::function<bool()> &,
                                     const std::function<void(float)> &);
} // namespace dome::detail
