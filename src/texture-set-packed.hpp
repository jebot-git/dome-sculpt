// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "texture-set-surface.hpp"
#include "dome/tangents.hpp"
namespace dome::detail {
class PolygonSurfacePreview;
// Immutable attribute layout retains actual source role packets when needed,
// independently of current Paint pixels/maps or the complete collection.
// Range proofs retain the separate current material/field plan.
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
  bool has_source()const noexcept{return !source_previews_.empty();}
  std::span<const Vec3>source_normals()const noexcept{return source_normals_;}
  std::span<const Tangent>source_tangents()const noexcept{return source_tangents_;}
  std::span<const std::array<float,2>>paint_coordinates()const noexcept{return paint_;}
  std::span<const std::array<float,2>>normal_coordinates()const noexcept{return normal_;}
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
  std::vector<Tangent>tangents_,source_tangents_;
  std::vector<Vec3>source_normals_;
  std::vector<std::array<float,2>>paint_,normal_;
  std::vector<std::shared_ptr<const PolygonSurfacePreview>>source_previews_;
};
std::shared_ptr<const TextureSetPackedGeometry> prepare_texture_set_packed(
    const std::shared_ptr<const TextureSetSurfacePlan>&,const TextureSetLimits & = {});
} // namespace dome::detail
