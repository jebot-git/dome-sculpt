// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mtoon.hpp"
#include "source-maps.hpp"
#include <optional>
namespace dome {
class PaintedMesh;
class CreatorAssetArchive;
struct CreatorAppearance;
enum class SourceUvAnimationPolicy { RejectAnimated, FreezeAtTime };
// Canonical Mikk normals belong to PaintedMesh::baked(), not this role table.
enum class SourceMaterialNormalFrame { UvDerivative };
struct SourceNativeMaterial {
  // Native inspector factors at the start of this explicit material binding.
  // These are a comparison baseline, not factors baked into role pixels.
  std::array<float,4> color{.58f,.30f,.15f,1};
  std::array<float,3> emission{};
  int alpha = 1; // Native Material::AlphaMode::Opaque.
  float alpha_cutoff = .5f;
  bool double_sided = false;
  void validate() const;
};
struct SourceUvOrigin {
  std::shared_ptr<const PaintedMesh> paint;
  std::shared_ptr<const CreatorAppearance> appearance;
  std::optional<MToonMaterial> material;
  std::array<PaintSourceTexture, size_t(MToonSlot::Count)> roles;
};
struct SourceMaterialBindings {
  // A generic PBR UV bake uses this holder only for immutable source provenance.
  bool has_mtoon = true;
  MToonMaterial material;
  // Typed material owns UV selection, affine transforms and animation. Views
  // hold images/samplers only: identity affine and texcoord zero.
  std::array<PaintSourceTexture, size_t(MToonSlot::Count)> roles;
  SourceMaterialNormalFrame normal_frame = SourceMaterialNormalFrame::UvDerivative;
  SourceUvAnimationPolicy animation_policy = SourceUvAnimationPolicy::RejectAnimated;
  double freeze_seconds = 0;
  bool animation_frozen = false;
  SourceNativeMaterial native_reference;
  // First original source, propagated unchanged through repeated UV edits.
  // PaintedMesh has no material holder, so this ownership cannot recurse.
  std::shared_ptr<const SourceUvOrigin> uv_origin;
  void validate() const;
};
using SourceUvMaterialBindings = SourceMaterialBindings;
// One bounded native archive deduplicates immutable holders and first-source
// origins by identity; all role/source images use the document image archive.
class SourceMaterialArchive {
public:
  uint32_t add(std::shared_ptr<const SourceMaterialBindings>);
  const std::shared_ptr<const SourceMaterialBindings> &at(uint32_t) const;
  const std::vector<std::shared_ptr<const SourceMaterialBindings>> &bindings() const { return bindings_; }
  const std::vector<std::shared_ptr<const SourceUvOrigin>> &origins() const { return origins_; }
  nlohmann::json encode(PaintImageArchive &, CreatorAssetArchive &) const;
  static SourceMaterialArchive decode(const nlohmann::json &, const PaintImageArchive &, const CreatorAssetArchive &);
private:
  std::vector<std::shared_ptr<const SourceMaterialBindings>> bindings_;
  std::vector<std::shared_ptr<const SourceUvOrigin>> origins_;
};
} // namespace dome
