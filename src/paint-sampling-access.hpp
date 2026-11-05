// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/paint.hpp"
#include "dome/paint-sampling.hpp"
namespace dome {
// Exact private strict-raster/geometry forecast, including constructor scratch.
// This does not authenticate any material, Paint, or sampling-domain owner.
size_t paint_sampling_surface_retained_bytes(size_t vertices,size_t corners,uint32_t resolution);
size_t paint_sampling_surface_working_bytes(size_t vertices,size_t corners,uint32_t resolution);
// Detached geometry/grid stage only. The owning colour/material/normal recipe
// must be admitted atomically before Scene24 can expose this editable result.
std::shared_ptr<PaintedMesh> retain_paint_grid(const PaintedMesh &,Mesh,
    std::shared_ptr<const PaintSamplingDomain>,const PaintSamplingLimits & = {});
// Borrowed target is copied only after the complete grid budget preflight.
std::shared_ptr<PaintedMesh> retain_paint_grid_borrowed(const PaintedMesh &,const Mesh &,
    std::shared_ptr<const PaintSamplingDomain>,const PaintSamplingLimits & = {});
struct SourceNormalSampling;
std::shared_ptr<PaintedMesh> adopt_retained_paint_normal(const PaintedMesh &,const SourceNormalSampling &,
    const PaintSamplingLimits & = {});
std::shared_ptr<PaintedMesh> rebind_paint_sampling_geometry(const PaintedMesh &,
    std::shared_ptr<const PaintSamplingAttachment>,const PaintSamplingLimits & = {});
}
