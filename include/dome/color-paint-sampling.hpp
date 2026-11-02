// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "color-preparation.hpp"
#include "paint-sampling.hpp"
namespace dome {
struct CornerPaintSamplingReceipt {
  uint32_t version=1,resolution=0;
  uint64_t triangles=0,tested_corners=0;
  std::array<float,4> maximum_corner_error{};
  void validate() const;
};
// An independently authenticated current CORNER field attached to the one
// retained writable grid. It never grants normal/role/source authority.
class SourceColorPaintSampling {
public:
  const CornerPaintSamplingReceipt &receipt() const noexcept{return receipt_;}
  const std::shared_ptr<const PaintSamplingDomain> &domain() const noexcept{return domain_;}
  const std::shared_ptr<const SourceColorUvLayout> &layout() const noexcept{return layout_;}
  const std::shared_ptr<const PolygonColorSurface> &current_descriptor() const noexcept{return current_;}
  const std::shared_ptr<const PolygonColorSurface> &sampling_descriptor() const noexcept{return sampling_;}
  const std::shared_ptr<const PaintedMesh> &before_paint() const noexcept{return before_paint_;}
  const std::shared_ptr<const SourceMaterialBindings> &before_material() const noexcept{return before_material_;}
  const std::shared_ptr<const DeformationState> &before_deformation() const noexcept{return before_deformation_;}
  const std::shared_ptr<const CreatorAppearance> &before_appearance() const noexcept{return before_appearance_;}
  void validate(bool deep=true,const std::function<bool()> & = {}) const;
  bool matches_current(const PaintedMesh &) const noexcept;
  void validate_current(const Mesh &,const std::function<bool()> & = {}) const;
private:
  CornerPaintSamplingReceipt receipt_;
  std::shared_ptr<const PaintSamplingDomain> domain_;
  std::shared_ptr<const SourceColorUvLayout> layout_;
  std::shared_ptr<const PolygonColorSurface> current_,sampling_;
  std::shared_ptr<const PaintedMesh> before_paint_;
  std::shared_ptr<const SourceMaterialBindings> before_material_;
  std::shared_ptr<const DeformationState> before_deformation_;
  std::shared_ptr<const CreatorAppearance> before_appearance_;
  std::shared_ptr<const PaintSamplingAttachment> attachment_;
  SourceColorPaintSampling()=default;
  friend struct ColorPaintSamplingAccess;
};
struct RetainedCornerPaint {
  std::shared_ptr<PaintedMesh> paint;
  std::shared_ptr<const SourceColorPaintSampling> sampling;
  std::shared_ptr<const SourceColorPreparation> colors;
};
// Detached source-current proof only. The caller must atomically compose this
// with independently adapted normal/material authority before Scene admission.
RetainedCornerPaint retain_source_corner_paint(
    std::shared_ptr<const PaintedMesh>,std::shared_ptr<const SourceMaterialBindings>,
    std::shared_ptr<const DeformationState>,std::shared_ptr<const PolygonColorSurface>,
    const Mesh &,std::span<const uint32_t>,const PaintSamplingLimits & = {});
} // namespace dome
