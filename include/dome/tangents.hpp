// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mesh.hpp"
#include <span>
namespace dome {
using Tangent = std::array<float, 4>;
// Per triangle corner: a shared position may have distinct tangent frames on
// mirrored UV islands. Never average these back onto original vertex IDs.
std::vector<Tangent> tangent_corners(const Mesh &,
                                     const std::function<bool()> &cancelled = {});
struct TangentFrame {
  Vec3 tangent, bitangent, normal;
  Vec3 to_world(Vec3) const;
  Vec3 to_tangent(Vec3) const;
};
TangentFrame tangent_frame(const Mesh &, std::span<const Tangent> corners,
                           size_t triangle, std::array<float, 3> barycentric);
} // namespace dome
