// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once

#include "dome/paint.hpp"
#include "paint-composite-kernel.hpp"
#include "paint-source-coordinate.hpp"
#include "source-uv-sampler.hpp"
#include <array>
#include <cstddef>
#include <optional>
#include <span>

namespace dome {struct MToonTexture;struct PaintSourceTexture;}
namespace dome::detail {
class SourceMaterialRaster {
public:
  using Roles=std::array<const PaintSourceTexture *,paint_compute::SourceRoleCount>;
  using Coordinates=std::function<void(unsigned,std::span<SourceUvSampler::Coordinate>)>;
  virtual ~SourceMaterialRaster()=default;
  virtual size_t capacity() const noexcept=0;
  virtual void read_material(std::span<const uint32_t>,const Coordinates &,
                             std::span<paint_compute::Output>,const std::function<bool()> &)=0;
};

std::unique_ptr<SourceMaterialRaster> create_source_material_raster(
    const PaintedMesh &,const Material &,const SourceMaterialRaster::Roles &,
    float occlusion_strength,uint32_t resolution,size_t working_bytes,const std::function<bool()> &,
    bool include_emission=true,std::optional<std::array<float,3>> shared_shade={});

class EmissionRasterDevice {
public:
  virtual ~EmissionRasterDevice() = default;
  using Pixel=std::array<float,10>;
  using Radiance=std::array<float,3>;
  struct MappedSample { uint32_t texel; float u,v; };
  struct BakeUvSample { uint32_t texels[4]{}; float weights[4]{}; };
  static constexpr size_t mapped_batch=16384;
  using TextureSetPreview=std::array<float,4>; // RGB radiance, coverage
  virtual void read(size_t first,std::span<Pixel>,const std::function<bool()> &) = 0;
  virtual void read_finite(size_t first,std::span<Radiance>,const std::function<bool()> &) = 0;
  virtual void read_mapped(std::span<const MappedSample>,std::span<Radiance>,const std::function<bool()> &) = 0;
  virtual void read_mapped_field(std::span<const MappedSample>,std::span<Pixel>,const std::function<bool()> &) = 0;
  virtual void read_bake_uv(std::span<const BakeUvSample>,std::span<BakedEmissionSample>,const std::function<bool()> &) = 0;
  virtual void read_texture_set_preview(size_t first,std::span<TextureSetPreview>,const std::function<bool()> &) = 0;
};

std::unique_ptr<EmissionRasterDevice> create_emission_raster_device(
    const PaintedMesh &,std::span<const int32_t>,const PaintSourceTexture *,const MToonTexture *,
    std::array<float,3> background,float strength,const std::function<bool()> &);
std::unique_ptr<EmissionRasterDevice> create_finite_emission_raster_device(
    const PaintedMesh &,const PaintSourceTexture *,std::array<float,3> background,
    uint32_t resolution,float offset,const std::function<bool()> &);
std::unique_ptr<EmissionRasterDevice> create_mapped_emission_raster_device(
    const PaintedMesh &,const PaintSourceTexture *,std::array<float,3> background,uint32_t resolution,
    size_t capacity,const std::function<bool()> &);
std::unique_ptr<EmissionRasterDevice> create_mapped_emission_field_device(
    const PaintedMesh &,const PaintSourceTexture &,std::array<float,3> background,float legacy_strength,size_t capacity,
    const std::function<bool()> &);
std::unique_ptr<EmissionRasterDevice> create_bake_uv_emission_device(const BakedEmissionField &,size_t capacity,
    const std::function<bool()> &);
std::unique_ptr<EmissionRasterDevice> create_texture_set_emission_device(
    const PaintedMesh &,const PaintSourceTexture *,const Image *aligned_mtoon,
    std::array<float,3> background,float strength,const std::function<bool()> &);
size_t emission_raster_staging_bytes(const PaintedMesh &) noexcept;

namespace emission_raster {
inline constexpr uint32_t absent=UINT32_MAX;
inline constexpr size_t chunk_pixels=65536, tile_pixels=PaintTile::size*PaintTile::size;
struct CommonPixel {uint8_t legacy[3],coverage,mask;};
struct Entry {uint32_t common=absent,physical=absent;};
struct MaterialEntry {uint32_t common=absent,pbr=absent;};
struct MaterialInput {
  SourceMaterialRaster::Roles roles;
  std::vector<MaterialEntry> entries;
  std::vector<const PaintTile *> common;
  std::vector<const PaintColorTile *> pbr;
  std::span<const uint8_t> bake_color,bake_orm,bake_coverage;
  paint_compute::Parameters factors;
  std::array<float,3> shade_factor{};
  float occlusion_strength=1;
  size_t capacity=0,largest_image=0;
  bool resident=false,shared_shade=false;
};
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
  const PaintSourceTexture *mapped_source=nullptr;
  std::array<uint32_t,3> background{};
  uint32_t strength=0,resolution=0;
  std::vector<SourceTextureAxis> finite_axis;
  size_t mapped_capacity=0,bake_uv_capacity=0;
  bool physical_bake=false,texture_set=false,mapped_field=false;
};
std::unique_ptr<EmissionRasterDevice> upload(const Input &,const std::function<bool()> &);
std::unique_ptr<SourceMaterialRaster> upload_material(const Input &,const MaterialInput &,
                                                        const std::function<bool()> &);
}
}
