// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/mesh.hpp"
#include "dome/source-maps.hpp"
#include <functional>

namespace dome::detail {
struct SourceSamplingLimits {
  size_t max_vertices=2'000'000,max_triangles=2'000'000;
  size_t max_coordinate_bytes=128*1024*1024;
  uint64_t max_work=64'000'000;
  std::function<bool()> cancelled;
};
// Transport only original image sampling across an exact UV-only layout.
// The new UV0 remains the authored atlas. Original UV0 gets a distinct actual
// coordinate carrier; UV1 and extra source sets retain their logical numbers.
// This is not writable paint, a material/normal-frame proof, or Scene admission.
// Callers still need independently owned authoring and persistence authorities.
std::shared_ptr<const PaintSourceMaps> transport_source_sampling(
    const Mesh &before,const PaintSourceMaps &,const Mesh &target,
    std::span<const uint32_t> source_vertices,const SourceSamplingLimits & = {});
}
