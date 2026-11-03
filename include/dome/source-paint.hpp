// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "paint.hpp"
#include <span>
namespace dome {
struct SourcePaintOptions {
  // The reader and producer share this ceiling. max_working_bytes may impose
  // a smaller caller budget; it never creates a file requiring a larger one.
  static constexpr size_t maximum_working_bytes=512*1024*1024;
  size_t max_vertices=2'000'000,max_triangles=2'000'000;
  size_t max_coordinate_bytes=128*1024*1024;
  size_t max_working_bytes=256*1024*1024;
  uint64_t max_raster_samples=128*1024*1024;
  uint64_t max_work=64'000'000;
  std::function<bool()> cancelled;
};
// Capability is supplied by the owning reader, never inferred from a file's
// version. Older overloads deliberately reject Paint4 even for empty layers.
struct SourcePaintDecodeContext { bool independent_source=false,physical_emission=false; };
// Explicit first-source capture, including materials without PBR image roles.
// An empty role bundle describes no images; no placeholder image is invented.
std::shared_ptr<PaintedMesh> capture_source_sampling(Mesh,
    std::shared_ptr<const PaintSourceMaps>,uint32_t resolution,
    const PaintSurfaceOptions & = {});
// Exact initial source-to-atlas adoption. The original must be pristine;
// subsequent paint/UV/detail transfer needs its own retaining adapter.
std::shared_ptr<PaintedMesh> prepare_source_paint(
    std::shared_ptr<const PaintedMesh>,Mesh target,
    std::span<const uint32_t> source_vertices,const SourcePaintOptions & = {});
// Optional genuine authored UV1 in the BEFORE vertex domain, e.g. a MToon-only
// accessor absent from PBR roles. It remains a coordinate field, never a fake
// image role or generated TEXCOORD carrier. The factory freezes its own data.
std::shared_ptr<PaintedMesh> prepare_source_paint(
    std::shared_ptr<const PaintedMesh>,Mesh target,
    std::span<const uint32_t> source_vertices,
    std::shared_ptr<const std::vector<std::array<float,2>>> authored_uv1,
    const SourcePaintOptions &);
}
