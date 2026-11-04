// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/normal-preparation.hpp"
namespace dome::detail {
// Detached conversion candidate, not a normal-preparation authority. A future
// owning operation must retain/replay these original owners and explicitly
// adopt the new attribute/fragment contract before enabling it in a document.
// In particular this never relabels historical canonical pixels as prepared.
struct CanonicalNormalConversionCandidate {
  std::shared_ptr<const PaintedMesh> original_paint;
  std::shared_ptr<const DeformationState> original_deformation;
  std::shared_ptr<const NormalFrameSet> target_frames;
  std::shared_ptr<const PaintNormalProjection> projection;
  uint64_t sampled_centers=0,raster_samples=0;
  double maximum_center_angular_error=0;
};
CanonicalNormalConversionCandidate convert_native_canonical_normals(
    const ColorPreparationInput &,const NormalPreparationOptions & = {});
} // namespace dome::detail
