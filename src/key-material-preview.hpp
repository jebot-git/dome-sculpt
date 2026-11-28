// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstdint>
#include <functional>
#include <memory>

namespace dome {
class Document;
struct SurfaceRange;
struct SourceMaterialBindings;
struct SourceColorPreparation;
struct SourceNormalPreparation;
struct NormalFrameSet;
struct PaintNormalProjection;
struct PaintColorProjection;
struct BakedTextures;
struct TextureMaps;

namespace detail {
class SourceKeyDerivation;
class PolygonSurfacePreview;

// Worker validation of the actual immutable material and packed texture
// owners in one preview range. The proof never enters the Scene archive.
class KeyMaterialPreview final {
public:
  void authenticate(const SurfaceRange &) const;

private:
  friend class dome::Document;
  friend class SourceKeyDerivation;
  static const std::function<bool()> &validation_cancellation() noexcept;
  static std::shared_ptr<const KeyMaterialPreview> prepare(const SurfaceRange &,
      const std::function<bool()> &cancelled);
  explicit KeyMaterialPreview(const SurfaceRange &);
  bool matches(const SurfaceRange &) const noexcept;

  uint64_t object_;
  std::shared_ptr<const SourceMaterialBindings> binding_;
  std::shared_ptr<const TextureMaps> maps_;
  std::shared_ptr<const SourceKeyDerivation> key_;
  std::shared_ptr<const NormalFrameSet> frames_;
  std::shared_ptr<const SourceNormalPreparation> normals_;
  std::shared_ptr<const SourceColorPreparation> colors_;
  std::shared_ptr<const PaintNormalProjection> normal_projection_;
  std::shared_ptr<const PaintColorProjection> color_projection_;
  std::shared_ptr<const BakedTextures> bake_;
  std::shared_ptr<const PolygonSurfacePreview> polygon_;
};
}
}
