// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/bake.hpp"
#include "dome/source-maps.hpp"
#include <span>
namespace dome::paint_compute {class MaterialAtlas;}
namespace dome::detail {
class AffineInteractionFrame;
class PaintOverlayPacket;
struct SourceBakeNormalOptions {
 size_t max_working_bytes=256*1024*1024;
 std::function<bool()> cancelled;
 const AffineInteractionFrame *placement=nullptr;
 std::shared_ptr<const std::vector<std::array<float,2>>> authored_mtoon_uv1{};
};
// Actual artist-frame roles after morph, skin and placement evaluation.
// Only the capture function can pair a resolver with prepared material data.
class SourceBakeNormalResolver {
public:
 using FilteredTextures=std::array<std::array<float,4>,2>;
 SourceBakeNormalResolver()=default;
 explicit operator bool() const noexcept { return bool(sample_); }
 std::array<Vec3,2> operator()(uint32_t triangle,std::array<float,3> bary,bool mtoon_back=false) const {
   return sample_(triangle,bary,mtoon_back,nullptr);
 }
 // Original PBR and typed normal images can be filtered in batches before
 // evaluating these same authored tangent columns on the worker.
 bool accepts_filtered_textures() const noexcept { return bool(texture_uv_); }
 std::array<float,2> texture_uv(unsigned role,uint32_t triangle,std::array<float,3> bary) const {
   return texture_uv_(role,triangle,bary);
 }
 std::array<Vec3,2> with_filtered_textures(uint32_t triangle,std::array<float,3> bary,
     const FilteredTextures &textures,bool mtoon_back=false) const {
   if(!texture_uv_)throw std::logic_error("Normal field has no original texture coordinate capture");
   return sample_(triangle,bary,mtoon_back,&textures);
 }
 bool matches_prepared_source(const TextureMaps &) const noexcept;
 bool requires_source_pairing() const noexcept {return bool(projection_)||bool(paired_maps_);}
 const auto &paired_maps() const noexcept {return paired_maps_;}
 const auto &paint_domain() const noexcept {return paint_domain_;}
private:
 using Sample=std::function<std::array<Vec3,2>(uint32_t,std::array<float,3>,bool,const FilteredTextures *)>;
 using TextureUv=std::function<std::array<float,2>(unsigned,uint32_t,std::array<float,3>)>;
 SourceBakeNormalResolver(Sample sample,std::shared_ptr<const PaintNormalProjection> projection,
     std::shared_ptr<const PaintOverlayPacket> packet,std::shared_ptr<const TextureMaps> paired_maps={},
     std::shared_ptr<const PaintSamplingDomain> paint_domain={},TextureUv texture_uv={})
   :sample_(std::move(sample)),projection_(std::move(projection)),packet_(std::move(packet)),
    paired_maps_(std::move(paired_maps)),paint_domain_(std::move(paint_domain)),texture_uv_(std::move(texture_uv)){}
 Sample sample_;
 std::shared_ptr<const PaintNormalProjection> projection_;
 std::shared_ptr<const PaintOverlayPacket> packet_;
 std::shared_ptr<const TextureMaps> paired_maps_;
 std::shared_ptr<const PaintSamplingDomain> paint_domain_;
 TextureUv texture_uv_;
 friend SourceBakeNormalResolver source_bake_normals(
     const SculptObject &,CanvasTransform,const SourceBakeNormalOptions &);
};
bool affine_source_normal_supported(const SculptObject &) noexcept;
bool writable_bake_source_supported(const SculptObject &) noexcept;
SourceBakeNormalResolver source_bake_normals(const SculptObject &,CanvasTransform,
 const SourceBakeNormalOptions & = {});
struct RetainedSourceBakeHit {
 std::array<float,2> paint_uv;
 std::array<float,2> normal_uv;
};
struct RetainedSourceBakeField {
 std::function<RetainedSourceBakeHit(uint32_t,std::array<float,3>)> sample;
 std::function<Vec3(uint32_t,std::array<float,3>,std::array<float,4>)> transport_normal;
 PaintSourceTexture normal_image;
 std::optional<std::array<float,3>> emission_background;
 std::shared_ptr<const paint_compute::MaterialAtlas> atlas;
};
// Individual material lookups use donor-local triangle and vertex IDs even
// when projection and AO run over a single combined surface.
struct SourceBakeDomain {
 const BakeSource *source;
 const SourceBakeNormalResolver *normals;
 std::span<const std::array<float,4>> tangent_corners;
 bool reverse_winding=false;
};
struct SourceBakePlacement {
 // Transformed native Mikk corners, in original logical triangle order.
 std::span<const std::array<float,4>> tangent_corners;
 bool reverse_winding=false;
 // Coordinate origin subtracted after the target was stored as floats.
 std::array<double,3> target_rounding_origin{};
 std::span<const SourceBakeDomain> domains;
};
// Internal dual-role route; the public legacy Bake API stays byte/layout exact.
BakeResult bake_mesh_source_normals(const Mesh &,const BakeSource &,const BakeOptions &,
 const SourceBakeNormalResolver &, std::span<const uint32_t> target_triangles = {},
 std::span<const RetainedSourceBakeField> source_fields = {},
 SourceBakePlacement source_placement = {},
 std::span<const std::array<float,4>> target_metric_corners = {}, bool corresponding_topology = false,
 bool absolute_emission = false,std::optional<float> canonical_pair_error = {});
BakeResult bake_mesh_native_attributes(const Mesh &,const BakeSource &,const BakeOptions &,
 const SourceBakeNormalResolver &,std::span<const RetainedSourceBakeField>,
 SourceBakePlacement,std::span<const std::array<float,4>> target_metric_corners,bool corresponding_topology);
}
