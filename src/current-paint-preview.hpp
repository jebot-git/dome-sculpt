// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "paint-overlay-packet.hpp"
namespace dome {struct SurfaceRange;struct TextureSetView;}
namespace dome::detail {
// Render-only current-grid interpretation. This is neither a Source4 packet
// nor a native-emission role, and it never replaces editable/persisted data.
class CurrentGridPaintPreview {
  friend struct PaintOverlayFactory;
  std::shared_ptr<const PaintOverlayPacket> layers_;
  std::shared_ptr<const Mesh> geometry_;
  std::shared_ptr<const CurrentGridPaintPreview> contribution_owner_;
  uint64_t object_=0;
  std::array<float,9> factors_{}; // linear RGBA, emission, metallic, roughness
  std::vector<bool> transparent_tiles_;
  bool transparent_=false,physical_=false;
  mutable std::weak_ptr<const CurrentGridPaintPreview> self_;
  CurrentGridPaintPreview()=default;
public:
  static bool eligible(const SculptObject &) noexcept;
  static bool eligible_field(uint64_t,const TextureSetView &) noexcept;
  static std::shared_ptr<const TextureMaps> for_render(const SculptObject &,
      const std::function<bool()> &cancelled={});
  static std::shared_ptr<const TextureMaps> for_field(uint64_t,const TextureSetView &,
      const std::function<bool()> &cancelled={});
  void authenticate_range(const Mesh &,const SurfaceRange &,bool check_coordinates)const;
  void authenticate_owner(const std::shared_ptr<const CurrentGridPaintPreview> &)const;
  bool matches_material(const Material &)const noexcept;
  const auto &layers()const noexcept{return layers_;}
  const auto &geometry()const noexcept{return geometry_;}
  const auto &factors()const noexcept{return factors_;}
  uint64_t object()const noexcept{return object_;}
  const auto &contribution_owner()const noexcept{return contribution_owner_;}
  bool transparent()const noexcept{return transparent_;}
  bool physical_emission()const noexcept{return physical_;}
  // Exact native alpha classification/sampling only, never a CPU RGB/ORM image.
  float alpha_texel(uint32_t x,uint32_t y)const;
  float sample_alpha(float u,float v)const;
};
} // namespace dome::detail
