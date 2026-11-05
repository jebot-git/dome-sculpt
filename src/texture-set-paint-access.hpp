// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/texture-sets.hpp"
namespace dome::detail {
struct TextureSetPaintAccess {
  static const std::shared_ptr<const TextureSetFaceDomain>& domain(const PaintedMesh&)noexcept;
  static size_t working_bytes(const PaintedMesh&,uint32_t resolution);
  static std::shared_ptr<PaintedMesh> bind(const PaintedMesh&,
      std::shared_ptr<const TextureSetFaceDomain>,uint32_t resolution,
      const TextureSetLimits&);
};
} // namespace dome::detail
