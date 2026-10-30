// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/normal-preparation.hpp"
namespace dome {
struct PaintNormalPreparationResult {
  std::shared_ptr<PaintedMesh> paint;
  NormalPreparationReceipt receipt;
};
struct SourceNormalPreparationAccess {
  static PaintNormalPreparationResult prepare(const PaintedMesh &,
      const SourceMaterialBindings *,const NormalPreparationOptions &);
};
} // namespace dome
