// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mesh.hpp"
#include <functional>
#include <memory>
#include <span>
#include <utility>
namespace dome {
struct PaintSamplingLimits {
  size_t max_vertices=2'000'000,max_triangles=2'000'000;
  size_t max_basis_bytes=128*1024*1024,max_working_bytes=256*1024*1024;
  uint64_t max_work=64'000'000;
  std::function<bool()> cancelled;
};
// Geometry-only writable-paint correspondence. This is NOT a material, colour
// or normal authority. Paired Scene24 owners must authenticate those separately.
// mesh() remains the current authored geometry/UV0. basis->uv describes the one
// retained editable paint grid. Each source_corners entry indexes basis.indices,
// rather than a current vertex, so welding never averages independent seams.
class PaintSamplingDomain {
public:
  uint32_t version() const noexcept { return 1; }
  const std::shared_ptr<const Mesh> &basis() const noexcept { return basis_; }
  std::span<const uint32_t> source_corners() const noexcept { return source_corners_; }
  size_t storage_bytes(const PaintSamplingLimits & = {}) const;
  void validate_current(const Mesh &,const PaintSamplingLimits & = {}) const;
  std::vector<std::array<float,2>> corner_coordinates(
      const Mesh &,const PaintSamplingLimits & = {}) const;
  // Temporary raster input only. Triangle and barycentric identities remain
  // current triangle IDs; a SurfaceMap must continue storing the authored mesh.
  Mesh raster_mesh(const Mesh &,const PaintSamplingLimits & = {}) const;
private:
  PaintSamplingDomain(std::shared_ptr<const Mesh> basis,std::vector<uint32_t> corners)
      :basis_(std::move(basis)),source_corners_(std::move(corners)){}
  std::shared_ptr<const Mesh> basis_;
  std::vector<uint32_t> source_corners_;
  friend std::shared_ptr<const PaintSamplingDomain> retain_paint_sampling(
      const Mesh &,std::shared_ptr<const PaintSamplingDomain>,const Mesh &,
      std::span<const uint32_t>,const PaintSamplingLimits &);
};
// Ephemeral factory proof for one exact authored mesh/domain attachment. It is
// held by PaintedMesh, never by SurfaceMap, so the strong mesh alias cannot form
// a SurfaceMap -> attachment -> SurfaceMap ownership cycle. This seals geometry
// only: editable layers, material factors and style are deliberately unrelated.
class PaintSamplingAttachment {
public:
  const std::shared_ptr<const Mesh> &authored_geometry() const noexcept { return geometry_; }
  const std::shared_ptr<const PaintSamplingDomain> &domain() const noexcept { return domain_; }
  bool matches(const std::shared_ptr<const Mesh> &geometry,
               const std::shared_ptr<const PaintSamplingDomain> &domain) const noexcept {
    return geometry&&domain&&geometry==geometry_&&domain==domain_&&
      !geometry.owner_before(geometry_)&&!geometry_.owner_before(geometry)&&
      !domain.owner_before(domain_)&&!domain_.owner_before(domain);
  }
private:
  PaintSamplingAttachment(std::shared_ptr<const Mesh> geometry,
                          std::shared_ptr<const PaintSamplingDomain> domain)
      :geometry_(std::move(geometry)),domain_(std::move(domain)){}
  std::shared_ptr<const Mesh> geometry_;
  std::shared_ptr<const PaintSamplingDomain> domain_;
  friend class PaintedMesh;
};
// Exact, explicit source-corner map, not positional/nearest matching. A complete
// ordered triangle may move in the array but may not reverse, duplicate or lose
// a source triangle. UV-only callers normally supply the identity corner map.
std::shared_ptr<const PaintSamplingDomain> retain_paint_sampling(
    const Mesh &before,std::shared_ptr<const PaintSamplingDomain> previous,
    const Mesh &current,std::span<const uint32_t> before_corners,
    const PaintSamplingLimits & = {});
} // namespace dome
