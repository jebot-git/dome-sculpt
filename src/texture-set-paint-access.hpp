// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/texture-sets.hpp"
namespace dome::detail {
struct TextureSetPaintAccess {
  static const std::shared_ptr<const TextureSetFaceDomain>& domain(const PaintedMesh&)noexcept;
  // Membership-required Paint7 payload. The domain is minted only by the
  // authenticated collection decoder; no standalone bool grants authority.
  static nlohmann::json encode(const PaintedMesh &,PaintImageArchive *,NormalSamplingArchive *,PaintSamplingArchive *);
  static std::shared_ptr<PaintedMesh> decode(const nlohmann::json &,const PaintImageArchive *,bool,
      const NormalSamplingArchive *,const PaintSamplingArchive *,const SourcePaintDecodeContext &,
      const std::shared_ptr<const TextureSetFaceDomain> &);
  static bool valid_raster(const PaintedMesh&)noexcept;
  static size_t initial_raster_bytes(uint32_t);
  static std::shared_ptr<PaintedMesh> empty(std::shared_ptr<const TextureSetFaceDomain>,uint32_t,const TextureSetLimits&);
  static size_t working_bytes(const PaintedMesh&,uint32_t resolution);
  static std::shared_ptr<PaintedMesh> bind(const PaintedMesh&,
      std::shared_ptr<const TextureSetFaceDomain>,uint32_t resolution,
      const TextureSetLimits&);
};
} // namespace dome::detail
