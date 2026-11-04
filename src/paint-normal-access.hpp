// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/normal-preparation.hpp"
namespace dome {
struct PaintNormalPreparationResult {
  std::shared_ptr<PaintedMesh> paint;
  NormalPreparationReceipt receipt;
};
struct PaintNormalUvProjectionResult {
  std::shared_ptr<const PaintNormalProjection> projection;
  double observed_sample_error=0;
  uint64_t sampled_texels=0,source_constraints=0,raster_samples=0;
};
struct SourceNormalPreparationAccess {
  // Resolve the historical per-texel normalize/quantize-before-filter model.
  // This does not change any source image or grant a new sampling domain.
  static std::shared_ptr<const PaintNormalProjection> resolved(
      const PaintedMesh &,const std::function<bool()> &cancelled = {});
  static PaintNormalUvProjectionResult reproject(const PaintedMesh &,const Mesh &,
      std::span<const uint32_t>,const NormalPreparationOptions &);
  static PaintNormalPreparationResult prepare(const PaintedMesh &,
      const SourceMaterialBindings *,const NormalPreparationOptions &);
  static std::shared_ptr<PaintedMesh> adopt_canonical(const PaintedMesh &,
      std::shared_ptr<const PaintNormalProjection>,const std::function<bool()> & = {});
};
} // namespace dome
