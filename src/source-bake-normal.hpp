// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/bake.hpp"
#include <span>
namespace dome::detail {
// Actual object-space roles after authentic artist-frame/morph/skin transport,
// rotated into the low target's coordinate system. Never a display normal map.
using SourceBakeNormalResolver=std::function<std::array<Vec3,2>(uint32_t,std::array<float,3>)>;
struct SourceBakeNormalOptions {
 size_t max_working_bytes=256*1024*1024;
 std::function<bool()> cancelled;
};
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
};
// Internal dual-role route; the public legacy Bake API stays byte/layout exact.
BakeResult bake_mesh_source_normals(const Mesh &,const BakeSource &,const BakeOptions &,
 const SourceBakeNormalResolver &, std::span<const uint32_t> target_triangles = {},
 std::span<const RetainedSourceBakeField> source_fields = {},
 SourceBakePlacement source_placement = {});
}
