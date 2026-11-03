// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/shapekey-authoring.hpp"
#include "dome/normal-preparation.hpp"
#include "frame-position-domain.hpp"
namespace dome::detail {
struct DerivedKeyTarget {
  uint64_t id=0;
  // Original stable target in the owning anchor; null means a newly authored
  // zero endpoint. These reference native attribute authority, not raw IDs.
  std::optional<uint64_t> reference;
};
class SourceKeyDerivation {
public:
  ~SourceKeyDerivation()=default;
  bool valid_owner(const std::shared_ptr<const SourceKeyDerivation> &p) const noexcept{return p.get()==this&&!owner_.expired()&&!owner_.owner_before(p)&&!p.owner_before(owner_);}
  const auto &before_paint() const noexcept{return paint_;}
  const auto &before_material() const noexcept{return material_;}
  const auto &before_deformation() const noexcept{return deformation_;}
  const auto &before_appearance() const noexcept{return appearance_;}
  const auto &position_domain() const noexcept{return domain_;}
  const auto &frames() const noexcept{return frames_;}
  const auto &targets() const noexcept{return targets_;}
  const std::vector<Vec3> &positions(uint64_t id) const;
  void validate(bool deep=true,const std::function<bool()> &cancelled={}) const;
private:
  SourceKeyDerivation()=default;
  SourceKeyDerivation(const SourceKeyDerivation&)=delete;
  SourceKeyDerivation &operator=(const SourceKeyDerivation&)=delete;
  std::weak_ptr<const SourceKeyDerivation> owner_;
  std::shared_ptr<const PaintedMesh> paint_;
  std::shared_ptr<const SourceMaterialBindings> material_;
  std::shared_ptr<const DeformationState> deformation_;
  std::shared_ptr<const CreatorAppearance> appearance_;
  std::shared_ptr<const FramePositionDomain> domain_;
  std::shared_ptr<const NormalFrameSet> frames_;
  std::vector<DerivedKeyTarget> targets_;
  std::vector<uint64_t> current_targets_;
  uint64_t next_target_=1;
  std::shared_ptr<const NormalFrameSet> replay(const std::function<bool()> &) const;
  friend ShapeKeyResult author_derived_normal_shapekey(Scene &,uint64_t,const ShapeKeyEdit &,const std::function<bool()> &);
};
bool needs_independent_key_domain(const SculptObject &,const ShapeKeyEdit &);
ShapeKeyResult author_derived_normal_shapekey(Scene &,uint64_t,const ShapeKeyEdit &,
    const std::function<bool()> &cancelled={});
bool key_recipe_sampling_matches(const SourceNormalPreparation &,const SourceNormalPreparation &);
} // namespace dome::detail
