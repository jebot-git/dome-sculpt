// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "dome/paint.hpp"
#include "paint-source-coordinate.hpp"
#include <array>
#include <cstddef>
#include <span>

namespace dome {struct MToonTexture;struct PaintSourceTexture;}
namespace dome::detail {
class EmissionRasterDevice {
public:
  virtual ~EmissionRasterDevice() = default;
  using Pixel=std::array<float,10>;
  using Radiance=std::array<float,3>;
  using TextureSetPreview=std::array<float,4>; // RGB radiance, coverage
  virtual void read(size_t first,std::span<Pixel>,const std::function<bool()> &) = 0;
  virtual void read_finite(size_t first,std::span<Radiance>,const std::function<bool()> &) = 0;
  virtual void read_texture_set_preview(size_t first,std::span<TextureSetPreview>,const std::function<bool()> &) = 0;
};

std::unique_ptr<EmissionRasterDevice> create_emission_raster_device(
    const PaintedMesh &,std::span<const int32_t>,const PaintSourceTexture *,const MToonTexture *,
    std::array<float,3> background,float strength,const std::function<bool()> &);
std::unique_ptr<EmissionRasterDevice> create_finite_emission_raster_device(
    const PaintedMesh &,const PaintSourceTexture *,std::array<float,3> background,
    uint32_t resolution,float offset,const std::function<bool()> &);
std::unique_ptr<EmissionRasterDevice> create_texture_set_emission_device(
    const PaintedMesh &,const PaintSourceTexture *,const Image *aligned_mtoon,
    std::array<float,3> background,float strength,const std::function<bool()> &);
size_t emission_raster_staging_bytes(const PaintedMesh &) noexcept;

namespace emission_raster {
inline constexpr uint32_t absent=UINT32_MAX;
inline constexpr size_t chunk_pixels=65536, tile_pixels=PaintTile::size*PaintTile::size;
struct CommonPixel {uint8_t legacy[3],coverage,mask;};
struct Entry {uint32_t common=absent,physical=absent;};
struct PackedField {
  uint32_t constants[7]{},mask=0,stride=0,offset=0;
};
struct Layer {std::array<uint32_t,256> opacity{};};
struct PhysicalTile {
  PackedField field;
  std::span<const float> samples;
};
struct Bake {
  PackedField packed;
  const uint32_t *dense=nullptr,*varying=nullptr,*coverage_bits=nullptr;
  const uint8_t *legacy=nullptr,*coverage=nullptr;
  bool physical=false;
};
struct Source {
  paint_compute::SourceCoordinateJob coordinates;
  const uint8_t *rgba=nullptr;
  const float *aligned_mtoon_linear=nullptr;
  uint32_t sampler_affine[6]{};
  bool typed_transform=false;
};
struct Input {
  std::span<const int32_t> padding;
  std::vector<Layer> layers;
  std::vector<Entry> entries;
  std::vector<const PaintTile *> common;
  std::vector<PhysicalTile> physical;
  BakedEmissionField::StorageView bake;
  std::span<const uint8_t> legacy_bake,bake_coverage;
  std::span<const uint8_t> source;
  std::span<const float> aligned_mtoon_linear;
  std::vector<paint_compute::SourceTerms> columns,rows;
  Source source_sampler;
  std::array<uint32_t,3> background{};
  uint32_t strength=0,resolution=0;
  std::vector<SourceTextureAxis> finite_axis;
  bool physical_bake=false,texture_set=false;
};
std::unique_ptr<EmissionRasterDevice> upload(const Input &,const std::function<bool()> &);
}
}
