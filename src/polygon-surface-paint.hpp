// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "polygon-surface-codec.hpp"
#include "dome/material-bindings.hpp"
namespace dome::detail {
// Typed private envelope seam. An actual replayed node grants only its exact
// current geometry/grid correspondence, never another field or arbitrary Paint.
// Public Paint6 calls require the actual synchronous Scene39 owning scope;
// standalone readers/writers keep refusing domain3.
struct PolygonSurfacePaintCodec {
 static std::shared_ptr<const TextureMaps> preview(const PaintedMesh &,
     const std::shared_ptr<const SourceMaterialBindings> &,const Material &,
     const std::function<bool()> &cancelled={},
     std::shared_ptr<const PolygonSurfacePreview> cached={});
 static nlohmann::json encode(const PaintedMesh &,
     const std::shared_ptr<const SourceMaterialBindings> &,PolygonSurfaceNodeWriter &,
     PaintImageArchive &,NormalSamplingArchive * = nullptr,
     const PolygonSurfaceTransferLimits & = {});
 static std::shared_ptr<PaintedMesh> decode(const nlohmann::json &,PolygonSurfaceNodeReader &,
     const PaintImageArchive &,const NormalSamplingArchive * = nullptr,
     const PolygonSurfaceTransferLimits & = {});
 // Borrowed raw grid inspection. shared_geometry forbids an inline Mesh and
 // grants no decode/material/domain permission by itself.
 static size_t preflight_grid(const nlohmann::json &,bool physical,bool shared_geometry,
     const PolygonSurfaceTransferLimits & = {});
 // Shape/metadata/binary scan only. Does not resolve a node or mint ownership.
 static size_t preflight(const nlohmann::json &,size_t node_count,
     const PolygonSurfaceTransferLimits & = {});
};
} // namespace dome::detail
