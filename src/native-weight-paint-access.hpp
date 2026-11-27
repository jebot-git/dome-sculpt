// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/paint.hpp"
namespace dome::detail {
// Private native-weight staging only. Cache/history are not authoring values.
struct NativeWeightPaintAccess {
  struct Surface {
    std::shared_ptr<const void> owner, raster;
    std::shared_ptr<const Mesh> geometry;
    std::shared_ptr<const TextureSetFaceDomain> membership;
    std::shared_ptr<const BakedTextures> baked;
    size_t bytes = 0, raster_bytes = 0;
  };
  static Surface surface(const PaintedMesh &,bool prospective=false);
  static bool same_current(const PaintedMesh &,const PaintedMesh &) noexcept;
  static std::shared_ptr<PaintedMesh> snapshot(const PaintedMesh &);
};
}
