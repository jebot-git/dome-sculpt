// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "texture-set-surface.hpp"
#include "dome/tangents.hpp"
namespace dome::detail {
// Geometry-only packed layout: it never retains Paint, TextureMaps or the full
// collection. Range proofs retain the separate current material/field plan.
class TextureSetPackedGeometry {
public:
  void authenticate_owner(const std::shared_ptr<const TextureSetPackedGeometry>&)const;
  bool matches_plan(const TextureSetSurfacePlan&)const;
  void authenticate_plan(const TextureSetSurfacePlan&)const;
  size_t owned_bytes()const noexcept{return bytes_;}
  size_t preparation_work()const noexcept{return work_;}
  const std::shared_ptr<const Mesh>&posed()const noexcept{return posed_;}
  std::span<const uint32_t>vertices()const noexcept{return vertices_;}
  std::span<const uint32_t>indices()const noexcept{return indices_;}
  std::span<const uint32_t>logical_corners()const noexcept{return corners_;}
  std::span<const Tangent>tangents()const noexcept{return tangents_;}
  bool canonical(size_t field)const{return canonical_.at(field);}
private:
  friend std::shared_ptr<const TextureSetPackedGeometry> prepare_texture_set_packed(
      const std::shared_ptr<const TextureSetSurfacePlan>&,const TextureSetLimits&);
  TextureSetPackedGeometry()=default;
  TextureSetPackedGeometry(TextureSetPackedGeometry&&)=default;
  mutable std::weak_ptr<const TextureSetPackedGeometry>self_;
  uint64_t object_=0;
  size_t bytes_=0,work_=0;
  std::shared_ptr<const Mesh>rest_,posed_;
  std::shared_ptr<const DeformationState>deformation_;
  std::shared_ptr<const TextureSetFaceDomain>membership_;
  std::vector<TextureSetDraw>draws_;
  std::vector<uint8_t>canonical_;
  std::vector<uint32_t>vertices_,indices_,corners_;
  std::vector<Tangent>tangents_;
};
std::shared_ptr<const TextureSetPackedGeometry> prepare_texture_set_packed(
    const std::shared_ptr<const TextureSetSurfacePlan>&,const TextureSetLimits & = {});
} // namespace dome::detail
