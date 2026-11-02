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
  uint64_t sampled_texels=0,source_constraints=0;
};
struct SourceNormalPreparationAccess {
  static PaintNormalUvProjectionResult reproject(const PaintedMesh &,const Mesh &,
      std::span<const uint32_t>,const NormalPreparationOptions &);
  static PaintNormalPreparationResult prepare(const PaintedMesh &,
      const SourceMaterialBindings *,const NormalPreparationOptions &);
};
} // namespace dome
