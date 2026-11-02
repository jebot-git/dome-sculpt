// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <memory>
#include <vector>
#include <span>
#include <nlohmann/json_fwd.hpp>
namespace dome {
struct Image;
struct Mesh;
struct Vec3;
struct PaintImageLimits {
  size_t max_image_pixels = 16 * 1024 * 1024;
  size_t max_unique_pixels = 64 * 1024 * 1024;
  size_t max_images = 256;
};
// Native glTF sampler constants; source filtering metadata is retained exactly.
struct PaintSourceTexture {
  std::shared_ptr<const Image> image;
  int wrap_s = 10497, wrap_t = 10497, min_filter = 0, mag_filter = 0;
  int texcoord = 0;
  // u'=a*u+b*v+tx; v'=c*u+d*v+ty. Image row zero is v=0.
  std::array<float,6> uv_transform{1,0,0,1,0,0};
  void validate(unsigned coordinate_count=1) const;
};
struct PaintSourceMaps {
  PaintSourceTexture color, metal_rough, emission, normal, occlusion;
  float normal_scale = 1, occlusion_strength = 1;
  // Immutable TEXCOORD_1 in the exact current mesh vertex domain. Presence
  // requires an actual UV1 image view; original asset UVs remain independent.
  std::shared_ptr<const std::vector<std::array<float,2>>> uv1;
  bool uses_uv1() const noexcept;
  void validate_surface(const Mesh &) const;
  void validate(const PaintImageLimits & = {}) const;
  size_t unique_pixels() const;
  bool empty() const noexcept;
};
// One document-wide archive deduplicates shared images without resampling them.
// Image storage must be immutable before submission; no mutable aliases may write.
class PaintImageArchive {
public:
  explicit PaintImageArchive(PaintImageLimits = {});
  uint32_t add(std::shared_ptr<const Image>);
  const std::shared_ptr<const Image> &at(uint32_t) const;
  size_t size() const noexcept { return images_.size(); }
  size_t pixels() const noexcept { return pixels_; }
  nlohmann::json encode() const;
  static PaintImageArchive decode(const nlohmann::json &, PaintImageLimits = {});
private:
  PaintImageLimits limits_;
  size_t pixels_ = 0;
  std::vector<std::shared_ptr<const Image>> images_;
};
nlohmann::json encode_source_maps(const PaintSourceMaps &, PaintImageArchive &);
std::shared_ptr<const PaintSourceMaps> decode_source_maps(const nlohmann::json &, const PaintImageArchive &);
// Resolve one explicit sampler against its owned geometry/coordinate domain.
// The ordinary sample_source_texture still rejects an unresolved UV1 view.
std::span<const std::array<float,2>> source_texture_coordinates(const Mesh &,const PaintSourceMaps &,int coordinate);
std::array<float,2> source_texture_uv(const Mesh &,const PaintSourceMaps &,int coordinate,size_t triangle,std::array<float,3> barycentric);
std::array<float,4> sample_surface_texture(const Mesh &,const PaintSourceMaps &,const PaintSourceTexture &,size_t triangle,std::array<float,3> barycentric,bool srgb);
// Bilinear interpolation decodes RGB to linear light when srgb=true; alpha and
// data channels remain linear. Source wrap/mirror/clamp and affine UV are applied.
std::array<float,4> sample_source_texture(const PaintSourceTexture &, float u, float v, bool srgb);
// Inputs must be validated once by the worker. Evaluate original normal views
// in the same affine-UV derivative basis as the raw-source GPU preview. A
// degenerate UV transform retains the interpolated geometric normal.
Vec3 source_texture_normal(const Mesh &, const PaintSourceMaps &, size_t triangle,
                           std::array<float,3> barycentric, float strength = 1);
} // namespace dome
