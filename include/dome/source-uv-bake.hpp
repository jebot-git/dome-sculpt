// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "humanoid-creator.hpp"
#include "mtoon.hpp"
#include "source-maps.hpp"
#include "material-bindings.hpp"
#include <optional>
namespace dome {
struct SourceUvBakeOptions {
  // Zero preserves at least each original image's largest dimension and the
  // editable paint resolution. Nonzero is an explicit resampling choice.
  uint32_t resolution = 0, padding = 4;
  SourceUvAnimationPolicy animation = SourceUvAnimationPolicy::RejectAnimated;
  double freeze_seconds = 0;
  size_t max_vertices = 2'000'000, max_triangles = 2'000'000;
  size_t max_image_pixels = 16 * 1024 * 1024;
  size_t max_output_bytes = 256 * 1024 * 1024;
  size_t max_layer_bytes = 128 * 1024 * 1024;
  size_t max_working_bytes = 256 * 1024 * 1024;
  size_t max_raster_samples = 128 * 1024 * 1024;
  size_t max_sample_operations = 256 * 1024 * 1024;
  std::function<bool()> cancelled;
};
struct SourceUvBakeInput {
  std::shared_ptr<const PaintedMesh> paint;
  std::shared_ptr<const CreatorAppearance> appearance;
  std::shared_ptr<const SourceUvMaterialBindings> current_material;
  std::optional<SourceNativeMaterial> native_material;
  std::shared_ptr<const DeformationState> current_deformation;
  Mesh target;
  // Destination vertex -> original PaintedMesh vertex. Every ordered corner
  // must map exactly; topology/geometry/order changes are not nearest-projected.
  std::vector<uint32_t> source_vertices;
};
struct SourceUvBakeResult {
  Mesh target;
  std::vector<uint32_t> source_vertices;
  std::shared_ptr<const PaintSourceMaps> maps;
  // Complete editable layers; metadata/IDs and mask/channel coverage survive.
  // Adoption uses original_paint's next ID and one caller-owned history boundary.
  std::vector<PaintLayer> layers;
  std::shared_ptr<const BakedTextures> baked;
  uint64_t active_layer = 0;
  std::shared_ptr<const PaintedMesh> original_paint;
  std::shared_ptr<const CreatorAppearance> original_appearance;
  std::shared_ptr<const SourceMaterialBindings> original_material;
  std::shared_ptr<const SourceUvOrigin> uv_origin;
  SourceNativeMaterial native_reference;
  std::shared_ptr<const NormalFrameSet> normal_frames;
  std::shared_ptr<const DeformationState> deformation;
  std::shared_ptr<const CreatorAppearance> appearance;
  // Explicit resolved role table. Original descriptor texture indices remain
  // provenance, NEVER implicit indices into this table. Consumers must use the
  // table or explicitly build/remap a new asset registry when exporting.
  std::optional<MToonMaterial> mtoon;
  std::array<PaintSourceTexture, size_t(MToonSlot::Count)> mtoon_roles;
  std::array<bool, size_t(MToonSlot::Count)> view_dependent{};
  SourceUvAnimationPolicy animation_policy = SourceUvAnimationPolicy::RejectAnimated;
  double freeze_seconds = 0;
  bool animation_frozen = false;
  size_t image_bytes = 0, layer_bytes = 0, baked_bytes = 0, raster_samples = 0,
         sample_operations = 0, padded_texels = 0;
  std::vector<std::string> diagnostics;
};
// Pure bounded native worker operation. No file access, GL, source mutation,
// automatic flattening or live-project installation. Rasterizes exact face
// barycentrics, re-samples unfactored original slots, transforms normal frames
// and transfers every editable layer independently. Cancellation returns no
// partial result. The existing source reparameterize guard stays in effect
// until a caller explicitly adopts this reviewed bake result.
SourceUvBakeResult bake_source_uv(const SourceUvBakeInput &,
                                 const SourceUvBakeOptions & = {});
struct SourceUvAdoption {
  std::shared_ptr<PaintedMesh> paint;
  std::shared_ptr<const SourceMaterialBindings> material_bindings;
  std::shared_ptr<const DeformationState> deformation;
  std::shared_ptr<const CreatorAppearance> appearance;
};
// Revalidate a detached result and adopt both paint and source provenance.
// Caller publishes BOTH components only after whole-Scene resource validation.
SourceUvAdoption adopt_source_uv_bake(const SourceUvBakeResult &,
                                     const std::function<bool()> &cancelled = {});
} // namespace dome
