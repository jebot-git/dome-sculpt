// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/paint.hpp"
namespace dome::detail {
// Private native-weight staging only. Cache/history are not authoring values.
struct NativeWeightPaintAccess {
  struct Surface {
    std::shared_ptr<const void> owner, raster, original_sampling;
    std::shared_ptr<const Mesh> geometry;
    std::shared_ptr<const TextureSetFaceDomain> membership;
    std::shared_ptr<const PaintSamplingDomain> domain;
    std::shared_ptr<const PaintedMesh> before;
    std::shared_ptr<const PaintSourceMaps> original_maps;
    std::shared_ptr<const BakedTextures> original_bake;
    std::array<std::shared_ptr<const std::vector<std::array<float,2>>>,3> original_uvs;
    size_t bytes = 0, raster_bytes = 0;
    size_t original_bytes = 0;
  };
  struct Paint {
    Surface surface;
    std::shared_ptr<const PaintSamplingAttachment> attachment;
    std::shared_ptr<const PaintColorProjection> color;
    std::shared_ptr<const PaintNormalProjection> normal;
    std::shared_ptr<const BakedTextures> baked;
  };
  static Paint paint(const PaintedMesh &,bool prospective);
  static Surface surface(const PaintSamplingAttachment &);
  static bool same_current(const PaintedMesh &,const PaintedMesh &) noexcept;
  static std::shared_ptr<PaintedMesh> snapshot(const PaintedMesh &);
private:
  static Surface surface(const std::shared_ptr<const PaintedMesh::SurfaceMap> &);
};
}
