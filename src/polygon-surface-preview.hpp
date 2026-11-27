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
 // Placed primitive frames use the same inverse transpose and handedness as
 // the packed viewport stream. The before-role basis is interpolated separately.
 std::pair<PolygonGeometryFrame,PolygonGeometryFrame> triangle_frames(size_t,const AffineTransform &)const;
 NormalBakeBasis before_role_basis(size_t,std::array<double,3>,const AffineTransform &)const;
 std::shared_ptr<const PolygonSurfacePreview> with_bake(const PaintedMesh &)const;
 std::shared_ptr<const PolygonSurfacePreview> with_bake(
     const PaintedMesh &,std::shared_ptr<const Image>)const;
 // A transient posed view. The original corner inputs, sampling attachment,
 // material maps and all before frames retain their exact owners.
 std::shared_ptr<const PolygonSurfacePreview> with_geometry(
     const std::shared_ptr<const Mesh> &rest,const Mesh &posed,
     std::span<const uint32_t> triangles,
     const std::function<bool()> &cancelled={})const;
 Vec3 sample_native_bake(size_t,std::array<double,3>,const AffineTransform &)const;
 // Explicit image-space lookup used by export at raster centers and gutters.
 // The barycentrics still address the genuine triangle frame.
 Vec3 sample_baked_normal(size_t,std::array<double,3>,const AffineTransform &,
     std::array<float,2> uv)const;
 void validate_world(size_t,const AffineTransform &,double minimum=1e-6,
     const std::function<bool()> &cancelled={})const;
 void validate_bake_world(std::span<const uint32_t>,const AffineTransform &,
     const std::function<bool()> &cancelled={})const;
 const auto &native_bake()const noexcept{return native_bake_;}
 const auto &baked()const noexcept{return baked_;}
 const auto &binding()const noexcept{return binding_;}
 const auto &field_owner()const noexcept{return owner_;}
 const auto &attachment()const noexcept{return attachment_;}
 const auto &normals()const noexcept{return streams_->normals;}
 // Empty for genuine N-only input. No artist tangent or W is synthesized.
 const auto &tangents()const noexcept{return streams_->tangents;}
 const auto &paint_coordinates()const noexcept{return streams_->paint;}
 const auto &normal_coordinates()const noexcept{return streams_->normal;}
 PolygonSurfaceNormalModel model()const noexcept{return model_;}
 // Four RGBA texels/triangle: before N+geometric-W, before T, current N+W,
 // current T. These are GEOMETRIC frames, not extra artist attributes/UVs.
 const auto &geometric_frames()const noexcept{return posed_frames_?*posed_frames_:streams_->frames;}
 const auto &posed_frame_owner()const noexcept{return posed_frames_;}
 std::vector<std::array<std::array<float,4>,4>> placed_frames(const AffineTransform &,
     const std::function<bool()> &cancelled={})const;
 // Certifies complete BEFORE role-basis interpolation, not sampled points.
 // This does not certify the nonzero/quality of every filtered normal texel;
 // actual field sampling remains owned and checked by its separate evaluator.
 // A later draw/query must use exactly this world context. This initial proof
 // may conservatively refuse triangles needing a finer interval proof.
 void validate_world(const AffineTransform &,double minimum=1e-6,
     const std::function<bool()> &cancelled={})const;
 bool shares_streams(const PolygonSurfacePreview &other) const noexcept {
   return streams_ == other.streams_ && attachment_ == other.attachment_;
 }
private:
 friend class BakeRecipeRemap;
 PolygonSurfacePreview()=default;
 std::weak_ptr<const PolygonSurfacePreview>self_;
 std::shared_ptr<const BakedTextures>baked_;
 // RGBA8 on the retained paint grid. Zero alpha denotes inherited normals.
 std::shared_ptr<const Image> native_bake_;
 std::shared_ptr<const SourceMaterialBindings> binding_;
 std::shared_ptr<const PolygonSurfaceOwner> owner_;
 std::shared_ptr<const PaintSamplingAttachment> attachment_;
 SourceNativeMaterial reference_;
 std::shared_ptr<const SourceUvOrigin> origin_;
 std::optional<std::array<float,3>> physical_;
 PolygonSurfaceNormalModel model_=PolygonSurfaceNormalModel::NativeNOnly;
 struct Streams {
   std::vector<Vec3> normals;
   std::vector<std::array<float,4>> tangents;
   std::vector<std::array<float,2>> paint, normal;
   std::vector<std::array<uint8_t,3>> native_frame_charts;
   std::vector<std::array<std::array<float,4>,4>> frames;
 };
 std::shared_ptr<const Streams> streams_;
 std::shared_ptr<const std::vector<std::array<std::array<float,4>,4>>> posed_frames_;
};
} // namespace dome::detail
