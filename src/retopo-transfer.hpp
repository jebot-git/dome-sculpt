// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/affine.hpp"
#include "dome/mesh-processing.hpp"
#include "dome/texture-sets.hpp"

namespace dome::detail {

class RetopoTransferBudget {
public:
  explicit RetopoTransferBudget(const std::function<bool()> &cancelled) : cancelled_(cancelled) {}
  void work(size_t amount = 1);
  void reserve(size_t count, size_t stride);
  const auto &cancelled() const { return cancelled_; }
  size_t remaining_work() const { return work_left_; }
  size_t remaining_bytes() const { return bytes_left_; }

private:
  const std::function<bool()> &cancelled_;
  size_t work_left_ = 500'000'000, bytes_left_ = 256 * 1024 * 1024;
};

class RetopoTransfer {
public:
  RetopoTransfer(const Mesh &, const AffineTransform &previous,
                 const AffineTransform &replacement,
                 RetopoTransferBudget &);
  RetopoTransfer(const RetopoTransfer &) = delete;
  RetopoTransfer &operator=(const RetopoTransfer &) = delete;
  std::optional<SurfaceHit> match(Vec3 position, Vec3 normal,
                                const TextureSetFaceDomain *source_domain = nullptr);
  void work(size_t amount = 1) { budget_.work(amount); }
  void reserve(size_t count, size_t stride) { budget_.reserve(count, stride); }
  const auto &cancelled() const { return budget_.cancelled(); }
  const auto &placement_change() const { return placement_change_; }
  const auto &previous_metric() const { return previous_metric_; }
  const auto &replacement_metric() const { return replacement_metric_; }
  size_t remaining_bytes() const { return budget_.remaining_bytes(); }
  size_t remaining_work() const { return budget_.remaining_work(); }

private:
  RetopoTransferBudget &budget_;
  AffineTransform previous_metric_, replacement_metric_, placement_change_;
  std::unique_ptr<SurfaceQuery> old_surface_;
  std::vector<Vec3> world_normals_;
  SurfaceQueryOptions options_;
  Vec3 current_normal_;
  const TextureSetFaceDomain *current_domain_ = nullptr;
};

struct RetopoTextureSets {
  std::shared_ptr<PaintedMesh> primary;
  std::shared_ptr<const TextureSetCollection> membership;
  std::shared_ptr<const SourceMaterialBindings> primary_binding;
};
struct RetopoTextureField {
  std::shared_ptr<PaintedMesh> paint;
  std::shared_ptr<const SourceMaterialBindings> binding;
};
using RetopoFieldProjector = std::function<RetopoTextureField(const PaintedMesh &,
    const std::shared_ptr<const SourceMaterialBindings> &,
    const std::shared_ptr<const TextureSetFaceDomain> &)>;
RetopoTextureSets project_retopology_texture_sets(const PaintedMesh &primary,
    const std::shared_ptr<const SourceMaterialBindings> &primary_binding,
    const TextureSetCollection &, std::shared_ptr<const Mesh> replacement,
    std::span<const TextureSetId> assignments, RetopoTransferBudget &, const RetopoFieldProjector &);

} // namespace dome::detail
