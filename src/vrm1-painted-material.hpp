// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "dome/material-bindings.hpp"
#include <set>
#include "paint-overlay-packet.hpp"
namespace dome::detail {
struct Vrm1PaintedMaterial {
  MToonMaterial material;
  std::array<PaintSourceTexture,size_t(MToonSlot::Count)> roles;
  std::set<std::string> rewritten_properties;
  size_t image_bytes=0;
  std::array<float,3> maximum_quantization_error{}; // base, shade, emission
};
// Internal arithmetic only; the sealed Scene compiler authenticates every input
// and owns material/expression remapping. Authored is the real pre-bake binding,
// so separate resampled images cannot erase a genuine original shade alias.
Vrm1PaintedMaterial compose_vrm1_paint(
    const ObjectSurface &, MToonMaterial sampled,
    std::array<PaintSourceTexture,size_t(MToonSlot::Count)> sampled_roles,
    const SourceMaterialBindings &authored, const PaintOverlayPacket &,
    uint32_t resolution, size_t maximum_image_bytes,
    const std::function<bool()> &cancelled);
}
