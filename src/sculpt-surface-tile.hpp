// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/local-surface.hpp"
namespace dome::detail {
inline constexpr size_t sculpt_surface_tile_working_bytes =
    sizeof(SurfaceTile)+512*(sizeof(SurfaceCell)+19*sizeof(SurfaceEdgeVertex)+
                            36*sizeof(uint32_t)+12*sizeof(Vec3))+4096;
struct SculptTileExtraction {
  std::shared_ptr<const SurfaceTile> tile;
  LocalSurfaceStats stats;
};
// A private implementation seam to the existing extractor. Both legacy and
// persistent packet owners use the exact same tetrahedra, float interpolation,
// gradient samples, prospective tile allocation and support-halo calculation.
// The returned immutable packet owns geometry only, never Scene or paint.
SculptTileExtraction extract_sculpt_surface_tile(const SurfaceGeometry &,Int3,
    const LocalSurfaceLimits &,size_t available,LocalSurfaceCache::Cancel);
} // namespace dome::detail
