// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "polygon-surface-owner.hpp"
#include "dome/material-bindings.hpp"
namespace dome::detail {
enum class PolygonSurfaceNormalModel { NativeNOnly=1, PreparedCorePbr=2 };
// Before raw attribute inputs are separate from geometric-frame transport.
// Every row addresses a CURRENT logical TRIANGLE CORNER, not a raw source ID.
// Consumers must pack their geometry using the same corner stream.
class PolygonSurfacePreview final {
public:
 static std::shared_ptr<const PolygonSurfacePreview> create(const PaintedMesh &,
     std::shared_ptr<const SourceMaterialBindings>,const PolygonSurfaceTransferLimits & = {});
 bool matches(const std::shared_ptr<const SourceMaterialBindings> &,
              const std::shared_ptr<const PaintSamplingAttachment> &) const noexcept;
 bool matches_binding(const SourceMaterialBindings &) const noexcept;
 void validate_maps(const TextureMaps &) const;
 Vec3 normal_at(size_t,std::array<double,3>,const AffineTransform &,
     const std::function<bool()> &cancelled={})const;
 const auto &binding()const noexcept{return binding_;}
 const auto &field_owner()const noexcept{return owner_;}
 const auto &attachment()const noexcept{return attachment_;}
 const auto &normals()const noexcept{return normals_;}
 // Empty for genuine N-only input. No artist tangent or W is synthesized.
 const auto &tangents()const noexcept{return tangents_;}
 const auto &paint_coordinates()const noexcept{return paint_;}
 const auto &normal_coordinates()const noexcept{return normal_;}
 PolygonSurfaceNormalModel model()const noexcept{return model_;}
 // Four RGBA texels/triangle: before N+geometric-W, before T, current N+W,
 // current T. These are GEOMETRIC frames, not extra artist attributes/UVs.
 const auto &geometric_frames()const noexcept{return frames_;}
 std::vector<std::array<std::array<float,4>,4>> placed_frames(const AffineTransform &,
     const std::function<bool()> &cancelled={})const;
 // Certifies complete BEFORE role-basis interpolation, not sampled points.
 // This does not certify the nonzero/quality of every filtered normal texel;
 // actual field sampling remains owned and checked by its separate evaluator.
 // A later draw/query must use exactly this world context. This initial proof
 // may conservatively refuse triangles needing a finer interval proof.
 void validate_world(const AffineTransform &,double minimum=1e-6,
     const std::function<bool()> &cancelled={})const;
private:
 PolygonSurfacePreview()=default;
 std::weak_ptr<const PolygonSurfacePreview>self_;
 std::shared_ptr<const BakedTextures>baked_;
 std::shared_ptr<const SourceMaterialBindings> binding_;
 std::shared_ptr<const PolygonSurfaceOwner> owner_;
 std::shared_ptr<const PaintSamplingAttachment> attachment_;
 SourceNativeMaterial reference_;
 std::shared_ptr<const SourceUvOrigin> origin_;
 std::optional<std::array<float,3>> physical_;
 PolygonSurfaceNormalModel model_=PolygonSurfaceNormalModel::NativeNOnly;
 std::vector<Vec3>normals_;
 std::vector<std::array<float,4>>tangents_;
 std::vector<std::array<float,2>>paint_,normal_;
 std::vector<std::array<std::array<float,4>,4>>frames_;
};
} // namespace dome::detail
