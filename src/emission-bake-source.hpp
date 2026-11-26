// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "dome/source-maps.hpp"
#include <optional>
namespace dome::detail {
class PaintOverlayPacket;
// The original image is sampled before layering writable emission. Preserve
// the authored UV transform and the texture's own transform and addressing.
struct EmissionImageRole {
 PaintSourceTexture texture;
 std::array<float,2> scale{1,1},offset{},rotation{1,0};
 std::array<float,3> factor{};
 std::array<float,3> sample(std::array<float,2>)const;
};
struct EmissionBakeInputs {
 using Coordinates=std::function<std::array<float,2>(uint32_t,std::array<float,3>)>;
 std::shared_ptr<const PaintOverlayPacket> paint;
 std::optional<EmissionImageRole> image;
 Coordinates paint_coordinates,image_coordinates;
 std::array<float,3> background{};
 float legacy_strength=1;
};
EmissionBakeInputs emission_bake_inputs(const SculptObject &,const std::function<bool()> &cancelled={});

// Private immutable raw source/overlay view for asset-worker Bake. Empty keeps
// the legacy UV0 dense-composite route unchanged.
std::shared_ptr<const TextureMaps> raw_bake_source(const SculptObject &,
    const std::function<bool()> &cancelled={});
std::shared_ptr<const TextureMaps> source_baked_preview(const SculptObject &,
    const std::function<bool()> &cancelled={});
// Actual immutable source radiance evaluated at the source projection hit.
// Empty means the unchanged legacy Bake path is sufficient. No RGB8 composite
// is accepted as physical radiance authority.
std::function<std::array<float,3>(uint32_t,std::array<float,3>)>
emission_bake_source(const SculptObject &,const std::function<bool()> &cancelled={});
}
