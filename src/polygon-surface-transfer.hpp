// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/affine.hpp"
#include "dome/polygon.hpp"
#include "normal-bake-encoding.hpp"
namespace dome::detail {
// Private mathematical/correspondence foundation. These values do not mint a
// material, field, sampling attachment, Scene capability or publication lease.
struct PolygonSurfaceTransferLimits {
 size_t maximum_steps=128,maximum_entries=8'000'000;
 size_t maximum_working_bytes=256*1024*1024;
 PolygonLimits geometry;
};
struct PolygonSurfaceStep {
 PolygonEdit edit;
 std::optional<AffineTransform> world;
};
struct PolygonSurfaceTriangle {
 uint32_t source_triangle=0;
 std::array<std::array<double,3>,3> source_barycentrics{};
};
// An immutable replayable edit program. Original CORNER sides remain distinct;
// matching positions, vertex IDs, UVs or RGBA values never merge their fields.
// Move/Flip is the first implemented boundary. New triangles/common refinement
// remain explicit further work, not an inferred nearest-surface correspondence.
class PolygonSurfaceTrace {
public:
 static PolygonSurfaceTrace begin(const PolygonMesh &,
                                  const PolygonSurfaceTransferLimits & = {});
 std::optional<PolygonSurfaceTrace> advance(const PolygonSurfaceStep &,
                                  const PolygonSurfaceTransferLimits & = {}) const;
 void validate(const PolygonSurfaceTransferLimits & = {}) const;
 std::vector<PolygonSurfaceTriangle> triangle_permutation(
                                  const PolygonSurfaceTransferLimits & = {}) const;
 const auto &original() const noexcept{return original_;}
 const auto &current() const noexcept{return current_;}
 const auto &steps() const noexcept{return steps_;}
 size_t storage_bytes() const;
private:
 std::shared_ptr<const PolygonMesh> original_,current_;
 std::vector<PolygonSurfaceStep> steps_;
};
struct PolygonGeometryFrame {
 NormalBakeBasis basis;
 float handedness=1;
};
// Evaluate the PHYSICAL geometric surface frame in the supplied current world
// frame. Normal uses inverse transpose; tangent uses the position derivative.
// The caller must supply the real role coordinates, never an edited atlas as a
// substitute for an independently owned normal coordinate set.
PolygonGeometryFrame polygon_geometry_frame(
 const std::array<Vec3,3> &,const std::array<std::array<float,2>,3> &,
 const AffineTransform &world,double minimum_relative_determinant=1e-6);
struct PolygonBeforeRole {
 // Already evaluated/interpolated full BEFORE VS outputs in the SAME frame as
 // both geometric bases. No normalize/rebuild of these columns is performed.
 NormalBakeBasis before_vs;
 float tangent_handedness=1;
 // Actual before shader evaluation decides whether a transformed B preserves
 // or reverses the declared W. It must be supplied, not guessed from geometry.
 int basis_orientation=1;
};
// Detail follows the current geometric surface frame. This preserves artist
// offsets and the before role's actual fragment/vertex normalization policy;
// freezing an old object-space normal is not this operation. The normal sample
// is the actual decoded/filtered role sample (including its declared scale).
// A native N-only field has no normal-map T/W. Its supplied actual BEFORE
// direction is transported by an explicit ordered geometric corner chart;
// that chart is not a fabricated texture-coordinate or artist-tangent owner.
Vec3 transport_polygon_direction(Vec3 before_direction,
 const PolygonGeometryFrame &before,const PolygonGeometryFrame &current,
 double minimum_relative_determinant=1e-6);
Vec3 transport_polygon_normal(const PolygonBeforeRole &,Vec3 normal_sample,
 const PolygonGeometryFrame &before,const PolygonGeometryFrame &current,
 double minimum_relative_determinant=1e-6);
} // namespace dome::detail
