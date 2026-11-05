// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
namespace dome::detail {
struct CanonicalPaintArchiveUsage {
 size_t deformation_entries=0;
 size_t deformation_slots=0;
};
// Count actual serialized model slots, not value-based pointer deduplication.
// The new current model and one complete before-origin need not yet exist.
CanonicalPaintArchiveUsage preflight_canonical_paint_models(const Scene &,uint64_t,
 size_t prospective_current_entries,const std::function<bool()> &cancelled={});
// Dimensions of an exact current-vertex remap, without allocating target arrays.
size_t canonical_paint_model_entries(const DeformationState *,const Mesh &,const Mesh &,
 std::span<const uint32_t>,const std::function<bool()> &cancelled={});
}
