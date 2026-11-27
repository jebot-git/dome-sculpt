// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/affine.hpp"
#include "dome/mesh-processing.hpp"

namespace dome::detail {

class RetopoTransfer {
public:
  RetopoTransfer(const Mesh &, const AffineTransform &previous,
                 const AffineTransform &replacement,
                 const std::function<bool()> &cancelled);
  RetopoTransfer(const RetopoTransfer &) = delete;
  RetopoTransfer &operator=(const RetopoTransfer &) = delete;
  std::optional<SurfaceHit> match(Vec3 position, Vec3 normal);
  void work(size_t amount = 1);
  void reserve(size_t count, size_t stride);
  const auto &cancelled() const { return cancelled_; }
  const auto &placement_change() const { return placement_change_; }

private:
  const std::function<bool()> &cancelled_;
  size_t work_left_ = 500'000'000, bytes_left_ = 256 * 1024 * 1024;
  AffineTransform previous_metric_, replacement_metric_, placement_change_;
  std::unique_ptr<SurfaceQuery> old_surface_;
  std::vector<Vec3> world_normals_;
  SurfaceQueryOptions options_;
  Vec3 current_normal_;
};

} // namespace dome::detail
