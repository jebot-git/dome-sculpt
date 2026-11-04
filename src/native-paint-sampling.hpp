// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "dome/normal-preparation.hpp"
#include "dome/paint-sampling.hpp"
namespace dome::detail {
// This owns a native grid correspondence, never a colour or normal receipt.
class NativePaintSampling;
struct RetainedNativePaint {
  std::shared_ptr<PaintedMesh> paint;
  std::shared_ptr<const NativePaintSampling> owner;
};
class NativePaintSampling final {
  std::shared_ptr<const PaintedMesh> before_;
  Material native_;
  std::shared_ptr<const SourceMaterialBindings> before_material_;
  uint32_t depth_=1;
  std::weak_ptr<const NativePaintSampling> self_;
  std::shared_ptr<const SourceNormalUvLayout> layout_;
  std::shared_ptr<const PaintSamplingDomain> domain_;
  std::shared_ptr<const PaintSamplingAttachment> attachment_;
  NativePaintSampling()=default;
  friend struct NativePaintSamplingAccess;
  friend RetainedNativePaint retain_native_paint(const SculptObject &,const Mesh &,
      std::span<const uint32_t>,const PaintSamplingLimits &);
public:
  const auto &before_paint() const noexcept{return before_;}
  const auto &before_material() const noexcept{return before_material_;}
  uint32_t history_depth() const noexcept{return depth_;}
  const auto &before_native_material() const noexcept{return native_;}
  const auto &layout() const noexcept{return layout_;}
  const auto &domain() const noexcept{return domain_;}
  const auto &attachment() const noexcept{return attachment_;}
  const std::shared_ptr<const PaintedMesh> &original_paint() const noexcept;
  const Material &original_native_material() const noexcept;
  bool valid_owner(const std::shared_ptr<const NativePaintSampling> &) const noexcept;
  bool matches_current(const PaintedMesh &) const noexcept;
  void validate_mesh(const Mesh &,const PaintSamplingLimits & = {}) const;
  void validate(const PaintSamplingLimits & = {}) const;
  void validate_current(const PaintedMesh &,const PaintSamplingLimits & = {}) const;
};
RetainedNativePaint retain_native_paint(const SculptObject &,const Mesh &,
    std::span<const uint32_t>,const PaintSamplingLimits & = {});
std::shared_ptr<const SourceMaterialBindings> bind_native_paint(
    const std::shared_ptr<const NativePaintSampling> &,std::shared_ptr<const SourceMaterialBindings>);
bool native_paint_retention_supported(const SculptObject &) noexcept;
std::shared_ptr<const PaintSamplingAttachment> material_paint_attachment(const SourceMaterialBindings *);
class NativePaintSamplingCodec {
  friend class dome::SourceMaterialArchive;
  static RetainedNativePaint replay(std::shared_ptr<const PaintedMesh>,Material,
      std::shared_ptr<const SourceMaterialBindings>,std::shared_ptr<const SourceNormalUvLayout>,
      std::shared_ptr<const PaintSamplingDomain>,const PaintSamplingLimits & = {});
};
nlohmann::json encode_native_material(const Material &);
Material decode_native_material(const nlohmann::json &);
} // namespace dome::detail
