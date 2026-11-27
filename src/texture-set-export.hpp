// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "texture-set-surface.hpp"
#include "emission-fallback-metrics.hpp"
#include <optional>
#include "dome/tangents.hpp"
#include "source-morph.hpp"
namespace dome { class MToonMaterial; }
namespace dome::detail {
struct TextureSetExportEmission {
  Image image;
  std::array<float,3> factor{};
  float strength=1;
  ResolvedFallbackMetrics measured;
  std::optional<double> filter_error;
};
bool texture_set_physical_emission(const TextureSetView &);
struct EmissionExportRaster;
EmissionExportRaster texture_set_emission_raster(const TextureSetView &,const TextureSetLimits &,bool authored_mtoon);
struct TextureSetExportField {
  TextureSetId set=0;
  std::string name;
  size_t first=0,count=0;
  Material material;
  std::shared_ptr<const TextureMaps> textures;
  bool normal_role=false, tangent_attribute=false;
  bool portable_mtoon_bake_frame=false, portable_mtoon_derivative_frame=false;
  bool portable_mtoon_normal_coordinates=false;
  bool independent_mtoon_paint=false;
  bool scaled_mtoon_alpha=false;
  std::optional<TextureSetExportEmission> emission;
};
struct PortableMToonSetPaint {
  std::shared_ptr<const Image> color, shade, emission, normal;
  float alpha_factor=1;
  // Max component errors for clamp-to-edge bilinear reads in linear units.
  double color_error=0, shade_error=0, shade_interpolation_error=0,
      alpha_error=0, emission_error=0, source_filter_error=0;
  double normal_error_radians=0;
};
class RetopoMToonExport {
public:
  const TextureSetView &field() const noexcept { return field_; }
  static std::shared_ptr<const RetopoMToonExport> capture(const SculptObject &,
      const std::function<bool()> &);
  void authenticate(const ObjectSurface &,const TextureSetLimits &) const;
private:
  RetopoMToonExport()=default;
  uint64_t object_=0;
  TextureSetView field_;
};
std::optional<PortableMToonSetPaint> compile_mtoon_set_paint(
    const TextureSetView &,const TextureSetExportField &,const MToonMaterial &,
    const TextureSetLimits &);
std::optional<PortableMToonSetPaint> compile_retopo_mtoon_paint(
    const TextureSetView &,const MToonMaterial &,TextureSetLimits &remaining);
struct MToonFloatPreview {
  TextureSetExportEmission emission;
  Image metal_rough;
  PortableMToonSetPaint paint;
};
std::optional<MToonFloatPreview> compile_editable_mtoon_float_preview(
    const TextureSetView &,const MToonMaterial &,TextureSetLimits &remaining);
// One REST geometry and deformation expansion for the entire mesh instance.
// Material ranges refer into one index stream; no per-material mesh/rig copies.
struct TextureSetExport {
  ObjectSurface surface;
  std::vector<Tangent> tangents;
 std::vector<SourceMorphChannels> morph_channels;
  std::vector<std::array<float,2>>paint_coordinates,normal_coordinates;
  std::vector<TextureSetExportField> fields;
  size_t working_bound=0;
};
size_t texture_set_export_working_bound(const ObjectSurface &,
    const TextureSetLimits & = {}, bool measured_physical=false, bool editable_materials=false);
TextureSetExport compile_texture_set_export(const ObjectSurface &,
    const TextureSetLimits & = {}, bool measured_physical=false, bool editable_materials=false);
std::optional<TextureSetExport> compile_retopo_mtoon_export(const ObjectSurface &,
    const TextureSetLimits &);
} // namespace dome::detail
