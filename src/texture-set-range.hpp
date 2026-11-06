// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "texture-set-surface.hpp"
namespace dome {struct SurfaceRange;}
namespace dome::detail {
// Runtime-only owner: the worker computes the actual field maps from a sealed
// set plan. Neither a label nor a numeric set ID can grant this interpretation.
class TextureSetRangeProof {
public:
  const std::shared_ptr<const TextureSetSurfacePlan>&plan()const noexcept{return plan_;}
  const std::shared_ptr<const TextureMaps>&textures()const noexcept{return maps_;}
  const TextureSetView&field()const{return plan_->fields()[field_];}
  const TextureSetDraw&draw()const{return plan_->draws()[field_];}
  uint32_t logical_corner(size_t packed_corner,bool reversed)const;
  void authenticate(const SurfaceRange&)const;
private:
  friend std::shared_ptr<const TextureSetRangeProof> prepare_texture_set_range(
      std::shared_ptr<const TextureSetSurfacePlan>,size_t,const std::function<bool()>&);
  TextureSetRangeProof()=default;
  TextureSetRangeProof(TextureSetRangeProof&&)=default;
  mutable std::weak_ptr<const TextureSetRangeProof>self_;
  std::shared_ptr<const TextureSetSurfacePlan>plan_;
  size_t field_=0;
  std::shared_ptr<const TextureMaps>maps_;
};
std::shared_ptr<const TextureSetRangeProof> prepare_texture_set_range(
    std::shared_ptr<const TextureSetSurfacePlan>,size_t field,
    const std::function<bool()> &cancelled = {});
} // namespace dome::detail
