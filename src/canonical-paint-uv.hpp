// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "dome/paint-sampling.hpp"
#include "canonical-normal-uv.hpp"
namespace dome::detail {
// Detached geometry/grid stage. No material lease or Scene admission is minted.
// The current-source map is separate from the flattened original normal map.
struct CanonicalPaintUvGeometry {
 std::shared_ptr<const PaintedMesh> before_paint;
 std::shared_ptr<PaintedMesh> paint;
 std::shared_ptr<const CanonicalNormalUv> normal_uv;
 std::shared_ptr<const PaintSamplingDomain> domain;
 std::vector<uint32_t> current_sources;
};
CanonicalPaintUvGeometry stage_canonical_paint_uv(const SculptObject &,const Mesh &,
 std::span<const uint32_t>,const PaintSamplingLimits & = {});
}
