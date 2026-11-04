// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "canonical-normal-conversion.hpp"
namespace dome::detail {
// Only the actual factory/replaying decoder can mint this immutable lease.
// DTO field assignment or matching bytes cannot grant conversion authority.
class CanonicalNormalConversion {
public:
  static std::shared_ptr<const CanonicalNormalConversion> create(
      const ColorPreparationInput &,const NormalPreparationOptions &);
  static std::shared_ptr<const CanonicalNormalConversion> restore(
      const nlohmann::json &,const ColorPreparationInput &,
      std::shared_ptr<const NormalFrameSet>,std::shared_ptr<const PaintNormalProjection>);
  bool valid_owner(const std::shared_ptr<const CanonicalNormalConversion> &p) const noexcept {
    return p.get()==this&&!owner_.expired()&&!owner_.owner_before(p)&&!p.owner_before(owner_);
  }
  const auto &before_paint() const noexcept{return paint_;}
  const auto &before_material() const noexcept{return binding_;}
  const auto &before_deformation() const noexcept{return deformation_;}
  const auto &frames() const noexcept{return frames_;}
  const auto &projection() const noexcept{return projection_;}
  NormalPreparationReceipt receipt() const;
  nlohmann::json encode() const;
  void validate(bool deep=true) const;
private:
  CanonicalNormalConversion()=default;
  CanonicalNormalConversion(const CanonicalNormalConversion&)=delete;
  CanonicalNormalConversion &operator=(const CanonicalNormalConversion&)=delete;
  std::weak_ptr<const CanonicalNormalConversion> owner_;
  std::shared_ptr<const PaintedMesh> paint_;
  std::shared_ptr<const DeformationState> deformation_;
  std::shared_ptr<const SourceMaterialBindings> binding_;
  std::shared_ptr<const NormalFrameSet> frames_;
  std::shared_ptr<const PaintNormalProjection> projection_;
  Material material_;
  double accepted_=0,observed_=0;
  uint64_t samples_=0,raster_=0,max_samples_=0;
  size_t max_bytes_=0;
};
} // namespace dome::detail
