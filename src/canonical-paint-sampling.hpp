// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "canonical-paint-uv.hpp"
#include "dome/polygon-scene.hpp"
namespace dome::detail {
class CanonicalPaintSampling;
struct CanonicalPaintTransition {
 std::shared_ptr<const CanonicalPaintSampling> owner;
 std::shared_ptr<PaintedMesh> paint;
 std::shared_ptr<const DeformationState> deformation;
 std::shared_ptr<const SourceMaterialBindings> binding;
};
// Immutable, independently owned writable-grid correspondence. This is not a
// native32 source, a normal-field receipt, or permission to publish a Scene.
// The material/Scene37 archive must own and replay this complete transition.
class CanonicalPaintSampling final {
public:
 static CanonicalPaintTransition create(const Scene &,uint64_t,const Mesh &,
     std::span<const uint32_t>,const PaintSamplingLimits & = {});
 bool valid_owner(const std::shared_ptr<const CanonicalPaintSampling> &) const noexcept;
 bool matches_current(const PaintedMesh &) const noexcept;
 void validate(const PaintSamplingLimits & = {},bool deep=true) const;
 void validate_binding(const SourceMaterialBindings &,bool deep=true) const;
 void validate_current(const PaintedMesh &,const PaintSamplingLimits & = {},bool deep=true) const;
 const auto &before() const noexcept{return before_;}
 const auto &normal_uv() const noexcept{return normal_uv_;}
 const auto &attachment() const noexcept{return attachment_;}
 const auto &current_sources() const noexcept{return current_sources_;}
private:
 CanonicalPaintSampling()=default;
 CanonicalPaintSampling(const CanonicalPaintSampling&)=delete;
 CanonicalPaintSampling &operator=(const CanonicalPaintSampling&)=delete;
 std::weak_ptr<const CanonicalPaintSampling> self_;
 std::shared_ptr<const PolygonOrigin> before_;
 std::shared_ptr<const CanonicalNormalUv> normal_uv_;
 std::shared_ptr<const PaintSamplingAttachment> attachment_;
 std::vector<uint32_t> current_sources_;
};
}
