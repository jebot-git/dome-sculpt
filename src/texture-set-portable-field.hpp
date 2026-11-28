// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "texture-set-field-source.hpp"
#include "dome/tangents.hpp"
namespace dome::detail {
struct PortableTextureSetCorner {
 Vec3 normal;
 Tangent tangent;
 std::array<float,2>paint,role;
};
struct PortableTextureSetField {
 std::vector<PortableTextureSetCorner>corners; // Actual domain triangle order.
 std::shared_ptr<const TextureMaps>maps;
 bool normal_role=false;
 bool reprojected=false;
};
// The original texture grid is tried first. Independent charts allow a
// distinct portable normal basis where source texture coordinates overlap.
constexpr uint32_t portable_normal_export_max_resolution(uint32_t source)noexcept{
 return source<1024?2*source:source;
}
size_t portable_normal_raster_scratch_bytes(uint32_t size) noexcept;
PortableTextureSetField portable_texture_set_field(const TextureSetView&,
 const AffineTransform&,size_t &work,const TextureSetLimits&);
std::vector<PortableTextureSetCorner> portable_texture_set_shape(
 const PolygonSurfacePreview&,std::span<const uint32_t>,const AffineTransform&,
 std::span<const std::array<float,2>>,const std::shared_ptr<const TextureMaps>&,
 bool rest_reprojected,size_t &work,const TextureSetLimits&);
void certify_portable_texture_set_skin(const PolygonSurfacePreview &,
 std::span<const uint32_t>,std::span<const PortableTextureSetCorner>,
 const AffineTransform &,const std::shared_ptr<const TextureMaps> &,
 size_t &work,const TextureSetLimits &);
}
