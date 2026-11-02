// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "material-bindings.hpp"
#include "scene.hpp"
namespace dome {
enum class SourceColorField { OriginalAccessor, Identity, PolygonCorners };
struct PolygonColorSurface;
struct SourceColorUvReprojection;
struct ColorPreparationOptions {
  uint64_t max_quality_cells=128'000'000, max_probe_samples=64'000'000;
  size_t max_working_bytes=256*1024*1024, max_layer_bytes=128*1024*1024;
  // A bound on the vertex-color field's projection, not a promise about the
  // complete material's texture resampling, mip selection or lighting.
  float max_projection_error=.04f;
  bool resolve_identity=false;
  std::function<bool()> cancelled;
};
struct ColorPreparationReceipt {
  uint32_t version=1, resolution=0;
  SourceColorField source_field=SourceColorField::OriginalAccessor;
  SourceNativeMaterial quality_material;
  std::array<float,4> projection_error_bound{}, observed_base_error{};
  // The explicit accepted tolerance follows this reversible conversion through
  // later UV edits; the measured initial bound itself remains immutable.
  float accepted_projection_error=.04f;
  uint64_t projected_texels=0, tested_quality_cells=0, probe_samples=0,
           observed_mask_disagreements=0;
  // Observed complete base-color errors are explicit LOD0 samples; they do
  // not establish equality for all filtering derivatives or camera poses.
  void validate() const;
  nlohmann::json encode() const;
  static ColorPreparationReceipt decode(const nlohmann::json &);
};
struct SourceColorPreparation {
  std::shared_ptr<const PaintColorProjection> projection;
  // Initial conversion quality/provenance stays immutable if a later UV edit
  // regenerates the current projection in a different atlas domain.
  std::shared_ptr<const PaintColorProjection> initial_projection;
  ColorPreparationReceipt receipt;
  // Exact preconversion authoring state, including its current UVs/edits.
  // The first raw creator/UV origin remains separately in the material holder.
  std::shared_ptr<const PaintedMesh> original_paint;
  std::shared_ptr<const CreatorAppearance> original_appearance;
  std::shared_ptr<const DeformationState> original_deformation;
  // Must not itself contain a color preparation: bounded acyclic ownership.
  std::shared_ptr<const SourceMaterialBindings> original_material;
  // Initial geometry-only CORNER authority. This is not an Origin/material
  // edge; current_descriptor() selects the separately owned later UV domain.
  std::shared_ptr<const PolygonColorSurface> polygon_colors;
  // Scene23 only: initial descriptor/receipt stay immutable; this independently
  // owned node proves the current UV domain and its newly measured projection.
  std::shared_ptr<const SourceColorUvReprojection> uv_reprojection;
  const std::shared_ptr<const PolygonColorSurface> &current_descriptor() const noexcept;
  void validate(bool deep=true) const;
  void validate_current(const Mesh &,const CreatorAppearance *,
                        const std::function<bool()> &cancelled={}) const;
  std::vector<std::array<float,4>> vertex_field(size_t,const CreatorAppearance *,
                         const std::function<bool()> &cancelled={}) const;
};
struct ColorUvReprojectionReceipt {
  uint32_t version=1,resolution=0;
  float accepted_projection_error=.04f;
  std::array<float,4> projection_error_bound{};
  uint64_t projected_texels=0,tested_quality_cells=0;
  uint64_t raster_samples=0,raster_budget=128'000'000;
  uint64_t max_quality_cells=128'000'000,max_working_bytes=256*1024*1024;
  void validate() const;
  nlohmann::json encode() const;
  static ColorUvReprojectionReceipt decode(const nlohmann::json &);
};
struct SourceColorUvLayout {
  std::vector<uint32_t> source_vertices,indices;
  std::vector<std::array<float,2>> uv;
  std::vector<uint8_t> pins,seams;
  size_t storage_bytes() const;
  Mesh target_mesh(const Mesh &,const std::function<bool()> & = {}) const;
  nlohmann::json encode() const;
  static size_t encoded_storage_bytes(const nlohmann::json &);
  static std::shared_ptr<const SourceColorUvLayout> decode(const nlohmann::json &);
};
struct SourceColorUvReprojection {
  ColorUvReprojectionReceipt receipt;
  std::shared_ptr<const SourceColorUvLayout> layout;
  std::shared_ptr<const PolygonColorSurface> polygon_colors;
  std::shared_ptr<const PaintColorProjection> projection;
  std::shared_ptr<const PaintedMesh> before_paint;
  std::shared_ptr<const CreatorAppearance> before_appearance;
  std::shared_ptr<const DeformationState> before_deformation;
  std::shared_ptr<const SourceMaterialBindings> before_material;
  void validate(bool deep=true,const std::function<bool()> & = {}) const;
  void validate_current(const Mesh &,const CreatorAppearance *,
                        const std::function<bool()> & = {}) const;
};
struct ColorPreparationInput {
  std::shared_ptr<const PaintedMesh> paint;
  std::shared_ptr<const CreatorAppearance> appearance;
  std::shared_ptr<const SourceMaterialBindings> material_bindings;
  std::shared_ptr<const DeformationState> deformation;
  Material material;
  std::shared_ptr<const PolygonColorSurface> polygon_colors;
};
struct ColorPreparationResult {
  std::shared_ptr<PaintedMesh> paint;
  std::shared_ptr<const SourceMaterialBindings> material_bindings;
  // False for absent/identity source colors or an already prepared input.
  bool converted=false;
};
// Detached native operation: retains all original/common paint channels and
// supplies a separately resolved PBR color plane. No division by vertex color,
// source mutation, GL readback or automatic flattening of editable layers.
ColorPreparationResult prepare_source_colors(const ColorPreparationInput &,
                                             const ColorPreparationOptions & = {});
nlohmann::json encode_color_projection(const PaintColorProjection &,PaintImageArchive &);
std::shared_ptr<const PaintColorProjection> decode_color_projection(const nlohmann::json &,const PaintImageArchive &);
} // namespace dome
