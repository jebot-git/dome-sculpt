// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/texture-sets.hpp"
namespace dome::detail {
struct TextureSetDraw {
  TextureSetId set=0;
  size_t first=0,count=0;
};
// One current geometry authority, one partitioned EBO and one explicit logical
// corner stream. No per-set Mesh or deformation copy. This immutable plan does
// not authorize a normal/source/paint sampling field; its first producer guards
// those owners until the complete per-corner attribute adapter is implemented.
class TextureSetSurfacePlan {
public:
  const std::shared_ptr<const Mesh>&geometry()const noexcept{return geometry_;}
  const std::shared_ptr<const DeformationState>&deformation()const noexcept{return deformation_;}
  const std::shared_ptr<const TextureSetCollection>&collection()const noexcept{return collection_;}
  std::span<const TextureSetView>fields()const noexcept{return fields_;}
  std::span<const TextureSetDraw>draws()const noexcept{return draws_;}
  std::span<const uint32_t>indices()const noexcept{return indices_;}
  std::span<const uint32_t>logical_corners()const noexcept{return corners_;}
  // Worker-only exact REST replay before evaluating pose/world transforms.
  void authenticate_rest(const Mesh&,const std::shared_ptr<const DeformationState>&,
                         const TextureSetLimits & = {})const;
private:
  friend std::shared_ptr<const TextureSetSurfacePlan> prepare_texture_set_surface(const SculptObject&,const TextureSetLimits&);
  TextureSetSurfacePlan()=default;
  TextureSetSurfacePlan(const TextureSetSurfacePlan&)=delete;
  std::shared_ptr<const Mesh>geometry_;
  std::shared_ptr<const DeformationState>deformation_;
  std::shared_ptr<const TextureSetCollection>collection_;
  std::vector<TextureSetView>fields_;
  std::vector<TextureSetDraw>draws_;
  std::vector<uint32_t>indices_,corners_;
};
std::shared_ptr<const TextureSetSurfacePlan> prepare_texture_set_surface(
    const SculptObject&,const TextureSetLimits & = {});
} // namespace dome::detail
