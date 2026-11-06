// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "polygon-surface-transfer.hpp"
#include "dome/polygon-scene.hpp"
namespace dome::detail {
// Detached owning source for the first Move/Flip transfer boundary. It does
// not attach a Paint, change a material or grant Scene39 publication authority.
// Original native/frame IDs remain original; the polygon derivative has its
// own corner domain. The current binding is never an edge of this graph.
class PolygonSurfaceOwner;
struct PolygonSurfaceStage {
 std::shared_ptr<const PolygonSurfaceOwner> owner;
 std::shared_ptr<PaintedMesh> paint;
 std::shared_ptr<const SourceMaterialBindings> binding;
};
class PolygonSurfaceOwner final {
public:
 static std::shared_ptr<const PolygonSurfaceOwner> capture(
     const Scene &,uint64_t,const PolygonSurfaceTransferLimits & = {});
 std::shared_ptr<const PolygonSurfaceOwner> advance(
     const PolygonSurfaceStep &,const PolygonSurfaceTransferLimits & = {}) const;
 // Inner private recipe only. It is not a standalone Scene/material record;
 // the owning polygon-source pool must supply the actual full before-root.
 nlohmann::json encode_recipe(const PolygonSurfaceTransferLimits & = {}) const;
 static std::shared_ptr<const PolygonSurfaceOwner> replay(
     std::shared_ptr<const PolygonOrigin>,const nlohmann::json &,
     const PolygonSurfaceTransferLimits & = {});
 // Conservative simultaneous capture/replay forecast, before source copies.
 // This is a resource bound only and authenticates no source or current owner.
 static size_t source_working_bytes(const PolygonOrigin &,
     const PolygonSurfaceTransferLimits & = {});
 void validate_current_geometry(const Mesh &,const PolygonSurfaceTransferLimits & = {}) const;
 bool valid_owner(const std::shared_ptr<const PolygonSurfaceOwner> &) const noexcept;
 void validate(const PolygonSurfaceTransferLimits & = {}) const;
 const auto &before() const noexcept{return before_;}
 const auto &trace() const noexcept{return trace_;}
 const auto &current_attachment() const noexcept{return attachment_;}
 // These two streams are intentionally independent. The first addresses the
 // exact sparse editable Paint/F32 grid; the second addresses the old resolved
 // canonical normal field. Neither is the new triangulation's atlas.
 std::array<float,2> paint_coordinates(size_t triangle,std::array<double,3>,
     const PolygonSurfaceTransferLimits & = {}) const;
 std::array<float,2> normal_coordinates(size_t triangle,std::array<double,3>,
     const PolygonSurfaceTransferLimits & = {}) const;
 // Actual prepared CorePBR evaluation from the captured raw posed N/T and
 // filtered owning field. World/held-object transforms are evaluated now,
 // never baked into an anchor or substituted by a guessed similarity.
 // Normal-map-free native meshes retain their actual evaluated/interpolated
 // N field with a declared geometric corner chart; no role UV/T/W is minted.
 Vec3 native_normal(size_t triangle,std::array<double,3>,const AffineTransform &,
     const PolygonSurfaceTransferLimits & = {}) const;
 Vec3 pbr_normal(size_t triangle,std::array<double,3>,const AffineTransform &,
     const PolygonSurfaceTransferLimits & = {}) const;
 bool has_normal_field() const noexcept;
 PolygonSurfaceStage stage_paint(const PolygonSurfaceTransferLimits & = {}) const;
 bool matches_current(const PaintedMesh &) const noexcept;
 void validate_binding(const SourceMaterialBindings &,const PolygonSurfaceTransferLimits & = {}) const;
 void validate_current(const PaintedMesh &,const PolygonSurfaceTransferLimits & = {}) const;
 // Structural stage only; current Paint/material publication additionally
 // needs the genuine39 attachment and current normal-field binding adapter.
 std::shared_ptr<const PaintSamplingDomain> paint_domain(
     const PolygonSurfaceTransferLimits & = {}) const;
private:
 friend class PolygonSurfacePreview;
 PolygonSurfaceOwner()=default;
 PolygonSurfaceOwner(const PolygonSurfaceOwner&)=delete;
 std::weak_ptr<const PolygonSurfaceOwner> self_;
 std::shared_ptr<const PolygonOrigin> before_;
 std::shared_ptr<const PaintSamplingAttachment> attachment_;
 PolygonSurfaceTrace trace_;
 // Original derivative corner -> original native corner, never raw vertex IDs.
 std::vector<uint32_t> native_corners_;
 std::vector<std::array<float,2>> paint_corners_,normal_corners_;
 std::array<double,3> source_barycentrics(size_t,std::array<double,3>,
     const std::vector<PolygonSurfaceTriangle> &) const;
};
} // namespace dome::detail
