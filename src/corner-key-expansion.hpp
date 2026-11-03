// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/mesh.hpp"
namespace dome::detail {
// Detached geometry only. This value grants no paint, source-material, frame or
// normal-sampling authority. An owning authoring conversion must prove each.
struct CornerKeyExpansion {
  Mesh basis;
  // A derived vertex keeps BOTH native identities. The original/source domain
  // is never overwritten with the current UV-seam vertex identity.
  std::vector<uint32_t> current_vertices,origin_vertices;
};
struct CornerKeyExpansionLimits {
  size_t max_vertices=2'000'000,max_corners=6'000'000;
  size_t max_bytes=128*1024*1024,max_work=128*1024*1024;
  std::function<bool()> cancelled;
};
CornerKeyExpansion expand_corner_key_basis(const Mesh &current,const Mesh &origin,
    const CornerKeyExpansionLimits & = {});
} // namespace dome::detail
