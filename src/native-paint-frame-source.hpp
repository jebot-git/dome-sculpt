// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native-paint-sampling.hpp"
#include "normal-bake-frames.hpp"
#include "normal-key-derivation.hpp"
namespace dome::detail {
// An independent normal capture made from the actual current native paint
// geometry. The first writable-paint UV origin remains in before_material().
class NativePaintFrameSource final {
  std::shared_ptr<const PaintedMesh> paint_;
  std::shared_ptr<const SourceMaterialBindings> material_;
  std::shared_ptr<const SourceUvOrigin> origin_;
  std::shared_ptr<const NormalFrameSet> base_,frames_;
  std::shared_ptr<const SourceKeyDerivation> keys_;
  std::weak_ptr<const NativePaintFrameSource> self_;
  // Private factory certificate: bounded public frame scalars and complete
  // owning role/domain identities. Geometry arrays remain immutable data.
  std::vector<uint64_t> metadata_;
  std::array<std::shared_ptr<const NormalFrames>,6> role_owners_;
  std::array<std::shared_ptr<const FramePositionDomain>,2> domain_owners_;
  void seal_metadata();
  void check_metadata() const;

  NativePaintFrameSource()=default;
  NativePaintFrameSource(const NativePaintFrameSource&)=delete;
  friend class NativePaintFrameCodec;
  friend std::shared_ptr<const NativePaintFrameSource> capture_native_paint_frames(
      const SculptObject &,const NormalBakeFrameOptions &);
  friend std::shared_ptr<const NativePaintFrameSource> rebind_native_paint_frames(
      const std::shared_ptr<const NativePaintFrameSource> &,
      std::shared_ptr<const NormalFrameSet>,std::shared_ptr<const SourceKeyDerivation>,
      const std::function<bool()> &);
public:
  const auto &before_paint() const noexcept{return paint_;}
  const auto &before_material() const noexcept{return material_;}
  const auto &origin() const noexcept{return origin_;}
  const auto &base_frames() const noexcept{return base_;}
  const auto &frames() const noexcept{return frames_;}
  const auto &keys() const noexcept{return keys_;}
  size_t metadata_bytes() const noexcept{return metadata_.capacity()*sizeof(uint64_t);}
  bool valid_owner(const std::shared_ptr<const NativePaintFrameSource>&) const noexcept;
  void validate(bool deep=true,const std::function<bool()> &cancelled={}) const;
  void validate_binding(const SourceMaterialBindings &,bool deep=true,
      const std::function<bool()> &cancelled={}) const;
};
std::shared_ptr<const NativePaintFrameSource> capture_native_paint_frames(
    const SculptObject &,const NormalBakeFrameOptions & = {});
std::shared_ptr<const NativePaintFrameSource> rebind_native_paint_frames(
    const std::shared_ptr<const NativePaintFrameSource> &,
    std::shared_ptr<const NormalFrameSet>,std::shared_ptr<const SourceKeyDerivation>,
    const std::function<bool()> & = {});
// Called only after actual key frame/recipe production, before material validation.
void seal_native_paint_frames(SourceMaterialBindings &,
    const std::function<bool()> & = {});
class NativePaintFrameCodec {
public:
  static std::shared_ptr<const NativePaintFrameSource> replay(
      std::shared_ptr<const PaintedMesh>,std::shared_ptr<const SourceMaterialBindings>,
      std::shared_ptr<const SourceUvOrigin>,std::shared_ptr<const NormalFrameSet>,
      std::shared_ptr<const NormalFrameSet>,
      std::shared_ptr<const SourceKeyDerivation>,const std::function<bool()> & = {});
};
} // namespace dome::detail
