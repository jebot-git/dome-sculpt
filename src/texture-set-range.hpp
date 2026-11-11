// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "texture-set-packed.hpp"
namespace dome {struct SurfaceRange;}
namespace dome::detail {
// Runtime-only owner: the worker computes the actual field maps from a sealed
// set plan. Neither a label nor a numeric set ID can grant this interpretation.
class TextureSetRangeProof {
public:
  const std::shared_ptr<const TextureSetSurfacePlan>&plan()const noexcept{return plan_;}
  const std::shared_ptr<const TextureSetPackedGeometry>&geometry_layout()const noexcept{return layout_;}
  size_t first_index()const noexcept{return first_;}
  size_t first_vertex()const noexcept{return vertex_;}
  void authenticate_stream(const Mesh&,const SurfaceRange&)const;
  void authenticate_tint(const Mesh&,const SurfaceRange&,std::span<const std::array<float,4>>)const;
  const std::shared_ptr<const TextureMaps>&textures()const noexcept{return maps_;}
  const TextureSetView&field()const{return plan_->fields()[field_];}
  const TextureSetDraw&draw()const{return plan_->draws()[field_];}
  uint32_t logical_corner(size_t packed_corner,bool reversed)const;
  void authenticate(const SurfaceRange&)const;
private:
  friend std::shared_ptr<const TextureSetRangeProof> prepare_texture_set_range(
      std::shared_ptr<const TextureSetSurfacePlan>,std::shared_ptr<const TextureSetPackedGeometry>,
      size_t,size_t,size_t,const std::function<bool()>&,bool);
  TextureSetRangeProof()=default;
  TextureSetRangeProof(TextureSetRangeProof&&)=default;
  mutable std::weak_ptr<const TextureSetRangeProof>self_;
  std::shared_ptr<const TextureSetSurfacePlan>plan_;
  std::shared_ptr<const TextureSetPackedGeometry>layout_;
  size_t field_=0,first_=0,vertex_=0;
  std::shared_ptr<const TextureMaps>maps_;
};
std::shared_ptr<const TextureSetRangeProof> prepare_texture_set_range(
    std::shared_ptr<const TextureSetSurfacePlan>,std::shared_ptr<const TextureSetPackedGeometry>,
    size_t field,size_t object_first_index,size_t object_first_vertex,
    const std::function<bool()> &cancelled = {},bool render_only=false);
} // namespace dome::detail
