// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/mesh.hpp"
namespace dome::detail {
// Optional worker-side diagnostics used to verify packing independently. This
// is not document state or a public Mesh/AtlasInfo layout change.
struct AtlasDiagnostics {
  std::vector<uint32_t> face_charts;
  std::vector<uint8_t> repaired_faces;
  uint32_t internal_padding = 0, band_width = 0;
  float u_scale = 1, u_offset = 0;
};
AtlasInfo unwrap_atlas(Mesh &,uint32_t resolution,uint32_t padding,AtlasDiagnostics *);
AtlasInfo unwrap_atlas(Mesh &,uint32_t resolution,uint32_t padding,AtlasDiagnostics *,
                       const std::function<bool()> &cancelled);
}
