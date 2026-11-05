// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mesh.hpp"
#include <nlohmann/json_fwd.hpp>
#include <functional>
#include <memory>
#include <span>
#include <utility>
namespace dome {
namespace detail { class PolygonSurfaceOwner; }
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
  uint32_t version() const noexcept { return version_; }
  // Kind3 only: an independent geometry snapshot, never a SurfaceMap alias.
  // This structural witness does not authenticate a material or Scene owner.
  const std::shared_ptr<const Mesh> &derived_geometry() const noexcept { return derived_; }
  const std::shared_ptr<const Mesh> &basis() const noexcept { return basis_; }
  std::span<const uint32_t> source_corners() const noexcept { return source_corners_; }
  size_t storage_bytes(const PaintSamplingLimits & = {}) const;
  size_t retained_bytes() const noexcept { return retained_bytes_; }
  void validate_current(const Mesh &,const PaintSamplingLimits & = {}) const;
  std::vector<std::array<float,2>> corner_coordinates(
      const Mesh &,const PaintSamplingLimits & = {}) const;
  // Temporary raster input only. Triangle and barycentric identities remain
  // current triangle IDs; a SurfaceMap must continue storing the authored mesh.
  Mesh raster_mesh(const Mesh &,const PaintSamplingLimits & = {}) const;
private:
  friend class detail::PolygonSurfaceOwner;
  PaintSamplingDomain(std::shared_ptr<const Mesh> basis,std::vector<uint32_t> corners,
                      std::shared_ptr<const Mesh> derived)
      : PaintSamplingDomain(std::move(basis),std::move(corners)) {
    version_=3;derived_=std::move(derived);
    retained_bytes_+=derived_->positions.size()*(2*sizeof(Vec3)+sizeof(std::array<float,2>))+derived_->indices.size()*sizeof(uint32_t)+derived_->uv_pins.size()+derived_->uv_seams.size();
  }
  PaintSamplingDomain(std::shared_ptr<const Mesh> basis,std::vector<uint32_t> corners)
      :basis_(std::move(basis)),source_corners_(std::move(corners)),
       retained_bytes_(basis_->positions.size()*(2*sizeof(Vec3)+sizeof(std::array<float,2>))+basis_->indices.size()*sizeof(uint32_t)+basis_->uv_pins.size()+basis_->uv_seams.size()+source_corners_.size()*sizeof(uint32_t)){}
  std::shared_ptr<const Mesh> basis_;
  uint32_t version_=1;
  std::shared_ptr<const Mesh> derived_;
  std::vector<uint32_t> source_corners_;
  size_t retained_bytes_=0;
  friend class PaintSamplingArchive;
  friend std::shared_ptr<const PaintSamplingDomain> retain_paint_sampling(
      const Mesh &,std::shared_ptr<const PaintSamplingDomain>,const Mesh &,
      std::span<const uint32_t>,const PaintSamplingLimits &);
};
struct PaintSamplingArchiveLimits {
  size_t max_domains=128,max_bases=128,max_bytes=128*1024*1024;
  PaintSamplingLimits geometry;
};
// Structural geometry pool only. Decoding/attaching this pool does not grant
// Scene24 colour, normal or source-editing authority; the owning Scene closure
// must authenticate those independently before publication.
class PaintSamplingArchive {
public:
  explicit PaintSamplingArchive(PaintSamplingArchiveLimits = {});
  uint32_t add(std::shared_ptr<const PaintSamplingDomain>);
  const std::shared_ptr<const PaintSamplingDomain> &at(uint32_t) const;
  size_t size() const noexcept { return domains_.size(); }
  size_t bytes() const noexcept { return bytes_; }
  void require_reachable() const;
  nlohmann::json encode() const;
  static PaintSamplingArchive decode(const nlohmann::json &,PaintSamplingArchiveLimits = {});
private:
  PaintSamplingArchiveLimits limits_;
  size_t bytes_=0;
  std::vector<std::shared_ptr<const PaintSamplingDomain>> domains_;
  mutable std::vector<uint8_t> referenced_;
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
                          std::shared_ptr<const PaintSamplingDomain> domain,std::shared_ptr<const void> surface)
      :geometry_(std::move(geometry)),domain_(std::move(domain)),surface_(std::move(surface)){}
  std::shared_ptr<const Mesh> geometry_;
  std::shared_ptr<const PaintSamplingDomain> domain_;
  // Actual geometry-only SurfaceMap owner, not a Paint/Material/recipe edge.
  std::shared_ptr<const void> surface_;
  friend class PaintedMesh;
  friend struct SourcePaintSamplingAccess;
};
// Exact, explicit source-corner map, not positional/nearest matching. A complete
// ordered triangle may move in the array but may not reverse, duplicate or lose
// a source triangle. UV-only callers normally supply the identity corner map.
std::shared_ptr<const PaintSamplingDomain> retain_paint_sampling(
    const Mesh &before,std::shared_ptr<const PaintSamplingDomain> previous,
    const Mesh &current,std::span<const uint32_t> before_corners,
    const PaintSamplingLimits & = {});
} // namespace dome
