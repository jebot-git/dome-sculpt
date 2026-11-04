// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/paint-sampling.hpp"
namespace dome::detail {
// Detached prototype only. No material/Scene codec may admit this object yet.
// This owns a native grid correspondence, never a colour or normal receipt.
class NativePaintSampling;
struct RetainedNativePaint {
  std::shared_ptr<PaintedMesh> paint;
  std::shared_ptr<const NativePaintSampling> owner;
};
class NativePaintSampling final {
  std::shared_ptr<const PaintedMesh> before_;
  Material native_;
  std::shared_ptr<const SourceNormalUvLayout> layout_;
  std::shared_ptr<const PaintSamplingDomain> domain_;
  std::shared_ptr<const PaintSamplingAttachment> attachment_;
  NativePaintSampling()=default;
  friend RetainedNativePaint retain_native_paint(const SculptObject &,Mesh,
      std::span<const uint32_t>,const PaintSamplingLimits &);
public:
  const auto &before_paint() const noexcept{return before_;}
  const auto &before_native_material() const noexcept{return native_;}
  const auto &layout() const noexcept{return layout_;}
  const auto &domain() const noexcept{return domain_;}
  const auto &attachment() const noexcept{return attachment_;}
  void validate(const PaintSamplingLimits & = {}) const;
  void validate_current(const PaintedMesh &,const PaintSamplingLimits & = {}) const;
};
RetainedNativePaint retain_native_paint(const SculptObject &,Mesh,
    std::span<const uint32_t>,const PaintSamplingLimits & = {});
} // namespace dome::detail
