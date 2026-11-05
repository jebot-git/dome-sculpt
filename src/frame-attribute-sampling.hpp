// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "refined-role-sampling.hpp"
#include "dome/paint-sampling.hpp"
#include "dome/polygon-scene.hpp"
namespace dome::detail {
enum class SampledFrameRole { Pbr, MToon, Canonical };
// Owning attribute/geometry witness only. This does not authorize a writable
// domain, normal-field transition, Scene publication, codec or portable export.
// A later enclosing factory must independently prove each of those consumers.
class FrameAttributeSampling final {
public:
  static std::shared_ptr<const FrameAttributeSampling> capture(const Scene &,uint64_t,
      KeyMirrorPlane,float tolerance,const PaintSamplingLimits & = {});
  bool valid_owner(const std::shared_ptr<const FrameAttributeSampling> &) const noexcept;
  const auto &before() const noexcept{return before_;}
  const auto &refinement() const noexcept{return refinement_;}
  const auto &geometry() const noexcept{return geometry_;}
  const auto &deformation() const noexcept{return deformation_;}
  const auto &paint_coordinates() const noexcept{return paint_coordinates_;}
  const auto &normal_coordinates() const noexcept{return normal_coordinates_;}
  const RefinedRoleField *role(SampledFrameRole r) const noexcept{return roles_[size_t(r)].get();}
  const auto &role_coordinates(SampledFrameRole r) const noexcept{return role_coordinates_[size_t(r)];}
  size_t working_bytes() const noexcept{return working_bytes_;}
  void validate(const PaintSamplingLimits & = {}) const;
private:
  FrameAttributeSampling()=default;
  FrameAttributeSampling(const FrameAttributeSampling&)=delete;
  std::weak_ptr<const FrameAttributeSampling> self_;
  std::shared_ptr<const PolygonOrigin> before_;
  KeyMirrorRefinement refinement_;
  std::shared_ptr<const Mesh> geometry_;
  std::shared_ptr<const DeformationState> deformation_;
  std::array<std::shared_ptr<const RefinedRoleField>,3> roles_;
  std::array<std::vector<std::array<float,2>>,3> role_coordinates_;
  std::vector<std::array<float,2>> paint_coordinates_,normal_coordinates_;
  size_t working_bytes_=0;
  float tolerance_=0;
};
}
