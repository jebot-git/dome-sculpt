// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "paint.hpp"
#include "navigation.hpp"
#include <filesystem>
#include <span>
namespace dome {
struct ProjectionImageLimits {
  uint64_t max_encoded_bytes = 64ull * 1024 * 1024, max_pixels = 16'777'216;
  uint32_t max_dimension = 8192;
};
Image decode_projection_png(std::span<const uint8_t>, const ProjectionImageLimits & = {});
Image load_projection_png(const std::filesystem::path &, const ProjectionImageLimits & = {});
enum class ProjectionMode { Orthographic, Perspective };
enum class ProjectionUsage { Color, Stencil };
enum class ProjectionColorSpace { Automatic, SRGB, Linear };
enum class ProjectionScalar { Luminance, Red, Green, Blue, Alpha };
enum class ProjectionAlpha { Coverage, ChannelValue };
struct ProjectionCamera {
  // Right-handed camera: +X right, +Y up, -Z forward. Pose and surface use
  // the same metric frame. Near/far/ortho dimensions are local pose units.
  CanvasTransform pose{};
  ProjectionMode mode = ProjectionMode::Orthographic;
  float width = 1, height = 1;
  float vertical_fov = 1.0471975512f, aspect = 1;
  float near_depth = .001f, far_depth = 10;
};
struct ProjectionOptions {
  ProjectionUsage usage = ProjectionUsage::Color;
  ProjectionColorSpace color_space = ProjectionColorSpace::Automatic;
  ProjectionScalar scalar = ProjectionScalar::Luminance;
  ProjectionAlpha alpha = ProjectionAlpha::Coverage;
  bool tint = false, front_only = true, visibility = true;
  float opacity = 1, minimum_facing = .05f, visibility_epsilon = .0001f;
  // [a,b,c,d,tx,ty]: u'=a*u+b*v+tx; v'=c*u+d*v+ty. Supports stencil
  // rotate/scale/translation/mirroring without moving the physical camera.
  std::array<float,6> uv_transform{1,0,0,1,0,0};
  uint64_t max_samples_per_stamp = 1'000'000;
  std::function<bool()> cancelled;
};
struct ProjectedPaint {
  PaintSettings settings;
  float opacity = 0; // Multiplies brush pressure/strength/flow exactly once.
};
// Construct on an asset worker: owns immutable image/pose and shared target
// visibility BVH. Build once per projection, not once per brush stamp. All
// geometry and positions/normals sampled by the painter must use the same frame.
class PaintProjection {
public:
  PaintProjection(Image, ProjectionCamera, const Mesh &visibility_surface,
                  ProjectionOptions = {});
  std::optional<ProjectedPaint> sample(Vec3 position, Vec3 normal,
                                      const PaintSettings &) const;
  // Cheap immutable pose/settings changes reuse image pixels and an existing
  // visibility BVH. Enabling visibility without a cached BVH needs a new worker
  // construction with geometry; these methods never construct a BVH or read files.
  PaintProjection with_camera(ProjectionCamera) const;
  PaintProjection with_camera(ProjectionCamera, ProjectionOptions) const;
  uint64_t max_samples_per_stamp() const;
private:
  struct Impl;
  std::shared_ptr<const Impl> impl_;
};
} // namespace dome
