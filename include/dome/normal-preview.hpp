// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "normal-preparation.hpp"
namespace dome {
// Runtime-only proof for an actual internally generated immutable paint preview.
// It authorizes normal interpretation only; colour/source geometry remains a
// separate owning contract. No maps/material/normal node owns this object.
class NormalPreviewProof {
public:
  bool matches(const std::shared_ptr<const SourceMaterialBindings> &,
               const std::shared_ptr<const TextureMaps> &) const noexcept;
  const std::shared_ptr<const TextureMaps> &maps()const noexcept{return maps_;}
  const std::shared_ptr<const PaintSamplingDomain> &metadata_domain()const noexcept{return source_.sampling_domain->metadata_sampling_domain;}
private:
  NormalPreviewProof(std::shared_ptr<const SourceMaterialBindings>,std::shared_ptr<const TextureMaps>);
  std::shared_ptr<const SourceMaterialBindings> binding_;
  std::shared_ptr<const TextureMaps> maps_;
  std::shared_ptr<const SourceNormalPreparation> recipe_;
  SourceNormalPreparation source_;
  std::shared_ptr<const PaintNormalProjection> maps_projection_;
  std::shared_ptr<const BakedTextures> baked_;
  friend std::shared_ptr<const NormalPreviewProof> prepare_normal_preview(
      const PaintedMesh &,std::shared_ptr<const SourceMaterialBindings>,const Material &,
      const std::function<bool()> &);
};
// Worker-only. Obtain the map from Painter's private immutable cache, replay
// its normal/event association, then mint the proof; never trust a supplied
// public TextureMaps copy. Null keeps all historical validation paths active.
std::shared_ptr<const NormalPreviewProof> prepare_normal_preview(const PaintedMesh &,
    std::shared_ptr<const SourceMaterialBindings>,const Material &,
    const std::function<bool()> &cancelled={});
} // namespace dome
