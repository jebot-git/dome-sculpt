// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/cut-authoring.hpp"
namespace dome::detail {
struct CutColorLimits {
  size_t maximum_work = 64'000'000, maximum_vertices = 500'000,
         maximum_faces = 500'000, maximum_corners = 2'000'000,
         maximum_provenance = 128;
  uint64_t maximum_raster_samples = 128'000'000;
};
// Cheap admission only. The detached publisher authenticates full geometry,
// current field, material/paint and clipping provenance before committing.
bool cut_color_supported(const SculptObject &) noexcept;
void require_cut_color_supported(const SculptObject &);
CutPublishReport publish_colored_cut(Scene &, const CutPreview &,
                                     const BakeSource &, uint32_t,
                                     const std::function<bool()> &,
                                     const std::function<void(float)> &,
                                     const CutColorLimits & = {});
} // namespace dome::detail
