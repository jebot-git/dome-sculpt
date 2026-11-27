// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/bake.hpp"
#include <span>
namespace dome::detail {
class AffineInteractionFrame;
class PaintOverlayPacket;
struct SourceBakeNormalOptions {
 size_t max_working_bytes=256*1024*1024;
 std::function<bool()> cancelled;
 const AffineInteractionFrame *placement=nullptr;
};
// Actual artist-frame roles after morph, skin and placement evaluation.
// Only the capture function can pair a resolver with prepared material data.
class SourceBakeNormalResolver {
public:
 SourceBakeNormalResolver()=default;
 explicit operator bool() const noexcept { return bool(sample_); }
 std::array<Vec3,2> operator()(uint32_t triangle,std::array<float,3> bary) const {
   return sample_(triangle,bary);
 }
 bool matches_prepared_source(const TextureMaps &) const noexcept;
 bool requires_source_pairing() const noexcept {return bool(projection_)||bool(paired_maps_);}
 const auto &paired_maps() const noexcept {return paired_maps_;}
 const auto &paint_domain() const noexcept {return paint_domain_;}
private:
 using Sample=std::function<std::array<Vec3,2>(uint32_t,std::array<float,3>)>;
 SourceBakeNormalResolver(Sample sample,std::shared_ptr<const PaintNormalProjection> projection,
     std::shared_ptr<const PaintOverlayPacket> packet,std::shared_ptr<const TextureMaps> paired_maps={},
     std::shared_ptr<const PaintSamplingDomain> paint_domain={})
   :sample_(std::move(sample)),projection_(std::move(projection)),packet_(std::move(packet)),
    paired_maps_(std::move(paired_maps)),paint_domain_(std::move(paint_domain)){}
 Sample sample_;
 std::shared_ptr<const PaintNormalProjection> projection_;
 std::shared_ptr<const PaintOverlayPacket> packet_;
 std::shared_ptr<const TextureMaps> paired_maps_;
 std::shared_ptr<const PaintSamplingDomain> paint_domain_;
 friend SourceBakeNormalResolver source_bake_normals(
     const SculptObject &,CanvasTransform,const SourceBakeNormalOptions &);
};
bool affine_source_normal_supported(const SculptObject &) noexcept;
bool writable_bake_source_supported(const SculptObject &) noexcept;
SourceBakeNormalResolver source_bake_normals(const SculptObject &,CanvasTransform,
 const SourceBakeNormalOptions & = {});
struct RetainedSourceBakeHit {
 std::array<float,2> paint_uv;
 Vec3 normal;
 std::optional<std::array<float,3>> emission;
};
struct RetainedSourceBakeField {
 std::function<RetainedSourceBakeHit(uint32_t,std::array<float,3>)> sample;
 bool physical_emission=false;
};
struct SourceBakePlacement {
 // Transformed native Mikk corners, in original logical triangle order.
 std::span<const std::array<float,4>> tangent_corners;
 bool reverse_winding=false;
 // Coordinate origin subtracted after the target was stored as floats.
 std::array<double,3> target_rounding_origin{};
};
// Internal dual-role route; the public legacy Bake API stays byte/layout exact.
BakeResult bake_mesh_source_normals(const Mesh &,const BakeSource &,const BakeOptions &,
 const SourceBakeNormalResolver &, std::span<const uint32_t> target_triangles = {},
 std::span<const RetainedSourceBakeField> source_fields = {},
 SourceBakePlacement source_placement = {},
 std::span<const std::array<float,4>> target_metric_corners = {}, bool corresponding_topology = false,
 bool absolute_emission = false);
BakeResult bake_mesh_native_attributes(const Mesh &,const BakeSource &,const BakeOptions &,
 const SourceBakeNormalResolver &,std::span<const RetainedSourceBakeField>,
 SourceBakePlacement,std::span<const std::array<float,4>> target_metric_corners,bool corresponding_topology);
}
