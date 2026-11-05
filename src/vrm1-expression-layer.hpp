// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "dome/source-uv-bake.hpp"
#include "paint-overlay-packet.hpp"
namespace dome::detail {
// Private compiler product, retained only by the sealed Scene compilation.
// The ordinary source exporter cannot use this to discard a paint packet.
struct Vrm1ExpressionLayer {
 std::shared_ptr<const Image> image;
 std::vector<std::array<float,2>> paint_coordinates;
 std::array<float,2> scale{1,1},offset{};
 // Exact native shared-role rule: zero native factors use the native fallback.
 bool forward_uv=true,shade_alias=false,static_paint_animation=false;std::array<float,3> shade_divisor{1,1,1};
 double maximum_color_error=0,maximum_coverage_error=0,maximum_shade_coefficient=1;
 size_t working_bytes=0,sample_operations=0;
 // Actual paint image channels are scaled by these exact powers of two.
 // The upper base factor is reciprocal; shared shade divides by them too.
 std::array<float,3> paint_rgb_scale{1,1,1};
};
Vrm1ExpressionLayer compile_vrm1_expression_layer(const nlohmann::json &original,
 const SculptObject &,const PaintOverlayPacket &,uint32_t resolution,
 const SourceUvBakeOptions &,const std::function<bool()> &);
// Complementary source view: this is called only after the real nonempty paint
// has been compiled into the sealed additional primitive, never as pristine.
void vrm1_expression_background(const PaintedMesh &,ObjectSurface &,
 const std::function<bool()> &);
}
