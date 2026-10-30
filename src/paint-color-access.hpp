// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/color-preparation.hpp"
#include <span>
namespace dome {
struct PaintColorPreparationResult {
  std::shared_ptr<PaintedMesh> paint;
  ColorPreparationReceipt receipt;
};
struct PaintColorProjectionResult {
  std::shared_ptr<const PaintColorProjection> projection;
  uint64_t projected_texels=0;
};
struct SourceColorPreparationAccess {
  // Rebuild only the static raw color field in an adopted writable UV domain.
  // Existing resolved PBR layer and baked pixels must not be multiplied again.
  static PaintColorProjectionResult project(
      const PaintedMesh &,std::span<const std::array<float,4>>,
      const ColorPreparationOptions &);
  static PaintColorPreparationResult prepare(
      const PaintedMesh &,std::span<const std::array<float,4>>,
      const Material &,const ColorPreparationOptions &,
      SourceColorField=SourceColorField::OriginalAccessor,
      std::shared_ptr<const PaintColorProjection> retained_projection={});
};
} // namespace dome
