#include "texture-set-field-source.hpp"
#include "texture-set-paint-access.hpp"
#include "texture-set-native-grid.hpp"
#include "texture-set-resample.hpp"
#include "retopo-transfer.hpp"
#include "affine-surface.hpp"
#include "canonical-normal-uv.hpp"
#include "normal-bake-encoding.hpp"
#include "source-paint-parameters.hpp"
#include "paint-overlay-packet.hpp"
#include "baked-emission-access.hpp"
#include "source4-baked.hpp"
#include "baked-emission-codec.hpp"
#include "current-paint-preview.hpp"
#include "emission-paint-access.hpp"
#include "emission-authority.hpp"
#include "native-weight-paint-access.hpp"
#include "native-weight-state.hpp"
#include "paint-blur-gpu.hpp"
#include <numeric>
// SPDX-License-Identifier: GPL-3.0-or-later
// Ray/triangle picking adapted from Blender 5.4 Alpha isect_ray_tri_v3,
// GPL-2.0-or-later; see third_party/blender/math_geom.cc.reference.
#include "dome/paint.hpp"
#include "dome/source-paint.hpp"
#include "source-sampling-transport.hpp"
#include "key-paint-adoption.hpp"
#include "normal-frame-runtime.hpp"
#include "source-paint-pairing.hpp"
#include "paint-source-admission.hpp"
#include "paint-archive-shape.hpp"
#include "source-uv-archive.hpp"
#include "dome/paint-sampling.hpp"
#include "paint-sampling-access.hpp"
#include "native-writable-grid.hpp"
#include "native-read-scope.hpp"
#include "native-grid-composite-work.hpp"
#include "scene-paint-raster.hpp"
#include "scene-bake-archive.hpp"
#include "native-bake-conversion.hpp"
#include "native-paint-bake.hpp"
#include "native-paint-frame-source.hpp"
#include "dome/normal-preparation.hpp"
#include "polygon-paint.hpp"
#include "polygon-surface-paint.hpp"
#include "polygon-surface-preview.hpp"
#include "polygon-surface-archive.hpp"
#include "paint-composite-backend.hpp"
#include "dome/source-maps.hpp"
#include "source-texture-grid.hpp"
#include "dome/normal-sampling.hpp"
#include "dome/scene.hpp"
#include "dome/tangents.hpp"
#include "paint-color-access.hpp"
#include "paint-normal-access.hpp"
#include "baked-normal-field.hpp"
#include "baked-normal-archive.hpp"
#include "color-projection-quality.hpp"
#include <bit>
#include <cstring>
#include <nlohmann/json.hpp>
#include <png.h>
#include <optional>
#include <set>
#include <unordered_map>
#include <atomic>
#include <mutex>
#include <list>
namespace dome {
namespace {
uint8_t byte(float v) {
  return uint8_t(std::lround(std::clamp(v, 0.f, 1.f) * 255));
}
float unit(uint8_t v) { return float(v) / 255; }
bool emission_fallback_matches(const PaintEmissionValue &value,const PaintPixel &fallback) {
  if(byte(value.coverage)!=fallback.data[12])return false;
  for(size_t k=0;k<3;++k)if(byte(linear_to_srgb(value.legacy[k]))!=fallback.data[6+k])return false;
  return true;
}
void name_valid(const std::string &s) {
  if (s.empty() || s.size() > 256 || s.find('\0') != std::string::npos)
    throw std::invalid_argument("Invalid paint layer name");
}
size_t tile_count(const PaintLayer &layer) {
  return std::count_if(layer.tiles.begin(),layer.tiles.end(),[](const auto &tile){return bool(tile);});
}
size_t color_tile_count(const PaintLayer &layer) {
  return std::count_if(layer.pbr_tiles.begin(),layer.pbr_tiles.end(),[](const auto &tile){return bool(tile);});
}
size_t emission_tile_count(const PaintLayer &layer) {
  return std::count_if(layer.emission_tiles.begin(),layer.emission_tiles.end(),[](const auto &tile){return bool(tile);});
}
uint64_t next_composite_token() {
  static std::atomic<uint64_t> sequence{1};auto value=sequence.load(std::memory_order_relaxed);
  while(value!=UINT64_MAX)if(sequence.compare_exchange_weak(value,value+1,std::memory_order_relaxed))return value;
  throw std::overflow_error("Paint composite content identity exhausted");
}
struct CompositeFactors {
  std::array<float,4> color;
  std::array<float,3> emission;
  float metallic,roughness;
  bool operator==(const CompositeFactors &)const=default;
};
enum class CompositeUse { Standard, NativeEmission };
struct NativeEmissionOwner {
  std::shared_ptr<const Mesh> geometry;
  std::shared_ptr<const PaintSamplingDomain> domain;

  bool matches(const TextureMaps &maps) const {
    const auto same_owner = [](const auto &a, const auto &b) {
      return a == b && !a.owner_before(b) && !b.owner_before(a);
    };
    return maps.overlay && same_owner(maps.overlay->native_geometry(), geometry) &&
           same_owner(maps.overlay->native_domain(), domain);
  }
};
struct CompositeRetention {
  struct Source {size_t references=0,bytes=0;};
  struct Entry {std::shared_ptr<const TextureMaps> maps;uint64_t token=0;CompositeFactors factors{};CompositeUse use=CompositeUse::Standard;size_t derived=0,count=0;std::array<const Image*,8> sources{};const BakedTextures *base=nullptr;bool deferred_standard=false;};
  std::mutex mutex;
  size_t limit=512*1024*1024,bytes=0;
  uint64_t hits=0,builds=0;
  std::list<Entry> entries;
  std::unordered_map<const Image*,Source> sources;
  std::unordered_map<const BakedTextures*,Source> bases;
  void evict(){
    auto &e=entries.back();bytes-=e.derived;
    for(size_t i=0;i<e.count;++i){auto found=sources.find(e.sources[i]);if(!--found->second.references){bytes-=found->second.bytes;sources.erase(found);}}
    if(e.base){auto found=bases.find(e.base);if(!--found->second.references){bytes-=found->second.bytes;bases.erase(found);}}
    entries.pop_back();
  }
  std::shared_ptr<const TextureMaps> find(uint64_t token, const CompositeFactors &factors,
                                          const NativeEmissionOwner *native = nullptr) {
    const auto use = native ? CompositeUse::NativeEmission : CompositeUse::Standard;
    std::lock_guard lock(mutex);
    auto found = std::find_if(entries.begin(), entries.end(), [&](const Entry &entry) {
      return entry.token == token && entry.factors == factors && entry.use == use &&
             (!native || native->matches(*entry.maps));
    });
    if(found==entries.end())return {};
    auto result=found->maps;
    found->deferred_standard = false;
    entries.splice(entries.begin(),entries,found);++hits;return result;
  }
  void retain(const std::shared_ptr<const TextureMaps> &maps, uint64_t token,
              const CompositeFactors &factors, bool hit, CompositeUse use = CompositeUse::Standard) {
    std::lock_guard lock(mutex);if(hit)++hits;else ++builds;
    auto found=std::find_if(entries.begin(),entries.end(),[&](auto &e){return e.maps.get()==maps.get();});
    if(found!=entries.end()){found->deferred_standard=false;entries.splice(entries.begin(),entries,found);return;}
    Entry entry;entry.maps=maps;entry.token=token;entry.factors=factors;entry.use=use;entry.derived=(maps->overlay?maps->overlay->storage_bytes():0)+maps->color.rgba.size()+maps->metal_rough.rgba.size()+maps->emission.rgba.size()+maps->normal.rgba.size()+maps->mtoon_normal.rgba.size()+maps->color_coverage.rgba.size()+maps->baked_normal.rgba.size()+maps->baked_coverage.rgba.size()+maps->emission_coverage.rgba.size()+maps->pbr_color.rgba.size();
    entry.base=maps->baked_base.get();
    // The native packet's storage already accounts for its exact baked base.
    if (use == CompositeUse::NativeEmission && maps->overlay &&
        maps->overlay->baked().get() == entry.base)
      entry.base = nullptr;
    if(maps->source_maps)for(auto slot:{&maps->source_maps->color,&maps->source_maps->metal_rough,&maps->source_maps->emission,&maps->source_maps->normal,&maps->source_maps->occlusion})
      if(slot->image&&std::find(entry.sources.begin(),entry.sources.begin()+entry.count,slot->image.get())==entry.sources.begin()+entry.count)
        entry.sources[entry.count++]=slot->image.get();
    if(maps->color_projection && maps->color_projection->vertex_rgba && std::find(entry.sources.begin(),entry.sources.begin()+entry.count,maps->color_projection->vertex_rgba.get())==entry.sources.begin()+entry.count)
      entry.sources[entry.count++]=maps->color_projection->vertex_rgba.get();
    if(maps->normal_projection)for(const auto &image:{maps->normal_projection->pbr,maps->normal_projection->mtoon})
      if(image&&std::find(entry.sources.begin(),entry.sources.begin()+entry.count,image.get())==entry.sources.begin()+entry.count)entry.sources[entry.count++]=image.get();
    size_t isolated=entry.derived+(entry.base?entry.base->storage_bytes():0);for(size_t i=0;i<entry.count;++i)isolated+=entry.sources[i]->rgba.size();
    if(isolated>limit)return;
    auto increment=[&]{size_t n=entry.derived;if(entry.base&&!bases.contains(entry.base))n+=entry.base->storage_bytes();for(size_t i=0;i<entry.count;++i)if(!sources.contains(entry.sources[i]))n+=entry.sources[i]->rgba.size();return n;};
    while (!entries.empty() && (entries.size() >= 128 || increment() > limit || bytes > limit - increment())) {
      auto &oldest = entries.back();
      if (use == CompositeUse::Standard && oldest.use == CompositeUse::NativeEmission &&
          !oldest.deferred_standard) {
        // Give the expensive HDR preview one chance to serve adjacent history.
        // The caller still gets its ordinary frame. A later miss can evict HDR.
        oldest.deferred_standard = true;
        return;
      }
      evict();
    }
    size_t additional=increment();if(additional>limit||bytes>limit-additional)return;
    // Retention is best effort. A cache-node allocation failure must not turn a
    // successfully composited immutable frame into a document edit failure.
    try {
      entries.push_front(entry);
      try {for(size_t i=0;i<entry.count;++i)sources.try_emplace(entry.sources[i],Source{0,entry.sources[i]->rgba.size()});if(entry.base)bases.try_emplace(entry.base,Source{0,entry.base->storage_bytes()});}
      catch(...){for(size_t i=0;i<entry.count;++i){auto p=sources.find(entry.sources[i]);if(p!=sources.end()&&!p->second.references)sources.erase(p);}if(entry.base){auto p=bases.find(entry.base);if(p!=bases.end()&&!p->second.references)bases.erase(p);}entries.pop_front();throw;}
      bytes+=entry.derived;if(entry.base){auto &b=bases.at(entry.base);if(!b.references++)bytes+=b.bytes;}
      for(size_t i=0;i<entry.count;++i){auto &s=sources.at(entry.sources[i]);if(!s.references++)bytes+=s.bytes;}
    }catch(const std::bad_alloc&){}
  }
};
CompositeRetention &composite_retention(){static CompositeRetention cache;return cache;}
Vec3 low(Vec3 a, Vec3 b) {
  return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}
Vec3 high(Vec3 a, Vec3 b) {
  return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}
using BakedPixel = std::array<float, 13>;
Vec3 unpack_baked_normal(const Image &image,size_t at) {
  if(image.rgba.empty())return {0,0,1};
  const auto *pixel=image.rgba.data()+at*4;
  return {unit(pixel[0])*2-1,unit(pixel[1])*2-1,unit(pixel[2])*2-1};
}
BakedPixel baked_pixel(const BakedTextures &baked, size_t at) {
  auto &m = baked.maps;
  size_t i = at * 4;
  BakedPixel result{};
  for (int c = 0; c < 4; ++c) {
    result[c] = unit(m.color.rgba[i + c]);
    if (c < 3) result[c] = srgb_to_linear(result[c]);
  }
  for (int c = 0; c < 3; ++c) {
    result[4 + c] = unit(m.metal_rough.rgba[i + c]);
    result[7 + c] = baked.emission_field ? 0 : srgb_to_linear(unit(m.emission.rgba[i + c]));
  }
  const auto normal=unpack_baked_normal(m.normal,at);
  result[10]=normal.x;result[11]=normal.y;result[12]=normal.z;
  return result;
}
void baked_pixel(BakedTextures &baked, size_t at, BakedPixel values, bool write_normal=true) {
  auto &m = baked.maps;
  size_t i = at * 4;
  for (int c = 0; c < 4; ++c)
    m.color.rgba[i + c] = byte(c < 3 ? linear_to_srgb(values[c]) : values[c]);
  for (int c = 0; c < 3; ++c) {
    m.metal_rough.rgba[i + c] = byte(values[4 + c]);
    if (!baked.emission_field) m.emission.rgba[i + c] = byte(linear_to_srgb(values[7 + c]));
  }
  m.metal_rough.rgba[i + 3] = 255;
  if (!baked.emission_field) m.emission.rgba[i + 3] = 255;
  if (write_normal && !m.normal.rgba.empty()) {
    auto n = Vec3{values[10], values[11], values[12]};
    n = length(n) > 1e-8f ? normalized(n) : Vec3{0, 0, 1};
    m.normal.rgba[i] = byte(n.x * .5f + .5f);
    m.normal.rgba[i + 1] = byte(n.y * .5f + .5f);
    m.normal.rgba[i + 2] = byte(n.z * .5f + .5f);
    m.normal.rgba[i + 3] = 255;
  }
}
Image blank_image(uint32_t r, bool normal = false) {
  Image result{r, r, std::vector<uint8_t>(size_t(r) * r * 4)};
  if (normal)
    for (size_t i = 0; i < result.rgba.size(); i += 4) {
      result.rgba[i] = result.rgba[i + 1] = 128;
      result.rgba[i + 2] = result.rgba[i + 3] = 255;
    }
  return result;
}
} // namespace
struct PaintedMesh::CompositeCache {
  std::mutex mutex;
  uint64_t token=0;
  CompositeFactors factors{};
  std::weak_ptr<const TextureMaps> maps;
  uint64_t preview_token=0;
  bool source_eligible=false;
  std::weak_ptr<const TextureMaps> source_preview;
  uint64_t source_baked_token=0;
  std::weak_ptr<const detail::PaintOverlayPacket> source_baked_packet;
  bool source_baked_transparent=false;
  uint64_t native_emission_token=0;
  CompositeFactors native_emission_factors{};
  std::weak_ptr<const TextureMaps> native_emission_preview;
  uint64_t texture_set_bake_token=0;
  CompositeFactors texture_set_bake_factors{};
  std::weak_ptr<const TextureMaps> texture_set_bake_preview;
  std::weak_ptr<const detail::CurrentGridPaintPreview> current_grid_preview;
  using GridKey=std::tuple<uint64_t,uintptr_t,uintptr_t,std::array<uint32_t,9>>;
  struct GridEntry {std::weak_ptr<const detail::CurrentGridPaintPreview> owner;uint64_t used=0;};
  std::map<GridKey,GridEntry> current_grids;
  uint64_t current_grid_access=0;
};
PaintCompositeCacheStats paint_composite_cache_stats(){
  auto &cache=composite_retention();std::lock_guard lock(cache.mutex);
  return {cache.bytes,cache.entries.size(),cache.limit,cache.hits,cache.builds};
}
void set_paint_composite_cache_budget(size_t bytes){
  if(bytes>512*1024*1024)throw std::invalid_argument("Paint composite retention budget exceeds 512 MiB");
  auto &cache=composite_retention();std::lock_guard lock(cache.mutex);cache.limit=bytes;
  while(!cache.entries.empty()&&cache.bytes>bytes)cache.evict();
}
float srgb_to_linear(float v) {
  return v <= .04045f ? v / 12.92f : std::pow((v + .055f) / 1.055f, 2.4f);
}
float linear_to_srgb(float v) {
  v = std::max(0.f, v);
  return v <= .0031308f ? 12.92f * v : 1.055f * std::pow(v, 1 / 2.4f) - .055f;
}
void Image::validate() const {
  if (!width || !height || width > 8192 || height > 8192 ||
      rgba.size() != size_t(width) * height * 4)
    throw std::invalid_argument("Invalid texture image");
}
void PaintColorProjection::validate(uint32_t resolution) const {
  if(version!=1 || !vertex_rgba || !quality_cells || quality_cells>1'000'000'000)throw std::invalid_argument("Invalid prepared vertex-color projection");
  for(auto error:maximum_error)if(!std::isfinite(error) || error<0 || error>1)throw std::invalid_argument("Invalid vertex-color projection quality bound");
  vertex_rgba->validate();
  if(vertex_rgba->width!=resolution || vertex_rgba->height!=resolution)
    throw std::invalid_argument("Prepared vertex-color atlas differs from the paint surface");
}
std::vector<uint8_t> Image::png() const {
  validate();
  png_image image{};
  struct Release {
    png_image *image;
    ~Release() { png_image_free(image); }
  } release{&image};
  image.version = PNG_IMAGE_VERSION;
  image.width = width;
  image.height = height;
  image.format = PNG_FORMAT_RGBA;
  png_alloc_size_t size = 0;
  if (!png_image_write_to_memory(&image, nullptr, &size, 0, rgba.data(), 0,
                                 nullptr))
    throw std::runtime_error(image.message);
  std::vector<uint8_t> bytes(size);
  if (!png_image_write_to_memory(&image, bytes.data(), &size, 0, rgba.data(), 0,
                                 nullptr))
    throw std::runtime_error(image.message);
  bytes.resize(size);
  return bytes;
}
void PaintSettings::validate() const {
  auto valid = [](float f) { return std::isfinite(f) && f >= 0 && f <= 1; };
  if (int(channel) < 0 || int(channel) > 4 ||
      !std::all_of(color.begin(), color.end(), valid) ||
      (emission_units!=PaintEmissionUnits::MaterialRelative&&emission_units!=PaintEmissionUnits::Physical) ||
      !std::all_of(emission.begin(), emission.end(), [&](float f){return std::isfinite(f)&&f>=0&&f<=(emission_units==PaintEmissionUnits::Physical?maximum_native_emission:1);}) ||
      !valid(metallic) || !valid(roughness) || !valid(mask) || !valid(flow))
    throw std::invalid_argument("Invalid paint brush settings");
}
struct PaintedMesh::SurfaceMap {
  struct OriginalSampling {
    std::shared_ptr<const PaintedMesh> before;
    std::shared_ptr<const PaintSourceMaps> current_maps;
    std::shared_ptr<const BakedTextures> baked;
    std::shared_ptr<const std::vector<std::array<float,2>>> original_uv0,original_uv1,authored_uv1;
    std::vector<uint32_t> vertices;
  };
  // Geometry is detached from the raster. Different-resolution texture sets
  // retain one immutable authoring mesh rather than copying its vertex arrays.
  std::shared_ptr<const Mesh> geometry;
  const Mesh &mesh;
  std::shared_ptr<const OriginalSampling> original_sampling;
  std::shared_ptr<const PaintSamplingDomain> paint_domain;
  std::shared_ptr<const TextureSetFaceDomain> set_domain;
  bool sampling_raster_proven=false;
  bool source_capture_only=false;
  uint32_t resolution, columns;
  struct Texel {
    Vec3 position, normal;
    bool covered = false;
    uint32_t face = 0;
    std::array<float, 2> bary{};
  };
  struct Bounds {
    Vec3 lo{INFINITY, INFINITY, INFINITY}, hi{-INFINITY, -INFINITY, -INFINITY};
  };
  struct Extra { Texel texel; uint32_t next = UINT32_MAX; };
  struct Head { uint32_t first = UINT32_MAX, count = 0; };
  using Links = std::unordered_map<uint32_t,Head>;
  PaintSurfaceOptions options;
  PaintSurfaceDiagnostics diagnostics;
  struct Raster {
    Links linked;
    std::vector<Extra> extra;
    std::vector<Texel> texels;
    std::vector<Bounds> tiles;
    std::vector<int32_t> padding;
  };
  std::shared_ptr<const Raster> raster_data;
  const Links &linked;
  const std::vector<Extra> &extra;
  const std::vector<Texel> &texels;
  const std::vector<Bounds> &tiles;
  const std::vector<int32_t> &padding;
  static const Mesh &checked_geometry(const std::shared_ptr<const Mesh> &geometry) {
    if(!geometry)throw std::invalid_argument("Missing immutable paint geometry");
    return *geometry;
  }
  // The existing same-UV triangle correspondence permits raster reuse while
  // replacing the immutable vertex/index owner. A reference member must be
  // bound to that new owner at construction, never assigned through const.
  SurfaceMap(const SurfaceMap &before,std::shared_ptr<const Mesh> current)
      :SurfaceMap(before,std::move(current),before.paint_domain) {}
  SurfaceMap(const SurfaceMap &before,std::shared_ptr<const Mesh> current,
      std::shared_ptr<const PaintSamplingDomain> domain)
      :geometry(std::move(current)),mesh(checked_geometry(geometry)),
       original_sampling(before.original_sampling),paint_domain(std::move(domain)),set_domain(before.set_domain),
       sampling_raster_proven(bool(paint_domain)),source_capture_only(before.source_capture_only),
       resolution(before.resolution),columns(before.columns),options(before.options),diagnostics(before.diagnostics),
       raster_data(before.raster_data),linked(raster_data->linked),extra(raster_data->extra),texels(raster_data->texels),
       tiles(raster_data->tiles),padding(raster_data->padding) {}
  SurfaceMap(Mesh input, uint32_t r, PaintSurfaceOptions opts = {},
             std::shared_ptr<const PaintSamplingDomain> domain = {},
             PaintSamplingLimits domain_limits = {})
      : SurfaceMap(std::make_shared<const Mesh>(std::move(input)),r,std::move(opts),std::move(domain),std::move(domain_limits)) {}
  SurfaceMap(std::shared_ptr<const Mesh> input,uint32_t r,PaintSurfaceOptions opts = {},
             std::shared_ptr<const PaintSamplingDomain> domain = {},
             PaintSamplingLimits domain_limits = {},std::shared_ptr<const TextureSetFaceDomain> membership = {})
      : geometry(std::move(input)),mesh(checked_geometry(geometry)),paint_domain(std::move(domain)),set_domain(std::move(membership)), resolution(r), columns(r / 32),options(std::move(opts)),
        raster_data(std::make_shared<Raster>()),linked(raster_data->linked),extra(raster_data->extra),texels(raster_data->texels),
        tiles(raster_data->tiles),padding(raster_data->padding) {
    // A fresh raster is writable solely during construction. Published
    // SurfaceMaps may share its immutable sample and padding arrays.
    auto data=std::const_pointer_cast<Raster>(raster_data);
    auto &[linked,extra,texels,tiles,padding]=*data;
    tiles.resize(size_t(columns)*columns);
    if((options.policy!=PaintUvPolicy::Strict&&options.policy!=PaintUvPolicy::SourceAuthored&&options.policy!=PaintUvPolicy::PolygonAuthored)||
       !options.max_raster_samples||options.max_raster_samples>512*1024*1024||
       options.max_additional_candidates>8*1024*1024||!options.max_candidate_checks||options.max_candidate_checks>512*1024*1024||
       options.max_candidates_per_texel<1||options.max_candidates_per_texel>128)
      throw std::invalid_argument("Invalid paint surface resource bounds");
    if(set_domain&&!set_domain->matches(geometry))throw std::invalid_argument("Paint raster membership belongs to another immutable mesh");
    diagnostics.policy=options.policy;
    const bool source=options.policy!=PaintUvPolicy::Strict;
    mesh.validate();
    std::optional<Mesh> domain_raster;
    if(paint_domain)domain_raster=paint_domain->raster_mesh(mesh,domain_limits);
    const auto &raster=domain_raster?*domain_raster:mesh;
    if (raster.uv.size() != raster.positions.size())
      throw std::invalid_argument("Painting requires persisted UV coordinates");
    for (size_t vertex=0;vertex<raster.uv.size();++vertex) {
      if((vertex&4095)==0&&options.cancelled&&options.cancelled())
        throw std::runtime_error("Source paint surface construction cancelled");
      const auto uv=raster.uv[vertex];
      if (uv[0] < 0 || uv[0] > 1 || uv[1] < 0 || uv[1] > 1) {
        if(options.policy!=PaintUvPolicy::SourceAuthored||paint_domain)
          throw std::invalid_argument("Paint UVs must fit a single texture tile");
        // Match the existing source UV1/extra-set range. This is an explicit
        // admission bound, never a clamp of authored sampling coordinates.
        if(std::abs(uv[0])>10000||std::abs(uv[1])>10000)
          throw std::invalid_argument("Original UV0 coordinate exceeds the finite native bound");
        source_capture_only=true;
      }
    }
    if(source_capture_only) {
      // Raw source coordinates are valid for texture viewing and an initial
      // source-aware atlas conversion, but do not define a writable unit grid.
      // Never clamp them or fabricate partial coverage of repeated triangles.
      for(size_t face=0;face<raster.indices.size()/3;++face) {
        if((face&4095)==0&&options.cancelled&&options.cancelled())
          throw std::runtime_error("Source paint surface construction cancelled");
        if(!set_domain||set_domain->contains(uint32_t(face)))diagnostics.unpaintable_uv_faces.push_back(uint32_t(face));
      }
      options.cancelled={};
      return;
    }
    // Original-coordinate captures above have no writable raster. Keep only
    // their bounded tile slots for layer/pixel metadata; allocating these
    // dense grids would spend O(resolution^2) memory on permanently unused
    // storage for every retained source owner.
    texels.resize(size_t(r) * r);
    padding.assign(size_t(r) * r, -1);
    for (size_t k = 0; k < raster.indices.size(); k += 3) {
      if((k&4095)==0&&options.cancelled&&options.cancelled())throw std::runtime_error("Set paint raster cancelled");
      if(set_domain&&!set_domain->contains(uint32_t(k/3)))continue;
      uint32_t ids[] = {raster.indices[k], raster.indices[k + 1],
                        raster.indices[k + 2]};
      auto a = raster.uv[ids[0]], b = raster.uv[ids[1]], c = raster.uv[ids[2]];
      double den = double(b[1] - c[1]) * (a[0] - c[0]) +
                   double(c[0] - b[0]) * (a[1] - c[1]);
      if(source&&options.cancelled&&options.cancelled())throw std::runtime_error("Source paint surface construction cancelled");
      if (source?den==0:std::abs(den)<1e-14) {
        if(!source)throw std::invalid_argument("Painting requires nondegenerate UV triangles");
        diagnostics.unpaintable_uv_faces.push_back(uint32_t(k/3));continue;
      }
      int x0 = std::max(0, int(std::floor(std::min({a[0], b[0], c[0]}) * r))),
          x1 = std::min(int(r) - 1,
                        int(std::ceil(std::max({a[0], b[0], c[0]}) * r)));
      int y0 = std::max(0, int(std::floor(std::min({a[1], b[1], c[1]}) * r))),
          y1 = std::min(int(r) - 1,
                        int(std::ceil(std::max({a[1], b[1], c[1]}) * r)));
      for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
          if(++diagnostics.raster_samples>options.max_raster_samples)throw std::length_error("Paint UV raster work budget exceeded");
          if((diagnostics.raster_samples&4095)==0&&options.cancelled&&options.cancelled())throw std::runtime_error("Paint surface construction cancelled");
          double u = (x + .5) / r, v = (y + .5) / r;
          float w0 = float(
              ((b[1] - c[1]) * (u - c[0]) + (c[0] - b[0]) * (v - c[1])) / den);
          float w1 = float(
                    ((c[1] - a[1]) * (u - c[0]) + (a[0] - c[0]) * (v - c[1])) /
                    den),
                w2 = 1 - w0 - w1;
          if (!std::isfinite(w0)||!std::isfinite(w1)||!std::isfinite(w2)||std::min({w0, w1, w2}) < -.000001f)
            continue;
          auto p = raster.positions[ids[0]] * w0 + raster.positions[ids[1]] * w1 +
                   raster.positions[ids[2]] * w2;
          size_t index = size_t(y) * r + x;
          auto &t = texels[index];
          Texel candidate{p,normalized(raster.normals[ids[0]]*w0+raster.normals[ids[1]]*w1+raster.normals[ids[2]]*w2),true,uint32_t(k/3),{w0,w1}};
          if(!t.covered){t=candidate;++diagnostics.covered_texels;}
          else if(!source){
            if(length(t.position-p)>1e-4f)throw std::invalid_argument("Overlapping UV islands cannot be painted independently");
            t=candidate;
          }else{
            auto same=[&](const Texel &other){
              if(++diagnostics.candidate_checks>options.max_candidate_checks)throw std::length_error("Paint surface comparison budget exceeded");
              return length(other.position-p)<=1e-5f&&dot(other.normal,candidate.normal)>.9999f;
            };
            bool duplicate=same(t);auto found=linked.find(uint32_t(index));
            if(!duplicate&&found!=linked.end())for(auto at=found->second.first;at!=UINT32_MAX;at=extra[at].next)
              if(same(extra[at].texel)){duplicate=true;break;}
            if(!duplicate){
              auto count=found==linked.end()?0:found->second.count;
              if(count+1>=options.max_candidates_per_texel||extra.size()>=options.max_additional_candidates)
                throw std::length_error("Linked source paint surface budget exceeded");
              auto &head=linked[uint32_t(index)];
              if(!head.count)++diagnostics.linked_texels;
              extra.push_back({candidate,head.first});head.first=uint32_t(extra.size()-1);++head.count;
              ++diagnostics.additional_candidates;
            }
          }
          padding[index] = int32_t(index);
          auto &bounds = tiles[size_t(y / 32) * columns + x / 32];
          bounds.lo = low(bounds.lo, p);
          bounds.hi = high(bounds.hi, p);
        }
    }
    if (!source && (!set_domain||!set_domain->triangles().empty()) && std::none_of(texels.begin(), texels.end(),
                     [](auto &t) { return t.covered; }))
      throw std::invalid_argument(
          "UV islands have no covered texels at this resolution");
    for (int pass = 0; pass < 4; ++pass) {
      auto next = padding;
      for (int y = 0; y < int(r); ++y)
        for (int x = 0; x < int(r); ++x)
          if (padding[size_t(y) * r + x] < 0) {
            for (auto [dx, dy] : {std::pair{-1, 0}, {1, 0}, {0, -1}, {0, 1}}) {
              int a = x + dx, b = y + dy;
              if (a >= 0 && b >= 0 && a < int(r) && b < int(r) &&
                  padding[size_t(b) * r + a] >= 0) {
                next[size_t(y) * r + x] = padding[size_t(b) * r + a];
                break;
              }
            }
          }
      padding = std::move(next);
    }
    options.cancelled={};
    sampling_raster_proven=bool(paint_domain);
  }
};
bool PaintedMesh::gpu_color_blur_eligible()const noexcept{
  return surface_&&!editing_&&!surface_->source_capture_only&&!surface_->paint_domain&&surface_->diagnostics.policy==PaintUvPolicy::Strict&&surface_->linked.empty();
}
PaintBlurReport PaintedMesh::blur_color_layer_gpu(const PaintBlurOptions&options,const std::function<bool()>&cancelled){
  auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("GPU layer blur cancelled");};stop();
  if(editing_)throw std::logic_error("Finish the active paint gesture before blurring its layer");
  if(!std::isfinite(options.strength)||options.strength<0||options.strength>1||options.maximum_working_bytes>512*1024*1024||!options.maximum_working_bytes)throw std::invalid_argument("Invalid GPU layer blur limits");
  if(!gpu_color_blur_eligible())throw std::logic_error("GPU layer blur requires an unambiguous current strict UV field; retained/repeated/overlapping source-coordinate domains need their separate authenticated tool");
  PaintBlurReport report;if(options.strength==0)return report;
  const auto&original=layer(active_);const uint32_t columns=surface_->columns;
  std::vector<uint8_t>dirty(original.tiles.size());
  for(size_t tile=0;tile<original.tiles.size();++tile){stop();
    if(!original.tiles[tile]||std::none_of(original.tiles[tile]->pixels.begin(),original.tiles[tile]->pixels.end(),[](const auto&p){return p.data[9]!=0;}))continue;
    int x=int(tile%columns),y=int(tile/columns);
    // Seven-pixel cross halos can affect adjacent32px tiles. A conservative3x3
    // tile directory includes corners; chart/membership and kernel reject them.
    for(int dy=-1;dy<=1;++dy)for(int dx=-1;dx<=1;++dx)if(x+dx>=0&&y+dy>=0&&x+dx<int(columns)&&y+dy<int(columns))dirty[size_t(y+dy)*columns+size_t(x+dx)]=1;
  }
  const size_t count=size_t(std::count(dirty.begin(),dirty.end(),uint8_t(1)));if(!count)return report;
  struct Edge{uint32_t a,b,face;};const auto&mesh=surface_->mesh;const size_t faces=mesh.indices.size()/3;
  if(faces>2'000'000||mesh.positions.size()>2'000'000)throw std::length_error("GPU layer blur chart geometry exceeds its exact label/planning bound");
  // Include the retained native data, private result tiles, chart planning,
  // layer-history pointer copies and bounded CPU/GPU tile transfer scratch.
  const size_t copies=count*(sizeof(PaintTile)+(original.pbr_tiles.empty()?0:sizeof(PaintColorTile)));
  const size_t fixed=sizeof(detail::ColorBlurInput)+sizeof(detail::ColorBlurOutput);
  const size_t chart=mesh.indices.size()*sizeof(Edge)+faces*sizeof(uint32_t);
  size_t retained=0;
  auto charge=[&](size_t bytes){if(bytes>options.maximum_working_bytes-retained)throw std::length_error("GPU layer blur retained authority/history budget exceeded");retained+=bytes;};
  charge(storage_bytes());charge(history_bytes_);
  charge(detail::TextureSetPaintAccess::working_bytes(*this,resolution()));
  charge(mesh.positions.size()*sizeof(Vec3)+mesh.normals.size()*sizeof(Vec3)+mesh.uv.size()*sizeof(std::array<float,2>)+mesh.indices.size()*sizeof(uint32_t)+mesh.uv_pins.size()+mesh.uv_seams.size());
  if(source_maps_)charge(source_maps_->unique_pixels()*4);
  for(const auto*history:{&undo_,&redo_})for(const auto&s:*history)charge(sizeof(Stroke)+(s.before.capacity()+s.after.capacity()+s.color_before.capacity()+s.color_after.capacity()+s.emission_before.capacity()+s.emission_after.capacity())*sizeof(std::shared_ptr<const PaintTile>));
  if(retained>options.maximum_working_bytes||copies>options.maximum_working_bytes-retained||chart>options.maximum_working_bytes-retained-copies||fixed*3>options.maximum_working_bytes-retained-copies-chart)throw std::length_error("GPU layer blur retained/chart/dirty-tile staging budget exceeded");
  std::vector<uint32_t>parent(faces);std::iota(parent.begin(),parent.end(),0);
  std::vector<Edge>edges;edges.reserve(mesh.indices.size());
  auto root=[&](uint32_t i){while(parent[i]!=i){parent[i]=parent[parent[i]];i=parent[i];}return i;};
  for(uint32_t face=0;face<faces;++face){if((face&1023)==0)stop();for(size_t k=0;k<3;++k){if(!mesh.uv_seams.empty()&&mesh.uv_seams[size_t(face)*3+k])continue;auto a=mesh.indices[size_t(face)*3+k],b=mesh.indices[size_t(face)*3+(k+1)%3];if(a>b)std::swap(a,b);edges.push_back({a,b,face});}}
  std::sort(edges.begin(),edges.end(),[](auto a,auto b){return std::tie(a.a,a.b,a.face)<std::tie(b.a,b.b,b.face);});stop();
  for(size_t i=1;i<edges.size();++i){if((i&4095)==0)stop();if(edges[i-1].a==edges[i].a&&edges[i-1].b==edges[i].b){auto a=root(edges[i-1].face),b=root(edges[i].face);if(a!=b)parent[b]=a;}}
  for(uint32_t face=0;face<faces;++face)parent[face]=root(face);
  // These labels name actual indexed writable UV connectivity. Normal/source
  // grids, nearest-position welding and generated texture carriers are absent.
  auto label=[&](uint32_t x,uint32_t y){const auto&t=surface_->texels[size_t(y)*resolution()+x];return t.covered&&(!texture_set_domain_||texture_set_domain_->contains(t.face))?parent.at(t.face)+1:0;};
  auto draft=snapshot();draft->undo_=undo_;draft->redo_=redo_;draft->history_bytes_=history_bytes_;
  draft->begin_stroke();auto&target=draft->active();
  detail::ColorBlurGpu gpu(options.allow_software_validation);report.device=gpu.device();
  auto input=std::make_unique<detail::ColorBlurInput>();auto output=std::make_unique<detail::ColorBlurOutput>();
  for(size_t tile=0;tile<dirty.size();++tile)if(dirty[tile]){
    stop();const int x0=int(tile%columns)*32,y0=int(tile/columns)*32;
    for(size_t y=0;y<detail::blur_input;++y)for(size_t x=0;x<detail::blur_input;++x){
      const uint32_t px=uint32_t(std::clamp(x0+int(x)-7,0,int(resolution())-1)),py=uint32_t(std::clamp(y0+int(y)-7,0,int(resolution())-1));
      const auto&p=pixel(active_,px,py);const auto other=pbr_pixel(active_,px,py);const size_t i=y*detail::blur_input+x;
      for(size_t c=0;c<3;++c){input->color[0][i][c]=unit(p.data[c]);input->color[1][i][c]=unit(other[c]);}
      input->color[0][i][3]=input->color[1][i][3]=unit(p.data[9]);
      input->metadata[i]={unit(p.data[3]),unit(other[3]),unit(p.data[13]),float(label(px,py))};
    }
    gpu.run(*input,*output,options.strength,cancelled);++report.tiles;report.uploaded_bytes+=sizeof(*input);report.downloaded_bytes+=sizeof(*output);
    auto copy=original.tiles[tile]?std::make_shared<PaintTile>(*original.tiles[tile]):std::make_shared<PaintTile>();
    std::shared_ptr<PaintColorTile>pbr_copy;if(!original.pbr_tiles.empty()){
      pbr_copy=original.pbr_tiles[tile]?std::make_shared<PaintColorTile>(*original.pbr_tiles[tile]):std::make_shared<PaintColorTile>();
      if(!original.pbr_tiles[tile])for(size_t y=0;y<32;++y)for(size_t x=0;x<32;++x)pbr_copy->pixels[y*32+x]=pbr_pixel(active_,uint32_t(x0+int(x)),uint32_t(y0+int(y)));
    }
    bool changed=false,pbr_changed=false;
    for(size_t y=0;y<32;++y)for(size_t x=0;x<32;++x){
      const size_t i=y*32+x,source=(y+7)*detail::blur_input+x+7;
      if(input->metadata[source][2]==0||input->metadata[source][3]==0)continue;
      auto&p=copy->pixels[i];const auto before=p;
      const float coverage=output->planes[0][i][3];
      if(!std::isfinite(coverage)||coverage<0||coverage>1.00001f)throw std::runtime_error("GPU blur returned invalid color coverage");
      for(const auto&plane:output->planes)for(float value:plane[i])if(!std::isfinite(value)||value<-.00001f||value>1.00001f)throw std::runtime_error("GPU blur returned invalid linear color/alpha data");
      if(coverage>0){
        for(size_t c=0;c<3;++c)p.data[c]=byte(output->planes[0][i][c]);
        p.data[3]=byte(output->planes[2][i][0]);p.data[9]=byte(coverage);
        if(pbr_copy){auto before_pbr=pbr_pixel(active_,uint32_t(x0+int(x)),uint32_t(y0+int(y)));auto&color=pbr_copy->pixels[i];for(size_t c=0;c<3;++c)color[c]=byte(output->planes[1][i][c]);color[3]=byte(output->planes[2][i][1]);pbr_changed|=color!=before_pbr;}
      }
      changed|=p!=before;
    }
    if(changed||pbr_changed){
      if(!original.tiles[tile])++draft->allocated_tiles_;
      target.tiles[tile]=std::move(copy);
      if(pbr_copy&&pbr_changed){if(!original.pbr_tiles[tile])++draft->allocated_color_tiles_;target.pbr_tiles[tile]=std::move(pbr_copy);}
      ++report.changed_tiles;
    }
  }
  stop();if(!report.changed_tiles)return report;
  if(draft->storage_bytes()>128*1024*1024)throw std::length_error("GPU layer blur native paint storage budget exceeded");
  if(draft->revision_==UINT64_MAX)throw std::overflow_error("GPU layer blur paint revision exhausted");
  ++draft->revision_;draft->composite_token_=next_composite_token();report.changed=draft->end_stroke();stop();swap_state(*draft);return report;
}
struct PaintedMesh::Evaluation {
  std::shared_ptr<const SurfaceMap> source;
  std::shared_ptr<const Mesh> geometry;
  std::vector<SurfaceMap::Bounds> bounds;
  std::pair<Vec3,Vec3> sample(const SurfaceMap::Texel &t)const {
    const auto &m=*geometry;auto at=size_t(t.face)*3;float w[]={t.bary[0],t.bary[1],1-t.bary[0]-t.bary[1]};
    Vec3 p{},n{};for(int k=0;k<3;++k){auto v=m.indices.at(at+k);p=p+m.positions.at(v)*w[k];n=n+m.normals.at(v)*w[k];}
    return {p,normalized(n)};
  }
};
std::shared_ptr<const PaintedMesh::Evaluation> PaintedMesh::evaluate(std::shared_ptr<const Mesh> mesh,const std::function<bool()> &cancelled)const {
  if(!mesh)throw std::invalid_argument("Missing evaluated paint surface");
  mesh->validate();const auto &rest=surface_->mesh;
  if(mesh->indices!=rest.indices||mesh->positions.size()!=rest.positions.size()||mesh->normals.size()!=rest.normals.size()||mesh->uv!=rest.uv||mesh->uv_pins!=rest.uv_pins||mesh->uv_seams!=rest.uv_seams)
    throw std::invalid_argument("Evaluated painting requires the exact original triangle and UV identities");
  auto result=std::make_shared<Evaluation>();result->source=surface_;result->geometry=std::move(mesh);result->bounds.resize(surface_->tiles.size());
  for(size_t i=0;i<surface_->texels.size();++i){
    if((i&255)==0&&cancelled&&cancelled())throw std::runtime_error("Evaluated paint preparation cancelled");
    auto tile=(i/resolution()/32)*surface_->columns+(i%resolution()/32);
    auto include=[&](const SurfaceMap::Texel &t){if(!t.covered)return;auto p=result->sample(t).first;auto &b=result->bounds.at(tile);b.lo=low(b.lo,p);b.hi=high(b.hi,p);};
    include(surface_->texels[i]);if(auto linked=surface_->linked.find(uint32_t(i));linked!=surface_->linked.end())for(auto at=linked->second.first;at!=UINT32_MAX;at=surface_->extra[at].next)include(surface_->extra[at].texel);
  }
  return result;
}
void PaintedMesh::stamp_evaluated(const Brush &b,const PaintSettings &settings,const Evaluation &evaluation,const std::function<bool()> &cancelled){
  if(evaluation.source!=surface_)throw std::invalid_argument("Evaluated painting source changed");
  stamp_sampled(b,settings,{},cancelled,&evaluation);
}
PaintedMesh::PaintedMesh(Mesh mesh, uint32_t r):PaintedMesh(std::move(mesh),r,{}){}
PaintedMesh::PaintedMesh(Mesh mesh,uint32_t r,const PaintSurfaceOptions &options)
    :PaintedMesh(std::move(mesh),r,options,{}){}
PaintedMesh::PaintedMesh(Mesh mesh,uint32_t r,const PaintSurfaceOptions &options,
    std::shared_ptr<const PaintSamplingDomain> sampling)
    :PaintedMesh(std::make_shared<const Mesh>(std::move(mesh)),r,options,std::move(sampling)){}
PaintedMesh::PaintedMesh(std::shared_ptr<const Mesh> mesh,uint32_t r,const PaintSurfaceOptions &options,
    std::shared_ptr<const PaintSamplingDomain> sampling,std::shared_ptr<const TextureSetFaceDomain> membership) {
  if (r < 64 || r > 2048 || (r & (r - 1)))
    throw std::invalid_argument(
        "Paint resolution must be a power of two between 64 and 2048");
  surface_ = std::make_shared<SurfaceMap>(std::move(mesh), r,options,std::move(sampling),PaintSamplingLimits{},membership);
  texture_set_domain_=std::move(membership);
  seal_paint_sampling_attachment();
  composite_cache_=std::make_shared<CompositeCache>();
  composite_token_=next_composite_token();
  PaintLayer layer;
  layer.id = 1;
  layer.tiles.resize(surface_->tiles.size());
  layers_.push_back(std::move(layer));
}
const Mesh &PaintedMesh::mesh() const { return surface_->mesh; }
bool PaintedMesh::source_sampling_prepared() const noexcept {return surface_&&bool(surface_->original_sampling);}
bool PaintedMesh::valid_source_sampling_attachment() const noexcept {
  if(!source_sampling_prepared())return true;
  const auto &maps=surface_->original_sampling->current_maps;
  return maps&&source_maps_&&maps==source_maps_&&!maps.owner_before(source_maps_)&&!source_maps_.owner_before(maps)&&
    surface_->options.policy==PaintUvPolicy::Strict&&!surface_->paint_domain&&!color_projection_&&!normal_projection_&&
    ((!baked_&&!surface_->original_sampling->baked)||
     (baked_&&baked_==surface_->original_sampling->baked&&!baked_.owner_before(surface_->original_sampling->baked)&&!surface_->original_sampling->baked.owner_before(baked_)));
}
std::shared_ptr<const PaintedMesh> PaintedMesh::source_sampling_original() const noexcept {
  return source_sampling_prepared()?surface_->original_sampling->before:nullptr;
}
std::span<const uint32_t> PaintedMesh::source_sampling_vertices() const noexcept {
  return source_sampling_prepared()?std::span<const uint32_t>(surface_->original_sampling->vertices):std::span<const uint32_t>{};
}
std::shared_ptr<const std::vector<std::array<float,2>>> PaintedMesh::source_sampling_authored_uv1() const noexcept {
  return source_sampling_prepared()?surface_->original_sampling->authored_uv1:nullptr;
}
struct SourcePaintAccess {
  using Coordinates=std::vector<std::array<float,2>>;
  static void merge_bake(PaintedMesh &,BakedTextures,BakedNormalWrite,const std::function<bool()> &cancelled={});
  static std::shared_ptr<PaintedMesh> install(const PaintedMesh &,BakedTextures,const SourcePaintOptions &);
  static void clear(PaintedMesh &);
  static size_t surface_copy_bytes(const PaintedMesh &p){
    const auto &s=*p.surface_;
    return sizeof(PaintedMesh::SurfaceMap)+s.texels.size()*sizeof(PaintedMesh::SurfaceMap::Texel)+
      s.extra.size()*sizeof(PaintedMesh::SurfaceMap::Extra)+s.tiles.size()*sizeof(PaintedMesh::SurfaceMap::Bounds)+
      s.padding.size()*sizeof(int32_t)+s.linked.size()*(sizeof(PaintedMesh::SurfaceMap::Links::value_type)+4*sizeof(void*))+
      s.linked.bucket_count()*sizeof(void*)+s.diagnostics.unpaintable_uv_faces.size()*sizeof(uint32_t)+
      sizeof(PaintedMesh::SurfaceMap::OriginalSampling)+p.source_sampling_vertices().size()*sizeof(uint32_t);
  }
  static const PaintSourceMaps &maps(const PaintedMesh &paint){static const PaintSourceMaps empty;return paint.source_maps_?*paint.source_maps_:empty;}
  static std::shared_ptr<PaintedMesh> capture(Mesh mesh,std::shared_ptr<const PaintSourceMaps> input,uint32_t resolution,PaintSurfaceOptions options){
    if(mesh.positions.size()>2'000'000||mesh.indices.size()/3>2'000'000)
      throw std::length_error("Original sampling capture exceeds native persisted geometry bounds");
    if(options.policy!=PaintUvPolicy::Strict&&options.policy!=PaintUvPolicy::SourceAuthored)
      throw std::invalid_argument("Original sampling capture cannot reinterpret a polygon paint domain");
    options.policy=PaintUvPolicy::SourceAuthored;
    auto roles=input?std::make_shared<const PaintSourceMaps>(*input):std::make_shared<const PaintSourceMaps>();
    roles->validate_surface(mesh);
    auto out=std::shared_ptr<PaintedMesh>(new PaintedMesh(std::move(mesh),resolution,options));out->source_maps_=std::move(roles);return out;
  }
  static void authored(const PaintedMesh &before,const std::shared_ptr<const Coordinates> &uv1,const SourcePaintOptions &o){
    if(!uv1)return;
    if(uv1->size()!=before.mesh().positions.size()||uv1->size()>o.max_vertices||uv1->size()>2'000'000)
      throw std::invalid_argument("Authored logical UV1 count differs from its original vertex domain");
    const auto &original=maps(before).uv1;
    for(size_t n=0;n<uv1->size();++n){if((n&1023)==0)paint_compute::checkpoint(o.cancelled);const auto &q=uv1->at(n);
      if(!std::isfinite(q[0])||!std::isfinite(q[1])||std::abs(q[0])>10000||std::abs(q[1])>10000)
        throw std::invalid_argument("Authored logical UV1 exceeds its finite native bound");
      if(original&&std::bit_cast<std::array<uint32_t,2>>(q)!=std::bit_cast<std::array<uint32_t,2>>(original->at(n)))
        throw std::invalid_argument("Authored logical UV1 differs from its genuine PBR coordinate owner");
    }
  }

  static void pristine(const PaintedMesh &before) {
    if(before.in_stroke()||before.source_sampling_prepared()||before.paint_sampling_domain()||
       before.baked_||before.color_projection_||before.normal_projection_||
       std::any_of(before.layers_.begin(),before.layers_.end(),[](const auto &l){return tile_count(l)||color_tile_count(l)||emission_tile_count(l);}))
      throw std::invalid_argument("Initial source paint requires its own pristine original image sampling");
  }
  static bool same(const PaintSourceMaps &a,const PaintSourceMaps &b) {
    auto coordinates=[](const auto &x,const auto &y){
      return bool(x)==bool(y)&&(!x||((x==y&&!x.owner_before(y)&&!y.owner_before(x))||
        (x->size()==y->size()&&!std::memcmp(x->data(),y->data(),x->size()*sizeof(x->front())))));
    };
    if(!coordinates(a.uv1,b.uv1)||a.extra_uv_sets.size()!=b.extra_uv_sets.size()||
       std::bit_cast<uint32_t>(a.normal_scale)!=std::bit_cast<uint32_t>(b.normal_scale)||
       std::bit_cast<uint32_t>(a.occlusion_strength)!=std::bit_cast<uint32_t>(b.occlusion_strength))return false;
    for(const auto &[set,values]:a.extra_uv_sets){auto it=b.extra_uv_sets.find(set);if(it==b.extra_uv_sets.end()||!coordinates(values,it->second))return false;}
    const std::array<const PaintSourceTexture*,5> x{&a.color,&a.metal_rough,&a.emission,&a.normal,&a.occlusion},
      y{&b.color,&b.metal_rough,&b.emission,&b.normal,&b.occlusion};
    for(size_t n=0;n<x.size();++n)if(x[n]->image!=y[n]->image||x[n]->texcoord!=y[n]->texcoord||
      x[n]->wrap_s!=y[n]->wrap_s||x[n]->wrap_t!=y[n]->wrap_t||x[n]->min_filter!=y[n]->min_filter||
      x[n]->mag_filter!=y[n]->mag_filter||std::bit_cast<std::array<uint32_t,6>>(x[n]->uv_transform)!=std::bit_cast<std::array<uint32_t,6>>(y[n]->uv_transform))return false;
    return true;
  }
  static detail::SourceSamplingLimits limits(const SourcePaintOptions &o,size_t vertices,size_t authored_vertices=0) {
    if(!o.max_work||o.max_work>64'000'000)throw std::invalid_argument("Invalid independent source paint work bound");
    const uint64_t reserved=uint64_t(vertices)+authored_vertices+(authored_vertices?vertices:0);
    if(o.max_work<=reserved)throw std::length_error("Independent source paint logical-coordinate work exceeds its budget");
    return {o.max_vertices,o.max_triangles,o.max_coordinate_bytes,o.max_work-reserved,o.cancelled};
  }
  struct Footprint {
    uint64_t geometry=0,coordinates=0,pixels=0,tiles=0;
    bool raster=false;
  };
  static uint64_t geometry_bytes(const Mesh &m) {
    return uint64_t(m.positions.size())*sizeof(Vec3)+uint64_t(m.normals.size())*sizeof(Vec3)+
      uint64_t(m.uv.size())*sizeof(std::array<float,2>)+uint64_t(m.indices.size())*sizeof(uint32_t)+
      m.uv_pins.size()+m.uv_seams.size();
  }
  static uint64_t coordinate_bytes(const PaintSourceMaps &maps) {
    uint64_t bytes=maps.uv1?maps.uv1->size()*sizeof(std::array<float,2>):0;
    for(const auto &[set,values]:maps.extra_uv_sets)if(values)bytes+=values->size()*sizeof(std::array<float,2>);
    return bytes;
  }
  static Footprint footprint(const PaintedMesh &p) {
    return {geometry_bytes(p.mesh()),coordinate_bytes(maps(p)),
      uint64_t(p.resolution())*p.resolution(),uint64_t(p.allocated_tiles_)*sizeof(PaintTile)+
      uint64_t(p.allocated_color_tiles_)*sizeof(PaintColorTile)+[&]{uint64_t n=p.baked_?p.baked_->storage_bytes():0;for(const auto &l:p.layers_)n+=emission_tile_count(l)*sizeof(PaintEmissionTile);return n;}(),!p.surface_->source_capture_only};
  }
  // One canonical forecast for preparation, save, decode and prospective
  // strokes. Reserve the full sixteen layer-slot arrays, so metadata-only
  // layer edits cannot make a previously accepted owner impossible to reopen.
  // The second current-coordinate copy is the detached decoder replay. Both
  // raster construction peaks include the temporary padding pass.
  static uint64_t working_bytes(const Footprint &before,const Footprint &current,uint64_t remap,uint64_t authored=0) {
    auto surface=[](const Footprint &p) {
      return p.geometry+p.tiles+p.pixels/1024*sizeof(PaintedMesh::SurfaceMap::Bounds)+
        p.pixels/1024*16*3*sizeof(std::shared_ptr<const PaintTile>)+
        (p.raster?p.pixels*(sizeof(PaintedMesh::SurfaceMap::Texel)+2*sizeof(int32_t)):0);
    };
    // Conservatively reserve an independently derived logical UV0 and its
    // decoder replay, even when a source-image carrier can share the owner.
    return surface(before)+surface(current)+before.coordinates+2*current.coordinates+5*remap+(authored?2*authored+4*remap:0);
  }
  static uint64_t retained_bytes(const Footprint &before,const Footprint &current,uint64_t remap,uint64_t authored=0) {
    return before.geometry+before.coordinates+before.tiles+current.coordinates+current.tiles+3*remap+(authored?authored+2*remap:0);
  }
  static uint64_t working_bytes(const PaintedMesh &current) {
    return working_bytes(footprint(*current.source_sampling_original()),footprint(current),
      current.source_sampling_vertices().size()*sizeof(uint32_t),current.source_sampling_authored_uv1()?current.source_sampling_authored_uv1()->size()*sizeof(std::array<float,2>):0);
  }
  static void budget(const PaintedMesh &before,const Mesh &target,const SourcePaintOptions &o,const std::shared_ptr<const Coordinates> &uv1={}) {
    if(!o.max_working_bytes||o.max_working_bytes>SourcePaintOptions::maximum_working_bytes||
       !o.max_raster_samples||o.max_raster_samples>512*1024*1024)
      throw std::invalid_argument("Invalid independent source paint resource bounds");
    const auto &maps=SourcePaintAccess::maps(before);
    const std::array views{&maps.color,&maps.metal_rough,&maps.emission,&maps.normal,&maps.occlusion};
    const bool zero=std::any_of(views.begin(),views.end(),
      [](const auto *view){return view->image&&view->texcoord==0;});
    const uint64_t sets=uint64_t(bool(maps.uv1))+maps.extra_uv_sets.size()+uint64_t(zero);
    const uint64_t extra=uv1&&!maps.uv1?uv1->size()*sizeof(std::array<float,2>):0;
    if((sets+uint64_t(!zero)+uint64_t(bool(extra)))*target.positions.size()*sizeof(std::array<float,2>)+extra>o.max_coordinate_bytes)
      throw std::length_error("Independent source paint logical coordinates exceed their budget");
    const Footprint current{geometry_bytes(target),sets*target.positions.size()*sizeof(std::array<float,2>),
      uint64_t(before.resolution())*before.resolution(),0,true};
    const auto original=footprint(before);const uint64_t remap=target.positions.size()*sizeof(uint32_t);
    if(working_bytes(original,current,remap,extra)>o.max_working_bytes||
       retained_bytes(original,current,remap,extra)>128*1024*1024)
      throw std::length_error("Independent source paint exceeds cumulative raster/coordinate/geometry budget");
  }
  static void encoded_budget(const nlohmann::json &input,bool physical,const detail::BakedEmissionReadCapability *capability) {
    const bool source_bake=input.at("version")==8;
    if(source_bake&&(!capability||!capability->source_bake_allowed()))throw std::invalid_argument("Source Bake preflight requires Scene42");
    const auto measure=[&](const nlohmann::json &p,bool original) {
      if(!p.is_object()||p.contains("version")||p.contains("domain")||p.contains("payload")||
         ((original||!source_bake)&&p.contains("baked"))||p.contains("color_projection")||p.contains("normal_projection")||p.contains("authored_uv1")||
         (!original&&p.contains("uv_policy")))
        throw std::invalid_argument("Independent source paint has an unsupported encoded interpretation");
      const auto &r=p.at("resolution");
      if(!r.is_number_integer()||r.get<int64_t>()<64||r.get<uint64_t>()>2048||
         (r.get<uint64_t>()&(r.get<uint64_t>()-1)))throw std::invalid_argument("Invalid source paint resolution");
      const uint64_t size=r.get<uint64_t>();const auto &mesh=p.at("mesh");
      auto bytes=[&](const char *key,size_t stride,size_t count){const auto &v=mesh.at(key);
        if(!v.is_binary()||v.get_binary().size()%stride||v.get_binary().size()/stride>count)
          throw std::invalid_argument("Independent source paint geometry exceeds bounds");return uint64_t(v.get_binary().size());};
      const auto positions=bytes("positions",12,2'000'000),normals=bytes("normals",12,2'000'000),
        uv=bytes("uv",8,2'000'000),indices=bytes("indices",12,2'000'000);
      if(positions!=normals||positions/12!=uv/8)throw std::invalid_argument("Incomplete source paint geometry");
      Footprint result;result.geometry=positions+normals+uv+indices;result.pixels=size*size;
      if(p.contains("baked")){
        const auto &b=p.at("baked");const auto v=detail::baked_legacy_version(b,capability);
        if(v>1||b.contains("normal_authority")||b.contains("normal_field"))throw std::invalid_argument("Source Bake requires current canonical normal/color data");
        for(const auto *key:{"color","orm","emission","normal","coverage","pbr_color"})if(b.contains(key)){
          const auto &data=b.at(key);if(!data.is_binary()||data.get_binary().size()>size*size*4)throw std::invalid_argument("Source Bake buffer exceeds its actual atlas");
          result.tiles+=data.get_binary().size();
        }
        result.tiles+=detail::baked_float_preflight(b,uint32_t(size),capability);
      }
      for(const char *key:{"uv_pins","uv_seams"})if(mesh.contains(key)){
        if(!mesh.at(key).is_binary())throw std::invalid_argument("Invalid source paint edit flags");result.geometry+=mesh.at(key).get_binary().size();
      }
      if(!original&&!p.contains("source_maps"))throw std::invalid_argument("Independent source paint lacks its explicit role bundle");
      if(p.contains("source_maps"))result.coordinates=detail::source_uv_archive_measure(p.at("source_maps")).bytes;
      bool outside=false;
      if(original&&p.value("uv_policy",std::string{})=="source-authored") {
        const auto &values=mesh.at("uv").get_binary();
        for(size_t at=0;at<values.size();at+=4){uint32_t word=0;for(size_t k=0;k<4;++k)word|=uint32_t(values[at+k])<<(k*8);
          const float q=std::bit_cast<float>(word);if(!std::isfinite(q)||std::abs(q)>10000)throw std::invalid_argument("Invalid original source paint coordinate");
          outside|=q<0||q>1;}
      }
      result.raster=!outside;
      const auto &layers=p.at("layers");if(!layers.is_array()||layers.empty()||layers.size()>16)throw std::invalid_argument("Invalid source paint layers");
      for(const auto &layer:layers) {
        if(layer.contains("physical_emission_tiles")) {
          const auto &emission=layer.at("physical_emission_tiles");
          if(original||!physical||!emission.is_array()||emission.empty()||emission.size()>size*size/1024)
            throw std::invalid_argument("Physical source paint requires its own nonempty capability");
          result.tiles+=emission.size()*sizeof(PaintEmissionTile);
        }
        const auto &tiles=layer.at("tiles");
        if(!tiles.is_array()||tiles.size()>size*size/1024||(original&&!tiles.empty()))throw std::invalid_argument("Invalid or nonpristine source paint tiles");
        result.tiles+=tiles.size()*sizeof(PaintTile);
        if(layer.contains("pbr_color_tiles")&&(!layer.at("pbr_color_tiles").is_array()||!layer.at("pbr_color_tiles").empty()))
          throw std::invalid_argument("Independent source paint cannot borrow a prepared color plane");
      }
      return result;
    };
    const auto before=measure(input.at("before"),true),current=measure(input.at("payload"),false);
    const auto remap=input.at("vertices").get_binary().size();
    const auto extra=detail::source_paint_authored_uv1_bytes(input);
    // Source8 reserves the independent immutable proof/raster copy and current
    // baked decode/adoption scratch BEFORE constructing either SurfaceMap.
    const size_t source_bake_scratch=source_bake?current.tiles+remap+sizeof(PaintedMesh::SurfaceMap)+
      sizeof(PaintedMesh::SurfaceMap::OriginalSampling)+current.pixels*(sizeof(PaintedMesh::SurfaceMap::Texel)+2*sizeof(int32_t))+
      current.pixels/1024*(sizeof(PaintedMesh::SurfaceMap::Bounds)+16*3*sizeof(std::shared_ptr<const PaintTile>)):0;
    const auto working=working_bytes(before,current,remap,extra);
    if(working>SourcePaintOptions::maximum_working_bytes||source_bake_scratch>SourcePaintOptions::maximum_working_bytes-working||
       retained_bytes(before,current,remap,extra)>128*1024*1024)
      throw std::length_error("Encoded independent source paint exceeds cumulative working/storage budget");
    if(input.at("vertices").get_binary().size()/4!=input.at("payload").at("mesh").at("positions").get_binary().size()/12)
      throw std::invalid_argument("Independent source paint remap size differs");
  }
  static void bind(PaintedMesh &current,std::shared_ptr<const PaintedMesh> before,
                   std::span<const uint32_t> vertices,const SourcePaintOptions &o,std::shared_ptr<const Coordinates> uv1={}) {
    const auto transport_limits=limits(o,vertices.size(),uv1?uv1->size():0);
    pristine(*before);budget(*before,current.mesh(),o,uv1);authored(*before,uv1,o);
    if(uv1&&maps(*before).uv1)throw std::invalid_argument("Redundant authored UV1 is not a canonical source-paint record");
    if(current.surface_->diagnostics.policy!=PaintUvPolicy::Strict||current.paint_sampling_domain()||
       current.baked_||current.color_projection_||current.normal_projection_||!current.source_maps_||
       current.resolution()!=before->resolution())
      throw std::invalid_argument("Independent source sampling requires its own strict current paint grid");
    auto expected=detail::transport_source_sampling(before->mesh(),maps(*before),current.mesh(),vertices,transport_limits);
    if((!expected->empty()&&!expected->uses_alternate_uv())||!same(*expected,*current.source_maps_))
      throw std::invalid_argument("Independent source paint differs from its exact original sampling transport");
    attach(current,std::move(before),vertices,o.cancelled,std::move(uv1));
  }
  static void attach(PaintedMesh &current,std::shared_ptr<const PaintedMesh> before,
                     std::span<const uint32_t> vertices,const std::function<bool()> &cancelled,std::shared_ptr<const Coordinates> uv1={}) {
    auto proof=std::make_shared<PaintedMesh::SurfaceMap::OriginalSampling>();
    proof->before=before->snapshot();proof->current_maps=current.source_maps_;proof->vertices.assign(vertices.begin(),vertices.end());
    const auto &original=maps(*before);const auto &transported=*current.source_maps_;
    const std::array<const PaintSourceTexture*,5> old{&original.color,&original.metal_rough,&original.emission,&original.normal,&original.occlusion},
      now{&transported.color,&transported.metal_rough,&transported.emission,&transported.normal,&transported.occlusion};
    for(size_t k=0;k<old.size();++k)if(old[k]->image&&old[k]->texcoord==0){
      const int carrier=now[k]->texcoord;
      proof->original_uv0=carrier==1?transported.uv1:transported.extra_uv_sets.at(uint32_t(carrier));break;
    }
    if(!proof->original_uv0){
      auto coordinates=std::make_shared<std::vector<std::array<float,2>>>();coordinates->reserve(vertices.size());
      for(size_t v=0;v<vertices.size();++v){if((v&1023)==0)paint_compute::checkpoint(cancelled);coordinates->push_back(before->mesh().uv.at(vertices[v]));}
      proof->original_uv0=std::move(coordinates);
    }
    // UV1 remains ordinal1 only if the original actually owned that authored
    // set. A generated raw-UV0 carrier must never be presented as authored UV1.
    if(original.uv1)proof->original_uv1=transported.uv1;
    else if(uv1){
      // Freeze caller data, then derive a separate actual CURRENT vertex domain.
      proof->authored_uv1=std::make_shared<const Coordinates>(*uv1);
      auto current_uv1=std::make_shared<Coordinates>();current_uv1->reserve(vertices.size());
      for(size_t n=0;n<vertices.size();++n){if((n&1023)==0)paint_compute::checkpoint(cancelled);current_uv1->push_back(proof->authored_uv1->at(vertices[n]));}
      proof->original_uv1=std::move(current_uv1);
    }
    paint_compute::checkpoint(cancelled);
    // Only fresh, unpublished factory/decoder construction reaches this bind.
    // No render snapshot or geometry alias may observe a mutable attachment.
    if(current.surface_.use_count()!=1)throw std::logic_error("Source paint attachment escaped before construction completed");
    std::const_pointer_cast<PaintedMesh::SurfaceMap>(current.surface_)->original_sampling=std::move(proof);
  }
  static std::shared_ptr<PaintedMesh> parameters(const PaintedMesh &current,
      const std::array<std::array<float,6>,5> &transforms,const SourcePaintOptions &o) {
    if(current.in_stroke()||!current.source_sampling_prepared()||!current.valid_source_sampling_attachment())
      throw std::invalid_argument("Source parameter edit requires its genuine independent writable owner");
    paint_compute::checkpoint(o.cancelled);
    const auto &original=*current.source_sampling_original();pristine(original);
    // The old current raster stays alive while prepare derives its replacement.
    // Source8 also retains its exact immutable bake; no merge or pixel copy is
    // needed, but both current geometry/coordinate/raster owners coexist.
    const auto working=working_bytes(current);
    const uint64_t replacement=current.baked_?surface_copy_bytes(current)+geometry_bytes(current.mesh())+
      coordinate_bytes(maps(current))+uint64_t(current.resolution())*current.resolution()/1024*16*3*sizeof(std::shared_ptr<const PaintTile>):0;
    if(!o.max_working_bytes||o.max_working_bytes>SourcePaintOptions::maximum_working_bytes||working>o.max_working_bytes||replacement>o.max_working_bytes-working)
      throw std::length_error("Source parameter edit exceeds prospective working budget");
    if(current.baked_){
      current.baked_->validate(current.resolution());
      if(current.baked_->normal_interpretation!=BakedNormalInterpretation::CanonicalMikk||current.baked_->normal_authority||current.baked_->normal_field||
         current.baked_->color_interpretation!=BakedColorInterpretation::VertexComponent||current.color_projection_||current.normal_projection_||current.paint_sampling_domain())
        throw std::invalid_argument("Source parameter edit requires its explicit current canonical baked owner");
    }
    // Authenticate the old attachment independently before deriving a new one.
    auto old=detail::transport_source_sampling(original.mesh(),maps(original),current.mesh(),
      current.source_sampling_vertices(),limits(o,current.mesh().positions.size(),current.source_sampling_authored_uv1()?current.source_sampling_authored_uv1()->size():0));
    if(!same(*old,maps(current)))throw std::invalid_argument("Source parameter edit has a stale original sampling witness");
    auto roles=std::make_shared<PaintSourceMaps>(maps(original));
    const std::array<PaintSourceTexture*,5> views{&roles->color,&roles->metal_rough,&roles->emission,&roles->normal,&roles->occlusion};
    for(size_t k=0;k<views.size();++k)views[k]->uv_transform=transforms[k];
    roles->validate_surface(original.mesh());
    auto before=original.snapshot();before->source_maps_=std::move(roles);
    before->composite_cache_=std::make_shared<PaintedMesh::CompositeCache>();before->composite_token_=next_composite_token();
    auto result=prepare(before,current.mesh(),current.source_sampling_vertices(),o,current.source_sampling_authored_uv1());
    result->layers_=current.layers_;result->active_=current.active_;result->next_=current.next_;
    result->allocated_tiles_=current.allocated_tiles_;result->allocated_color_tiles_=current.allocated_color_tiles_;
    if(current.baked_){
      // prepare authenticated the new original parameters against the exact
      // existing current mesh/UV trace. Pair that new proof with the unchanged
      // current canonical bake, whose frame never follows the source affine.
      auto proof=std::make_shared<PaintedMesh::SurfaceMap::OriginalSampling>(*result->surface_->original_sampling);
      proof->baked=current.baked_;
      if(result->surface_.use_count()!=1)throw std::logic_error("Source parameter attachment escaped before construction completed");
      std::const_pointer_cast<PaintedMesh::SurfaceMap>(result->surface_)->original_sampling=std::move(proof);
      result->baked_=current.baked_;
    }
    result->revision_=current.revision_+1;
    if(working_bytes(*result)>o.max_working_bytes||retained_bytes(footprint(*before),footprint(*result),
       result->source_sampling_vertices().size()*sizeof(uint32_t),result->source_sampling_authored_uv1()?result->source_sampling_authored_uv1()->size()*sizeof(std::array<float,2>):0)>128*1024*1024)
      throw std::length_error("Source parameter edit exceeds cumulative paint storage budget");
    if(!result->valid_source_sampling_attachment())throw std::logic_error("Source parameter edit failed its immutable current owner pairing");
    paint_compute::checkpoint(o.cancelled);return result;
  }
  static std::shared_ptr<PaintedMesh> prepare(std::shared_ptr<const PaintedMesh> before,Mesh target,
                                           std::span<const uint32_t> vertices,const SourcePaintOptions &o,std::shared_ptr<const Coordinates> uv1={}) {
    if(!before)throw std::invalid_argument("Independent source paint has no original owner");
    if(o.cancelled&&o.cancelled())throw std::runtime_error("Independent source paint cancelled");
    const auto transport_limits=limits(o,vertices.size(),uv1?uv1->size():0);
    pristine(*before);budget(*before,target,o,uv1);authored(*before,uv1,o);
    auto roles=detail::transport_source_sampling(before->mesh(),maps(*before),target,vertices,transport_limits);
    PaintSurfaceOptions surface;surface.cancelled=o.cancelled;surface.max_raster_samples=o.max_raster_samples;
    auto result=std::shared_ptr<PaintedMesh>(new PaintedMesh(std::move(target),before->resolution(),surface));
    result->source_maps_=std::move(roles);
    result->layers_=before->layers_;result->active_=before->active_;result->next_=before->next_;
    result->revision_=before->revision_+1;
    // Only this private factory has just produced the authenticated maps.
    // Decoding still replays and compares independently in bind().
    attach(*result,std::move(before),vertices,o.cancelled,std::move(uv1));return result;
  }
};
std::shared_ptr<PaintedMesh> detail::rebind_source_paint_transforms(const PaintedMesh &current,
 const std::array<std::array<float,6>,5> &transforms,const SourcePaintOptions &options) {
 return SourcePaintAccess::parameters(current,transforms,options);
}
std::shared_ptr<PaintedMesh> prepare_source_paint(std::shared_ptr<const PaintedMesh> before,Mesh target,
    std::span<const uint32_t> vertices,const SourcePaintOptions &options) {
  return SourcePaintAccess::prepare(std::move(before),std::move(target),vertices,options);
}
std::shared_ptr<PaintedMesh> capture_source_sampling(Mesh mesh,std::shared_ptr<const PaintSourceMaps> maps,uint32_t resolution,const PaintSurfaceOptions &options){
  return SourcePaintAccess::capture(std::move(mesh),std::move(maps),resolution,options);
}
std::shared_ptr<PaintedMesh> prepare_source_paint(std::shared_ptr<const PaintedMesh> before,Mesh target,
    std::span<const uint32_t> vertices,std::shared_ptr<const std::vector<std::array<float,2>>> uv1,const SourcePaintOptions &options){
  return SourcePaintAccess::prepare(std::move(before),std::move(target),vertices,options,std::move(uv1));
}
std::shared_ptr<const Mesh> PaintedMesh::mesh_owner() const { return surface_->geometry; }
std::shared_ptr<const PaintSamplingDomain> PaintedMesh::paint_sampling_domain() const noexcept {return surface_?surface_->paint_domain:nullptr;}
bool PaintedMesh::valid_paint_sampling_attachment() const noexcept {
  const auto domain=paint_sampling_domain();
  if(!domain)return !sampling_attachment_;
  return sampling_attachment_&&surface_->sampling_raster_proven&&
    sampling_attachment_->matches(surface_->geometry,domain);
}
void PaintedMesh::seal_paint_sampling_attachment(){
  if(!surface_->paint_domain){sampling_attachment_.reset();return;}
  if(!surface_->sampling_raster_proven)throw std::logic_error("Writable paint domain has no proved raster attachment");
  sampling_attachment_=std::shared_ptr<const PaintSamplingAttachment>(new const PaintSamplingAttachment(mesh_owner(),surface_->paint_domain,surface_));
}
const PaintSurfaceDiagnostics &PaintedMesh::surface_diagnostics() const {return surface_->diagnostics;}
uint32_t PaintedMesh::resolution() const { return surface_->resolution; }
const PaintLayer &PaintedMesh::layer(uint64_t id) const {
  auto i = std::find_if(layers_.begin(), layers_.end(),
                        [&](auto &l) { return l.id == id; });
  if (i == layers_.end())
    throw std::out_of_range("Unknown paint layer");
  return *i;
}
PaintLayer &PaintedMesh::active() {
  return const_cast<PaintLayer &>(layer(active_));
}
void PaintedMesh::select_layer(uint64_t id) {
  if (editing_)
    throw std::logic_error("Finish the paint stroke before changing layers");
  layer(id);
  active_ = id;
}
void PaintedMesh::add_layer(std::string name) {
  name_valid(name);
  if (editing_ || layers_.size() >= 16 || next_ == UINT64_MAX)
    throw std::length_error(
        "Finish the stroke or remove a layer before adding another");
  PaintLayer l;
  l.id = next_++;
  l.name = std::move(name);
  l.tiles.resize(surface_->tiles.size());
  if(color_projection_)l.pbr_tiles.resize(surface_->tiles.size());
  active_ = l.id;
  layers_.push_back(std::move(l));
  ++revision_;
}
void PaintedMesh::delete_layer() {
  if (editing_ || layers_.size() == 1)
    throw std::logic_error("Keep at least one paint layer");
  const auto &removed = layer(active_);
  bool visible=removed.visible&&removed.opacity>0&&tile_count(removed);
  uint64_t token=visible?next_composite_token():composite_token_;
  allocated_tiles_ -= tile_count(removed);
  allocated_color_tiles_ -= color_tile_count(removed);
  std::erase_if(layers_, [&](auto &l) { return l.id == active_; });
  active_ = layers_.back().id;
  composite_token_=token;
  ++revision_;
}
void PaintedMesh::layer_properties(std::string name, bool visible,
                                   float opacity) {
  name_valid(name);
  if (editing_ || !std::isfinite(opacity) || opacity < 0 || opacity > 1)
    throw std::invalid_argument("Invalid paint layer properties");
  auto &l = active();
  bool previous=l.visible&&l.opacity>0,next=visible&&opacity>0;
  bool visual=tile_count(l)&&((previous!=next)||(previous&&next&&l.opacity!=opacity));
  uint64_t token=visual?next_composite_token():composite_token_;
  l.name = std::move(name);
  l.visible = visible;
  l.opacity = opacity;
  composite_token_=token;
  ++revision_;
}
void PaintedMesh::move_layer(int direction) {
  if (editing_ || (direction != 1 && direction != -1))
    throw std::invalid_argument("Invalid layer move");
  auto i = std::find_if(layers_.begin(), layers_.end(),
                        [&](auto &l) { return l.id == active_; });
  auto index = i - layers_.begin();
  auto to = index + direction;
  if (to >= 0 && to < ptrdiff_t(layers_.size())) {
    bool visual=layers_[index].visible&&layers_[index].opacity>0&&tile_count(layers_[index])&&layers_[to].visible&&layers_[to].opacity>0&&tile_count(layers_[to]);
    auto token=visual?next_composite_token():composite_token_;
    std::swap(layers_[index], layers_[to]);
    composite_token_=token;
    ++revision_;
  }
}
const PaintPixel &PaintedMesh::pixel(uint64_t id, uint32_t x,
                                     uint32_t y) const {
  if (x >= resolution() || y >= resolution())
    throw std::out_of_range("Paint pixel out of bounds");
  auto &l = layer(id);
  auto &tile = l.tiles[size_t(y / 32) * surface_->columns + x / 32];
  static const PaintPixel empty;
  return tile ? tile->pixels[size_t(y % 32) * 32 + x % 32] : empty;
}
std::array<uint8_t,4> PaintedMesh::pbr_pixel(uint64_t id,uint32_t x,uint32_t y) const {
  if(x>=resolution() || y>=resolution())throw std::out_of_range("PBR paint pixel out of bounds");
  const auto &l=layer(id);
  if(!l.pbr_tiles.empty()) {
    const auto &tile=l.pbr_tiles.at(size_t(y/32)*surface_->columns+x/32);
    if(tile)return tile->pixels[size_t(y%32)*32+x%32];
  }
  const auto &p=pixel(id,x,y);return {p.data[0],p.data[1],p.data[2],p.data[3]};
}
std::array<std::shared_ptr<const std::vector<std::array<float,2>>>,2>
detail::PaintOverlayFactory::source_coordinates(const PaintedMesh &paint){
  if(!paint.source_sampling_prepared()||!paint.valid_source_sampling_attachment())
    throw std::invalid_argument("Original source coordinates require their own exact native sampling owner");
  const auto &proof=*paint.surface_->original_sampling;
  return {proof.original_uv0,proof.original_uv1};
}
std::shared_ptr<const detail::PaintOverlayPacket> detail::PaintOverlayFactory::capture(const PaintedMesh &paint,const std::function<bool()> &cancelled){
  return capture_impl(paint,cancelled,false,nullptr);
}
size_t detail::PaintOverlayFactory::raster_storage_bytes(const PaintedMesh &paint) noexcept{
  const auto &surface=*paint.surface_;
  const auto &mesh=surface.mesh;
  return sizeof(PaintedMesh::SurfaceMap)+
    (mesh.positions.capacity()+mesh.normals.capacity())*sizeof(Vec3)+mesh.uv.capacity()*sizeof(std::array<float,2>)+
    mesh.indices.capacity()*sizeof(uint32_t)+mesh.uv_pins.capacity()+mesh.uv_seams.capacity()+
    surface.texels.capacity()*sizeof(PaintedMesh::SurfaceMap::Texel)+surface.extra.capacity()*sizeof(PaintedMesh::SurfaceMap::Extra)+
    surface.tiles.capacity()*sizeof(PaintedMesh::SurfaceMap::Bounds)+surface.padding.capacity()*sizeof(int32_t)+
    surface.linked.size()*(sizeof(std::pair<const uint32_t,PaintedMesh::SurfaceMap::Head>)+2*sizeof(void*))+surface.linked.bucket_count()*sizeof(void*)+
    surface.diagnostics.unpaintable_uv_faces.capacity()*sizeof(uint32_t)+
    (surface.paint_domain?surface.paint_domain->retained_bytes():0);
}
std::shared_ptr<const detail::PaintOverlayPacket> detail::PaintOverlayFactory::capture_paired_bake(
    const PaintedMesh &paint,const std::shared_ptr<const PaintNormalProjection> &projection,
    const std::function<bool()> &cancelled) {
  if(projection!=paint.normal_projection()||
     projection.owner_before(paint.normal_projection())||paint.normal_projection().owner_before(projection))
    throw std::invalid_argument("Bake overlay lost the current prepared normal owner");
  return capture_impl(paint,cancelled,true,projection.get());
}
std::shared_ptr<const detail::PaintOverlayPacket> detail::PaintOverlayFactory::capture_impl(
    const PaintedMesh &paint,const std::function<bool()> &cancelled,bool paired_bake,
    const PaintNormalProjection *projection) {
  paint_compute::checkpoint(cancelled);
  if(!paint.valid_source_sampling_attachment())throw std::invalid_argument("Overlay capture lost its actual source coordinate/baked owner");
  auto packet=std::shared_ptr<PaintOverlayPacket>(new PaintOverlayPacket);
  packet->paired_bake_source_=paired_bake;
  if(paint.source_sampling_prepared()&&paint.baked_){
    const auto &baked=*paint.baked_;
    if(projection){
      if(baked.normal_interpretation!=BakedNormalInterpretation::PreparedAttributes||
         baked.normal_field||(baked.normal_authority&&
         (!projection->sampling_domain||baked.normal_authority!=projection->sampling_domain->detail)))
        throw std::invalid_argument("Bake overlay lost the prepared normal detail event");
    }else if(baked.normal_interpretation!=BakedNormalInterpretation::CanonicalMikk||baked.normal_authority||baked.normal_field)
      throw std::invalid_argument("Source8 overlay needs its explicit normal-field adapter");
    else packet->source_baked_geometry_=paint.mesh_owner();
  }
  packet->resolution_=paint.resolution();
  packet->padding_=std::shared_ptr<const std::vector<int32_t>>(paint.surface_,&paint.surface_->padding);
  packet->baked_=paint.baked_;packet->source_maps_=paint.source_maps_;
  if(const auto &proof=paint.surface_->original_sampling){packet->original_uv0_=proof->original_uv0;packet->original_uv1_=proof->original_uv1;}
  const size_t count=size_t(packet->resolution_/32)*(packet->resolution_/32);
  if(packet->resolution_<64||packet->resolution_>2048||(packet->resolution_&(packet->resolution_-1))||packet->padding_->size()!=size_t(packet->resolution_)*packet->resolution_||paint.layers_.size()>16)
    throw std::invalid_argument("Invalid native sparse overlay source");
  packet->layers_.reserve(paint.layers_.size());
  packet->empty_=!packet->baked_;
  // The aliasing padding pointer keeps its complete immutable SurfaceMap alive.
  // Charge that retained owner conservatively, not only the padding subvector.
  const auto &surface=*paint.surface_;
  packet->storage_bytes_=sizeof(PaintOverlayPacket)+raster_storage_bytes(paint);
  auto coordinate_storage=[](const PaintSourceMaps *maps){size_t n=0;if(!maps)return n;
    n=sizeof(PaintSourceMaps);if(maps->uv1)n+=maps->uv1->capacity()*sizeof(std::array<float,2>);
    for(const auto &[set,uv]:maps->extra_uv_sets)if(uv)n+=uv->capacity()*sizeof(std::array<float,2>)+sizeof(decltype(maps->extra_uv_sets)::value_type)+3*sizeof(void*);
    return n;};
  packet->storage_bytes_+=coordinate_storage(paint.source_maps_.get())+(paint.baked_?paint.baked_->storage_bytes():0);
  if(const auto &proof=surface.original_sampling){const auto &original=*proof->before;const auto &old=*original.surface_;const auto &m=old.mesh;
    packet->storage_bytes_+=sizeof(PaintedMesh::SurfaceMap::OriginalSampling)+proof->vertices.capacity()*sizeof(uint32_t)+(proof->original_uv0->capacity()+(proof->authored_uv1?proof->authored_uv1->capacity()+proof->original_uv1->capacity():0))*sizeof(std::array<float,2>)+sizeof(PaintedMesh)+sizeof(PaintedMesh::SurfaceMap)+
      (m.positions.capacity()+m.normals.capacity())*sizeof(Vec3)+m.uv.capacity()*sizeof(std::array<float,2>)+m.indices.capacity()*sizeof(uint32_t)+m.uv_pins.capacity()+m.uv_seams.capacity()+
      old.texels.capacity()*sizeof(PaintedMesh::SurfaceMap::Texel)+old.extra.capacity()*sizeof(PaintedMesh::SurfaceMap::Extra)+old.tiles.capacity()*sizeof(PaintedMesh::SurfaceMap::Bounds)+old.padding.capacity()*sizeof(int32_t)+
      old.linked.size()*(sizeof(std::pair<const uint32_t,PaintedMesh::SurfaceMap::Head>)+2*sizeof(void*))+old.linked.bucket_count()*sizeof(void*)+old.diagnostics.unpaintable_uv_faces.capacity()*sizeof(uint32_t)+coordinate_storage(original.source_maps_.get());
    for(const auto &layer:original.layers_)packet->storage_bytes_+=sizeof(PaintLayer)+layer.name.capacity()+(layer.tiles.capacity()+layer.pbr_tiles.capacity()+layer.emission_tiles.capacity())*sizeof(std::shared_ptr<const PaintTile>);
  }
  std::set<const PaintTile*> common_owners;std::set<const PaintColorTile*> color_owners;std::set<const PaintEmissionTile*> emission_owners;
  for(const auto &layer:paint.layers_){
    paint_compute::checkpoint(cancelled);
    if(!std::isfinite(layer.opacity)||layer.opacity<0||layer.opacity>1||layer.tiles.size()!=count||(!layer.pbr_tiles.empty()&&layer.pbr_tiles.size()!=count)||(!layer.emission_tiles.empty()&&layer.emission_tiles.size()!=count))
      throw std::invalid_argument("Invalid sparse overlay layer");
    if(layer.visible&&layer.opacity>0){
      packet->layers_.push_back({layer.opacity,layer.tiles,layer.pbr_tiles,layer.emission_tiles});
      packet->storage_bytes_+=(layer.tiles.size()+layer.pbr_tiles.size()+layer.emission_tiles.size())*sizeof(std::shared_ptr<const PaintTile>);
      for(const auto &tile:layer.tiles)if(tile){packet->empty_=false;if(common_owners.insert(tile.get()).second)packet->storage_bytes_+=sizeof(PaintTile);}
      for(const auto &tile:layer.pbr_tiles)if(tile&&color_owners.insert(tile.get()).second)packet->storage_bytes_+=sizeof(PaintColorTile);
      for(const auto &tile:layer.emission_tiles)if(tile&&emission_owners.insert(tile.get()).second)packet->storage_bytes_+=sizeof(PaintEmissionTile);
    }
  }
  paint_compute::checkpoint(cancelled);return packet;
}
std::shared_ptr<const TextureMaps> detail::PaintOverlayFactory::capture_source_baked(
    const PaintedMesh &paint,const std::function<bool()> &cancelled){
  paint_compute::checkpoint(cancelled);
  if(!paint.source_sampling_prepared()||!paint.baked_||!paint.valid_source_sampling_attachment()||
     !paint.source_maps_||paint.normal_projection_||paint.color_projection_||paint.paint_sampling_domain())
    throw std::invalid_argument("Source8 preview requires actual before/current/baked ownership");
  auto &cache=*paint.composite_cache_;
  {std::lock_guard lock(cache.mutex);
    if(cache.source_baked_token==paint.composite_token_)if(auto packet=cache.source_baked_packet.lock())
      if(packet->baked()==paint.baked_&&packet->source_maps()==paint.source_maps_&&packet->source_baked_geometry()==paint.mesh_owner()){
        auto view=std::make_shared<TextureMaps>();view->source_maps=paint.source_maps_;view->source_only=true;
        view->baked_base=paint.baked_;view->overlay=std::move(packet);view->transparent=cache.source_baked_transparent;return view;
      }
  }
  const auto r=paint.resolution();const size_t pixels=size_t(r)*r;
  const size_t extra=paint.baked_->maps.normal.rgba.empty()?0:pixels*8;
  if(extra>128*1024*1024||paint.storage_bytes()>128*1024*1024-extra)
    throw std::length_error("Source8 preview normal carriers exceed existing paint storage budget");
  auto result=std::make_shared<TextureMaps>();result->source_maps=paint.source_maps_;result->source_only=true;
  auto packet=std::const_pointer_cast<PaintOverlayPacket>(capture(paint,cancelled));
  result->baked_base=paint.baked_;
  if(extra){
    auto normal=std::make_shared<Image>(blank_image(r,true)),coverage=std::make_shared<Image>(blank_image(r));
    for(size_t at=0;at<pixels;++at){
      if((at&1023)==0)paint_compute::checkpoint(cancelled);
      coverage->rgba[at*4+3]=255;
      const auto owner=paint.surface_->padding[at];if(owner<0)continue;const size_t source=size_t(owner);
      std::copy_n(paint.baked_->maps.normal.rgba.data()+source*4,4,normal->rgba.data()+at*4);
      coverage->rgba[at*4]=paint.baked_->coverage[source];
    }
    packet->source_baked_normal_=std::move(normal);packet->source_baked_coverage_=std::move(coverage);packet->storage_bytes_+=extra;
  }
  result->overlay=std::move(packet);
  if(const auto &image=paint.source_maps_->color.image)for(size_t at=3;at<image->rgba.size();at+=4)if(image->rgba[at]!=255){result->transparent=true;break;}
  for(size_t at=0;at<pixels;++at)if(paint.baked_->coverage[at]&&paint.baked_->maps.color.rgba[at*4+3]!=255){result->transparent=true;break;}
  for(const auto &layer:result->overlay->layers())for(const auto &tile:layer.tiles)if(tile)
    for(const auto &pixel:tile->pixels)if(pixel.data[13]&&pixel.data[9]&&pixel.data[3]!=255){result->transparent=true;break;}
  paint_compute::checkpoint(cancelled);
  {std::lock_guard lock(cache.mutex);cache.source_baked_token=paint.composite_token_;cache.source_baked_packet=result->overlay;cache.source_baked_transparent=result->transparent;}
  return result;
}
std::shared_ptr<const TextureMaps> detail::PaintOverlayFactory::capture_current_grid(
    const PaintedMesh &paint,const Material &material,const std::function<bool()> &cancelled){
  paint_compute::checkpoint(cancelled);material.validate();
  if(paint.source_maps_||paint.baked_||paint.color_projection_||paint.normal_projection_||
     paint.surface_->paint_domain||paint.sampling_attachment_||paint.source_sampling_prepared()||
     paint.surface_->diagnostics.policy!=PaintUvPolicy::Strict)
    throw std::invalid_argument("Current-grid GPU preview requires its actual plain writable grid");
  auto &cache=*paint.composite_cache_;std::lock_guard lock(cache.mutex);
  auto previous=cache.current_grid_preview.lock();
  const auto padding=std::shared_ptr<const std::vector<int32_t>>(paint.surface_,&paint.surface_->padding);
  const std::array<float,9> factors{material.color[0],material.color[1],material.color[2],material.color[3],
    material.emissive[0],material.emissive[1],material.emissive[2],material.metallic,material.roughness};
  const auto key=PaintedMesh::CompositeCache::GridKey{paint.composite_token_,uintptr_t(paint.mesh_owner().get()),uintptr_t(padding.get()),std::bit_cast<std::array<uint32_t,9>>(factors)};
  if(cache.current_grid_access==UINT64_MAX){for(auto&[key,value]:cache.current_grids)value.used=0;cache.current_grid_access=0;}
  const auto access=++cache.current_grid_access;
  if(auto found=cache.current_grids.find(key);found!=cache.current_grids.end()){
    const auto held=found->second.owner.lock();
    if(held&&held->matches_material(material)&&held->geometry()==paint.mesh_owner()&&
       !held->geometry().owner_before(paint.mesh_owner())&&!paint.mesh_owner().owner_before(held->geometry())&&
       held->layers()->padding()==padding&&!held->layers()->padding().owner_before(padding)&&!padding.owner_before(held->layers()->padding())){
      found->second.used=access;cache.current_grid_preview=held;
      auto maps=std::make_shared<TextureMaps>();maps->transparent=held->transparent();maps->current_grid_preview=held;return maps;
    }
    cache.current_grids.erase(found);
  }
  auto packet=std::shared_ptr<CurrentGridPaintPreview>(new CurrentGridPaintPreview);
  packet->geometry_=paint.mesh_owner();packet->layers_=capture(paint,cancelled);
  packet->factors_=factors;
  packet->physical_=overlay_uses_float_emission(*packet->layers_)||
    std::any_of(material.emissive.begin(),material.emissive.end(),[](float value){return value>1;});
  const auto r=paint.resolution(),columns=r/32;const auto layers=packet->layers_->layers();
  packet->transparent_tiles_.resize(size_t(columns)*columns);
  const auto old=previous;
  bool compatible=old&&old->factors_[3]==packet->factors_[3]&&
    old->layers_->padding()==packet->layers_->padding()&&
    !old->layers_->padding().owner_before(packet->layers_->padding())&&!packet->layers_->padding().owner_before(old->layers_->padding())&&old->layers_->layers().size()==layers.size();
  if(compatible)for(size_t i=0;i<layers.size();++i)
    compatible&=old->layers_->layers()[i].opacity==layers[i].opacity;
  const bool transparent_base=material.color[3]<.999f;
  packet->transparent_=transparent_base&&
    std::find(packet->layers_->padding()->begin(),packet->layers_->padding()->end(),-1)!=packet->layers_->padding()->end();
  for(size_t tile=0;tile<packet->transparent_tiles_.size();++tile){
    if((tile&63)==0)paint_compute::checkpoint(cancelled);
    bool reuse=compatible;
    if(reuse)for(size_t i=0;i<layers.size();++i)reuse&=old->layers_->layers()[i].tiles[tile]==layers[i].tiles[tile];
    bool transparent=reuse?old->transparent_tiles_[tile]:false;
    const bool scan_alpha=!reuse&&(transparent_base||std::any_of(layers.begin(),layers.end(),
      [tile](const auto&layer){return bool(layer.tiles[tile]);}));
    if(scan_alpha)for(uint32_t y=0;y<32&&!transparent;++y)for(uint32_t x=0;x<32;++x){
      const auto px=uint32_t(tile%columns)*32+x,py=uint32_t(tile/columns)*32+y;
      if(packet->layers_->padding()->at(size_t(py)*r+px)==int32_t(size_t(py)*r+px)&&packet->alpha_texel(px,py)<.999f){transparent=true;break;}
    }
    packet->transparent_tiles_[tile]=transparent;packet->transparent_|=transparent;
  }
  paint_compute::checkpoint(cancelled);
  auto maps=std::make_shared<TextureMaps>();maps->transparent=packet->transparent_;
  maps->current_grid_preview=packet;packet->self_=packet;
  cache.current_grid_preview=packet;
  // Weak, bounded LRU metadata: simultaneous duplicate actors/materials must
  // keep their own unchanged data keys instead of invalidating one last entry.
  if(cache.current_grids.size()>=1024){auto oldest=cache.current_grids.begin();for(auto it=cache.current_grids.begin();it!=cache.current_grids.end();++it){if(it->second.owner.expired()){oldest=it;break;}if(it->second.used<oldest->second.used)oldest=it;}cache.current_grids.erase(oldest);}
  try{cache.current_grids.emplace(key,PaintedMesh::CompositeCache::GridEntry{packet,access});}catch(const std::bad_alloc&){}
  return maps;
}
std::shared_ptr<const detail::PaintOverlayPacket> detail::PaintOverlayFactory::capture_native_emission(
    const PaintedMesh &paint,const std::function<bool()> &cancelled) {
  if(paint.source_sampling_prepared()||paint.surface_diagnostics().policy==PaintUvPolicy::SourceAuthored)
    throw std::invalid_argument("Native emission packet cannot impersonate original source sampling");
  if(paint.paint_sampling_domain()&&!paint.valid_paint_sampling_attachment())
    throw std::invalid_argument("Native emission lost its exact writable geometry attachment");
  auto packet=std::const_pointer_cast<PaintOverlayPacket>(capture(paint,cancelled));
  packet->native_geometry_=paint.mesh_owner();packet->native_domain_=paint.paint_sampling_domain();
  return packet;
}
std::shared_ptr<const detail::PaintOverlayPacket> detail::PaintOverlayFactory::pack_source_maps(
    const std::shared_ptr<const PaintOverlayPacket> &packet,std::shared_ptr<const PaintSourceMaps> packed,
    std::span<const uint32_t> vertices,const std::function<bool()> &cancelled) {
  paint_compute::checkpoint(cancelled);
  if(!packet||!packet->source_maps_||!packed||vertices.empty()||vertices.size()>6'000'000)
    throw std::invalid_argument("Packed overlay requires its actual bounded source coordinate expansion");
  const auto &source=*packet->source_maps_;
  if(std::bit_cast<uint32_t>(source.normal_scale)!=std::bit_cast<uint32_t>(packed->normal_scale)||
     std::bit_cast<uint32_t>(source.occlusion_strength)!=std::bit_cast<uint32_t>(packed->occlusion_strength)||
     source.extra_uv_sets.size()!=packed->extra_uv_sets.size())
    throw std::invalid_argument("Packed overlay changed source material sampling");
  const std::array<const PaintSourceTexture*,5> a{&source.color,&source.metal_rough,&source.emission,&source.normal,&source.occlusion},
    b{&packed->color,&packed->metal_rough,&packed->emission,&packed->normal,&packed->occlusion};
  for(size_t k=0;k<a.size();++k)if(a[k]->image!=b[k]->image||a[k]->image.owner_before(b[k]->image)||b[k]->image.owner_before(a[k]->image)||
      a[k]->wrap_s!=b[k]->wrap_s||a[k]->wrap_t!=b[k]->wrap_t||a[k]->min_filter!=b[k]->min_filter||a[k]->mag_filter!=b[k]->mag_filter||
      a[k]->texcoord!=b[k]->texcoord||std::bit_cast<std::array<uint32_t,6>>(a[k]->uv_transform)!=std::bit_cast<std::array<uint32_t,6>>(b[k]->uv_transform))
    throw std::invalid_argument("Packed overlay changed original image/view ownership");
  auto coordinates=[&](const auto &before,const auto &after){
    if(bool(before)!=bool(after)|| (after&&after->size()!=vertices.size()))throw std::invalid_argument("Packed overlay coordinate domain differs");
    if(!before)return;
    for(size_t v=0;v<vertices.size();++v){
      if((v&1023)==0)paint_compute::checkpoint(cancelled);
      if(vertices[v]>=before->size()||std::bit_cast<std::array<uint32_t,2>>(after->at(v))!=std::bit_cast<std::array<uint32_t,2>>(before->at(vertices[v])))
        throw std::invalid_argument("Packed overlay coordinates differ from exact source vertex expansion");
    }
  };
  coordinates(source.uv1,packed->uv1);
  for(const auto &[set,values]:source.extra_uv_sets){auto it=packed->extra_uv_sets.find(set);
    if(it==packed->extra_uv_sets.end())throw std::invalid_argument("Packed overlay changed original coordinate identities");coordinates(values,it->second);}
  auto out=std::shared_ptr<PaintOverlayPacket>(new PaintOverlayPacket);
  out->contribution_owner_=packet->contribution_owner_?packet->contribution_owner_:packet;
  out->source_maps_=std::move(packed);
  auto logical=[&](const auto &input){
    using Coordinates=std::vector<std::array<float,2>>;
    if(!input)return std::shared_ptr<const Coordinates>{};
    // Reuse only a coordinate owner which was already proven by the exact
    // source-map expansion above. Value equality alone grants no authority.
    auto same_owner=[&](const auto &v){return input==v&&!input.owner_before(v)&&!v.owner_before(input);};
    if(same_owner(source.uv1))return out->source_maps_->uv1;
    for(const auto &[set,uv]:source.extra_uv_sets)if(same_owner(uv))return out->source_maps_->extra_uv_sets.at(set);
    auto values=std::make_shared<Coordinates>();values->reserve(vertices.size());
    for(size_t v=0;v<vertices.size();++v){if((v&1023)==0)paint_compute::checkpoint(cancelled);values->push_back(input->at(vertices[v]));}
    return std::shared_ptr<const Coordinates>(std::move(values));
  };
  out->original_uv0_=logical(packet->original_uv0_);out->original_uv1_=logical(packet->original_uv1_);
  out->storage_bytes_=sizeof(PaintOverlayPacket)+sizeof(PaintSourceMaps);
  if(out->source_maps_->uv1)out->storage_bytes_+=out->source_maps_->uv1->capacity()*sizeof(std::array<float,2>);
  for(const auto &[set,uv]:out->source_maps_->extra_uv_sets)out->storage_bytes_+=uv->capacity()*sizeof(std::array<float,2>)+sizeof(decltype(out->source_maps_->extra_uv_sets)::value_type)+3*sizeof(void*);
  if(out->original_uv0_)out->storage_bytes_+=out->original_uv0_->capacity()*sizeof(std::array<float,2>);
  paint_compute::checkpoint(cancelled);return out;
}
PaintEmissionValue PaintedMesh::emission_pixel(uint64_t id,uint32_t x,uint32_t y) const {
  const auto &p=pixel(id,x,y);const auto &l=layer(id);
  if(!l.emission_tiles.empty())if(const auto &tile=l.emission_tiles.at(size_t(y/32)*surface_->columns+x/32))
    return tile->pixels[size_t(y%32)*32+x%32];
  PaintEmissionValue result;
  for(size_t k=0;k<3;++k)result.legacy[k]=srgb_to_linear(unit(p.data[6+k]));
  result.coverage=unit(p.data[12]);
  return result;
}
bool PaintedMesh::has_physical_emission() const noexcept {
  if(baked_&&baked_->emission_field)return true;
  for(const auto &l:layers_)if(emission_tile_count(l))return true;
  return false;
}
void PaintedMesh::begin_stroke() {
  if(!detail::TextureSetPaintAccess::valid_raster(*this))throw std::logic_error("Texture-set painting lost its exact logical raster/geometry");
  detail::require_editable_texture_atlas(*this);
  if (editing_)
    throw std::logic_error("A paint stroke is already active");
  if (!active().visible)
    throw std::logic_error("Show the active paint layer before painting");
  before_ = active().tiles;
  before_colors_ = active().pbr_tiles;
  before_emission_ = active().emission_tiles;
  before_allocated_tiles_ = tile_count(active());
  before_allocated_color_tiles_ = color_tile_count(active());
  before_composite_token_=composite_token_;
  editing_ = true;
}
bool PaintedMesh::pending_changes() const {
  return editing_ && (before_ != layer(active_).tiles || before_colors_ != layer(active_).pbr_tiles || before_emission_!=layer(active_).emission_tiles);
}
void PaintedMesh::stamp(const Brush &b, const PaintSettings &settings) {
  stamp_sampled(b,settings,{});
}
void PaintedMesh::stamp(const Brush &b,const PaintSettings &settings,const std::function<bool()> &cancelled) {stamp_sampled(b,settings,{},cancelled);}
void PaintedMesh::stamp_sampled(const Brush &b,const PaintSettings &settings,const PaintSampler &sampler,const std::function<bool()> &cancelled,const Evaluation *evaluation) {
  auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Paint stroke cancelled");};stop();
  settings.validate();
  if (!editing_)
    throw std::logic_error("Begin the paint stroke first");
  if (!finite(b.from) || !finite(b.to) || !finite(b.normal) ||
      !std::isfinite(b.radius) || b.radius <= 0 || b.radius > 5 ||
      !std::isfinite(b.pressure) || b.pressure < 0 || b.pressure > 1 ||
      !std::isfinite(b.strength) || b.strength < 0 || b.strength > 1)
    throw std::invalid_argument("Invalid paint stroke");
  if (!b.pressure || !b.strength || !settings.flow)
    return;
  float radius = b.radius * std::sqrt(b.pressure);
  Vec3 margin{radius, radius, radius},
      lo = low(b.from, b.to) - margin, hi = high(b.from, b.to) + margin,
      axis = b.to - b.from, n = normalized(b.normal);
  float axis2 = dot(axis, axis);
  auto &l = active();
  if(color_projection_ && l.pbr_tiles.size()!=l.tiles.size())throw std::logic_error("Prepared color layer is incomplete");
  auto token=l.visible&&l.opacity>0?next_composite_token():composite_token_;
  auto prior = l.tiles;
  auto prior_colors = l.pbr_tiles;
  auto prior_emission = l.emission_tiles;
  size_t emission_bytes=0;for(const auto &layer:layers_)emission_bytes+=emission_tile_count(layer)*sizeof(PaintEmissionTile);
  const size_t original_emission_bytes=emission_bytes;
  size_t allocated = allocated_tiles_;
  size_t allocated_colors = allocated_color_tiles_;
  const size_t retained_storage=storage_bytes()-allocated_tiles_*sizeof(PaintTile)-
    allocated_color_tiles_*sizeof(PaintColorTile)-original_emission_bytes;
  const uint64_t source_working=source_sampling_prepared()?SourcePaintAccess::working_bytes(*this):0;
  bool changed = false;
  size_t candidate_checks=0;
  try {
    for (size_t tile = 0; tile < l.tiles.size(); ++tile) {
      stop();
      auto bounds = evaluation?evaluation->bounds[tile]:surface_->tiles[tile];
      if (bounds.hi.x < lo.x || bounds.hi.y < lo.y || bounds.hi.z < lo.z ||
          bounds.lo.x > hi.x || bounds.lo.y > hi.y || bounds.lo.z > hi.z)
        continue;
      auto copy = l.tiles[tile] ? std::make_shared<PaintTile>(*l.tiles[tile])
                                : std::make_shared<PaintTile>();
      std::shared_ptr<PaintColorTile> color_copy;
      std::shared_ptr<PaintEmissionTile> emission_copy;
      bool color_changed=false;
      bool emission_changed=false;
      bool tile_changed = false;
      uint32_t tx = uint32_t(tile % surface_->columns) * 32,
               ty = uint32_t(tile / surface_->columns) * 32;
      for (uint32_t y = 0; y < 32; ++y)
        for (uint32_t x = 0; x < 32; ++x) {
          size_t index=size_t(ty+y)*resolution()+tx+x;
          const auto &t=surface_->texels[index];
          float distance=INFINITY,projection_opacity=1;
          uint32_t selected_face=UINT32_MAX;auto selected=settings;
          auto consider=[&](const SurfaceMap::Texel &candidate){
            if(!candidate.covered)return;
            if((candidate_checks&255)==0)stop();
            if(++candidate_checks>surface_->options.max_candidate_checks)throw std::length_error("Paint candidate work budget exceeded");
            if(texture_set_domain_&&!texture_set_domain_->contains(candidate.face))return;
            auto evaluated=evaluation?evaluation->sample(candidate):std::pair{candidate.position,candidate.normal};
            auto position=evaluated.first,normal=evaluated.second;
            if(settings.front_only&&dot(normal,n)<=.05f)return;
            float u=axis2>1e-20f?std::clamp(dot(position-b.from,axis)/axis2,0.f,1.f):0;
            float d=length(position-(b.from+axis*u));
            if(d>=radius||d>distance+1e-7f||(std::abs(d-distance)<=1e-7f&&candidate.face>=selected_face))return;
            auto value=settings;float opacity=1;
            if(sampler){auto sample=sampler(position,normal);if(!sample)return;value=sample->settings;opacity=sample->opacity;}
            if(opacity<=0)return;
            distance=d;selected_face=candidate.face;selected=value;projection_opacity=opacity;
          };
          consider(t);
          if(!surface_->linked.empty()){
            auto found=surface_->linked.find(uint32_t(index));
            if(found!=surface_->linked.end())for(auto at=found->second.first;at!=UINT32_MAX;at=surface_->extra[at].next)
              consider(surface_->extra[at].texel);
          }
          if(selected_face==UINT32_MAX)continue;
          float edge = 1 - distance * distance / (radius * radius),
                alpha = std::clamp(edge * edge * b.pressure * b.strength *
                                       settings.flow * projection_opacity,
                                   0.f, 1.f);
          if (!alpha) continue;
          auto &p = copy->pixels[y * 32 + x];
          auto old = p;
          if (selected.channel == PaintChannel::Mask)
            p.data[13] =
                byte(unit(p.data[13]) * (1 - alpha) + selected.mask * alpha);
          else {
            int channel = int(selected.channel), coverage = 9 + channel;
            const bool linear_emission=channel==3&&(selected.emission_units==PaintEmissionUnits::Physical||(!l.emission_tiles.empty()&&l.emission_tiles[tile]));
            PaintEmissionValue *linear=nullptr,old_linear;
            if(linear_emission) {
              if(!emission_copy) {
                // Absolute zero and fractional coverage both require their
                // genuine owning plane, even if fallback bytes do not change.
                emission_changed|=l.emission_tiles.empty()||!l.emission_tiles[tile];
                emission_copy=std::make_shared<PaintEmissionTile>();
                if(!l.emission_tiles.empty()&&l.emission_tiles[tile])*emission_copy=*l.emission_tiles[tile];
                else if(l.tiles[tile])for(size_t i=0;i<emission_copy->pixels.size();++i){
                  auto &value=emission_copy->pixels[i];const auto &q=l.tiles[tile]->pixels[i];
                  for(size_t k=0;k<3;++k)value.legacy[k]=srgb_to_linear(unit(q.data[6+k]));
                  value.coverage=unit(q.data[12]);
                }
              }
              linear=&emission_copy->pixels[y*32+x];old_linear=*linear;
            }
            float previous = linear?linear->coverage:unit(p.data[coverage]);
            if (selected.erase)
              {const float next=previous*(1-alpha);p.data[coverage]=byte(next);if(linear)linear->coverage=next;}
            else {
              float next = alpha + previous * (1 - alpha);
              p.data[coverage] = byte(next);
              auto mix = [&](int i, float value, bool srgb) {
                float old_value = unit(p.data[i]);
                if (srgb)
                  old_value = srgb_to_linear(old_value);
                float result =
                    (value * alpha + old_value * previous * (1 - alpha)) /
                    std::max(next, 1e-12f);
                p.data[i] = byte(srgb ? linear_to_srgb(result) : result);
              };
              if (channel == 0) {
                if(color_projection_) {
                  if(!color_copy) {
                    color_copy=std::make_shared<PaintColorTile>();
                    if(l.pbr_tiles[tile])*color_copy=*l.pbr_tiles[tile];
                    else if(l.tiles[tile])for(size_t i=0;i<color_copy->pixels.size();++i) {
                      const auto &q=l.tiles[tile]->pixels[i];color_copy->pixels[i]={q.data[0],q.data[1],q.data[2],q.data[3]};
                    }
                  }
                  auto &q=color_copy->pixels[y*32+x];const auto old_color=q;
                  for(int k=0;k<4;++k) {
                    float old_value=unit(q[k]);if(k<3)old_value=srgb_to_linear(old_value);
                    const float result=(selected.color[k]*alpha+old_value*previous*(1-alpha))/std::max(next,1e-12f);
                    q[k]=byte(k<3?linear_to_srgb(result):result);
                  }
                  color_changed|=q!=old_color;
                }
                for (int k = 0; k < 4; ++k)
                  mix(k, selected.color[k], k < 3);
              }
              else if (channel == 1)
                mix(4, selected.metallic, false);
              else if (channel == 2)
                mix(5, selected.roughness, false);
              else if(linear) {
                auto &value=*linear;value.coverage=next;
                for(size_t k=0;k<3;++k) {
                  const float relative=selected.emission_units==PaintEmissionUnits::MaterialRelative?selected.emission[k]:0;
                  const float physical=selected.emission_units==PaintEmissionUnits::Physical?selected.emission[k]:0;
                  // lerp is bounded by its endpoints, including the exact
                  // maximum physical value; do not clip a rounded quotient.
                  value.legacy[k]=std::lerp(value.legacy[k],relative,alpha/next);
                  value.physical[k]=std::lerp(value.physical[k],physical,alpha/next);
                  p.data[6+k]=byte(linear_to_srgb(value.legacy[k]));
                }
              } else for (int k = 0; k < 3; ++k)mix(6 + k, selected.emission[k], true);
            }
            if(linear)emission_changed|=*linear!=old_linear;
          }
          tile_changed |= p != old;
        }
      if (tile_changed || color_changed || emission_changed) {
        if(!l.tiles[tile])++allocated;
        if(color_changed && !l.pbr_tiles[tile])++allocated_colors;
        if(emission_changed&&(l.emission_tiles.empty()||!l.emission_tiles[tile]))emission_bytes+=sizeof(PaintEmissionTile);
        const uint64_t added=uint64_t(allocated-allocated_tiles_)*sizeof(PaintTile)+
          uint64_t(allocated_colors-allocated_color_tiles_)*sizeof(PaintColorTile)+emission_bytes-original_emission_bytes;
        if(retained_storage+allocated*sizeof(PaintTile)+allocated_colors*sizeof(PaintColorTile)+emission_bytes>128*1024*1024||
           (source_sampling_prepared()&&source_working+added>SourcePaintOptions::maximum_working_bytes))
          throw std::length_error("Paint storage budget exceeded");
        l.tiles[tile] = std::move(copy);
        if(color_changed)l.pbr_tiles[tile]=std::move(color_copy);
        if(emission_changed){if(l.emission_tiles.empty())l.emission_tiles.resize(l.tiles.size());l.emission_tiles[tile]=std::move(emission_copy);}
        changed = true;
      }
    }
    stop();
  } catch (...) {
    l.tiles = std::move(prior);
    l.pbr_tiles = std::move(prior_colors);
    l.emission_tiles = std::move(prior_emission);
    throw;
  }
  allocated_tiles_ = allocated;
  allocated_color_tiles_ = allocated_colors;
  if (changed){
    composite_token_=token;
    ++revision_;
  }
}
bool PaintedMesh::end_stroke() {
  if (!editing_)
    return false;
  editing_ = false;
  auto &tiles = active().tiles;
  auto &colors=active().pbr_tiles;
  auto &emission=active().emission_tiles;
  if (before_ == tiles && before_colors_ == colors && before_emission_==emission) {
    before_.clear();
    before_colors_.clear();
    before_emission_.clear();
    return false;
  }
  size_t bytes = 0;
  for (size_t i = 0; i < tiles.size(); ++i)
    if (before_[i] != tiles[i])
      bytes += sizeof(PaintTile) * 2;
  for(size_t i=0;i<colors.size();++i)if(before_colors_[i]!=colors[i])bytes+=sizeof(PaintColorTile)*2;
  for(size_t i=0;i<std::max(before_emission_.size(),emission.size());++i)
    if((i<before_emission_.size()?before_emission_[i]:nullptr)!=(i<emission.size()?emission[i]:nullptr))bytes+=sizeof(PaintEmissionTile)*2;
  for (auto &s : redo_)
    history_bytes_ -= s.bytes;
  redo_.clear();
  undo_.push_back({active_, std::move(before_), tiles, bytes,before_allocated_tiles_,tile_count(active()),std::move(before_colors_),colors,before_allocated_color_tiles_,color_tile_count(active()),std::move(before_emission_),emission});
  history_bytes_ += bytes;
  while (undo_.size() > 1 &&
         (history_bytes_ > 64 * 1024 * 1024 || undo_.size() > 128)) {
    history_bytes_ -= undo_.front().bytes;
    undo_.pop_front();
  }
  return true;
}
void PaintedMesh::cancel_stroke() {
  if (editing_) {
    allocated_tiles_ = allocated_tiles_ - tile_count(active()) + before_allocated_tiles_;
    active().tiles = std::move(before_);
    allocated_color_tiles_=allocated_color_tiles_-color_tile_count(active())+before_allocated_color_tiles_;
    active().pbr_tiles=std::move(before_colors_);
    active().emission_tiles=std::move(before_emission_);
    editing_ = false;
    composite_token_=before_composite_token_;
    ++revision_;
  }
}
bool PaintedMesh::undo() {
  if (editing_)
    throw std::logic_error("Finish the paint stroke before undo");
  if (undo_.empty())
    return false;
  const auto &target=layer(undo_.back().layer);
  auto token=target.visible&&target.opacity>0?next_composite_token():composite_token_;
  auto s = std::move(undo_.back());
  undo_.pop_back();
  active_ = s.layer;
  allocated_tiles_ = allocated_tiles_ - tile_count(active()) + s.before_tiles;
  active().tiles = s.before;
  allocated_color_tiles_=allocated_color_tiles_-color_tile_count(active())+s.before_color_tiles;
  active().pbr_tiles=s.color_before;
  active().emission_tiles=s.emission_before;
  redo_.push_back(std::move(s));
  composite_token_=token;
  ++revision_;
  return true;
}
bool PaintedMesh::redo() {
  if (editing_)
    throw std::logic_error("Finish the paint stroke before redo");
  if (redo_.empty())
    return false;
  const auto &target=layer(redo_.back().layer);
  auto token=target.visible&&target.opacity>0?next_composite_token():composite_token_;
  auto s = std::move(redo_.back());
  redo_.pop_back();
  active_ = s.layer;
  allocated_tiles_ = allocated_tiles_ - tile_count(active()) + s.after_tiles;
  active().tiles = s.after;
  allocated_color_tiles_=allocated_color_tiles_-color_tile_count(active())+s.after_color_tiles;
  active().pbr_tiles=s.color_after;
  active().emission_tiles=s.emission_after;
  undo_.push_back(std::move(s));
  composite_token_=token;
  ++revision_;
  return true;
}
namespace {
void finish_polygon_maps(TextureMaps &maps,const PaintedMesh &paint,
    const std::shared_ptr<const SourceMaterialBindings> &binding,
    std::shared_ptr<const detail::PaintOverlayPacket> packet,
    const std::shared_ptr<const detail::PolygonSurfacePreview> &cached,
    const std::function<bool()> &cancelled){
  maps.baked_base=paint.baked();
  if(paint.baked()&&paint.baked()->normal_field){
    maps.normal=*paint.baked()->normal_field->pbr();
    maps.mtoon_normal=*paint.baked()->normal_field->mtoon();
  }
  maps.overlay=std::move(packet);
  detail::PolygonSurfaceTransferLimits limits;
  limits.geometry.cancelled=cancelled;
  if(cached&&!cached->matches(binding,paint.paint_sampling_attachment()))
    throw std::invalid_argument("Cached polygon preview borrowed another actual source/material attachment");
  maps.polygon_surface=cached?cached->with_bake(paint):detail::PolygonSurfacePreview::create(paint,binding,limits);
  if(const auto &normal=maps.polygon_surface->native_bake())maps.normal=*normal;
  maps.polygon_surface->validate_maps(maps);
  paint_compute::checkpoint(cancelled);
}
}

std::shared_ptr<const TextureMaps> detail::PolygonSurfacePaintCodec::preview(const PaintedMesh&paint,
    const std::shared_ptr<const SourceMaterialBindings>&binding,const Material&material,const std::function<bool()>&cancelled,
    std::shared_ptr<const detail::PolygonSurfacePreview>cached){
  paint_compute::checkpoint(cancelled);material.validate();if(!binding||!binding->polygon_surface_transfer)throw std::invalid_argument("Polygon preview requires its actual current owning field");
  binding->validate();const auto&owner=binding->polygon_surface_transfer;owner->validate_current(paint);
  const size_t pixels=size_t(paint.resolution())*paint.resolution(),limit=256*1024*1024;size_t prospective=paint.storage_bytes();
  const auto charge=[&](size_t n,size_t unit){if(prospective>limit||n>(limit-prospective)/unit)throw std::length_error("Polygon preview exceeds prospective material/float workspace");prospective+=n*unit;};
  charge(pixels,64);charge(paint.mesh().positions.size(),128);charge(paint.mesh().indices.size(),512);
  auto packet=PaintOverlayFactory::capture_native_emission(paint,cancelled);
  const auto hdr=[](const auto&v){return std::any_of(v.begin(),v.end(),[](float x){return x>1;});};
  const bool physical=overlay_uses_float_emission(*packet)||hdr(material.emissive)||binding->physical_emission||hdr(binding->native_reference.emission);
  auto detached=physical?EmissionExportAccess::without_emission(paint):paint.snapshot();
  for(auto&layer:detached->layers_)layer.emission_tiles.clear();
  if(detached->baked_){auto material_bake=std::make_shared<BakedTextures>(*detached->baked_);material_bake->normal_field.reset();material_bake->maps.normal={};material_bake->maps.mtoon_normal={};material_bake->normal_interpretation=BakedNormalInterpretation::CanonicalMikk;detached->baked_=std::move(material_bake);}
  auto safe=material;if(physical)safe.emissive={};
  // Only colour/ORM/emission use this original-grid composite. The full genuine
  // normal field remains separately owned and is evaluated by the typed range.
  auto maps=detached->build_composite(safe,cancelled);
  finish_polygon_maps(*maps,paint,binding,std::move(packet),cached,cancelled);
  return maps;
}
std::shared_ptr<TextureMaps> detail::PolygonSurfacePaintCodec::preview_retained_material(
    const TextureSetView&field,size_t &work,const TextureSetLimits&limits,
    std::shared_ptr<const PolygonSurfacePreview> cached) {
  limits.validate();
  paint_compute::checkpoint(limits.cancelled);
  if(!retained_material_grid(field))
    throw std::invalid_argument("Retained preview requires its actual PBR material member");
  field.source->authenticate_owner(field.source);
  const auto paint=field.source->read_view(field,limits);
  const auto &binding=field.material_bindings;
  if(!cached)cached=field.source->source_preview();
  binding->validate();
  binding->polygon_surface_transfer->validate_current(*paint);
  field.material.validate();
  const auto mode=retained_grid_mode(field);
  const auto normal_bytes=retained_hdr_normal_bytes(field);
  const auto map_bytes=native_material_grid_bytes(*paint,mode);
  if(map_bytes>limits.maximum_bytes||normal_bytes>limits.maximum_bytes-map_bytes)
    throw std::length_error("Retained preview exceeds its material and normal byte limit");
  if(work>limits.maximum_work||normal_bytes/2>limits.maximum_work-work)
    throw std::length_error("Retained normal carriage exceeds its work limit");
  work+=normal_bytes/2; // Copy and validate both normal images at texel granularity.
  auto maps=compile_native_material_grid(*paint,field.material,work,limits,mode);
  auto packet=PaintOverlayFactory::capture_native_emission(*paint,limits.cancelled);
  finish_polygon_maps(*maps,*paint,binding,std::move(packet),cached,limits.cancelled);
  return maps;
}
bool detail::EmissionExportAccess::requires_preview(const PaintedMesh &paint,const Material &material,
    const SourceMaterialBindings *binding) {
  return !paint.source_sampling_prepared()&&paint.surface_diagnostics().policy!=PaintUvPolicy::SourceAuthored&&
    (paint.has_physical_emission()||std::any_of(material.emissive.begin(),material.emissive.end(),[](float v){return v>1;})||
     (binding&&(binding->physical_emission||(!binding->native_paint_sampling&&std::any_of(binding->native_reference.emission.begin(),binding->native_reference.emission.end(),[](float v){return v>1;})))));
}
std::shared_ptr<const TextureMaps> detail::EmissionExportAccess::preview(const PaintedMesh &paint,const Material &material,
    const std::function<bool()> &cancelled) {
  paint_compute::checkpoint(cancelled);material.validate();
  const CompositeFactors factors{material.color,material.emissive,material.metallic,material.roughness};
  const NativeEmissionOwner owner{paint.mesh_owner(), paint.paint_sampling_domain()};
  auto &cache=*paint.composite_cache_;
  const auto remember = [&](const std::shared_ptr<const TextureMaps> &maps) {
    std::lock_guard lock(cache.mutex);
    cache.native_emission_token = paint.composite_token_;
    cache.native_emission_factors = factors;
    cache.native_emission_preview = maps;
  };
  std::shared_ptr<const TextureMaps> previous;
  {
    std::lock_guard lock(cache.mutex);
    if (cache.native_emission_token == paint.composite_token_ && cache.native_emission_factors == factors)
      previous = cache.native_emission_preview.lock();
  }
  auto &retention = composite_retention();
  if (previous && owner.matches(*previous)) {
    retention.retain(previous, paint.composite_token_, factors, true, CompositeUse::NativeEmission);
    return previous;
  }
  // Editable history can outlive the renderer's last frame. Restore an exact
  // preview through the shared byte-bounded cache when that frame is gone.
  if (auto retained = retention.find(paint.composite_token_, factors, &owner)) {
    remember(retained);
    return retained;
  }
  // Deep normal replay may legitimately visit a before-Paint that shares this
  // metadata cache. Build without holding its mutex, then publish completed maps.
  auto packet=PaintOverlayFactory::capture_native_emission(paint,cancelled);
  const bool legacy=!overlay_uses_float_emission(*packet)&&
    std::all_of(material.emissive.begin(),material.emissive.end(),[](float value){return value<=1;});
  std::shared_ptr<PaintedMesh> detached;auto safe=material;
  if(legacy){
    // Invisible float metadata must not reinterpret existing visible legacy
    // filtering/quantization. Keep that genuine composite as well as the real
    // packet; the renderer still selects float for an absolute/HDR context.
    auto fresh=std::make_shared<PaintedMesh::CompositeCache>();const auto token=next_composite_token();
    detached=paint.snapshot();detached->composite_cache_=std::move(fresh);detached->composite_token_=token;
    for(auto &layer:detached->layers_)layer.emission_tiles.clear();
  }else{detached=without_emission(paint);safe.emissive={};}
  // Complete the fresh maps before publishing. A short-lived sanitized paint
  // snapshot has no caller that can reuse a separate cached composite.
  const bool source_capture = detached->surface_->source_capture_only;
  const auto &domain = detached->surface_->paint_domain;
  if (domain && domain->version() == 3)
    throw std::invalid_argument("Polygon surface preview/export requires its actual current field adapter");
  auto maps = source_capture ? std::make_shared<TextureMaps>(*detached->composite(safe, cancelled))
                             : detached->build_composite(safe, cancelled);
  // The images contain the safe legacy channels. The attached bake remains
  // the original immutable owner already retained by the HDR packet.
  if (!source_capture && maps->baked_base)
    maps->baked_base = packet->baked();
  maps->overlay=std::move(packet);
  // The existing compositor refuses unresolved alternate original coordinates.
  // Do not silently reinterpret those coordinates as the native writable grid.
  paint_compute::checkpoint(cancelled);
  remember(maps);
  retention.retain(maps, paint.composite_token_, factors, false, CompositeUse::NativeEmission);
  return maps;
}
std::shared_ptr<const TextureMaps> detail::EmissionExportAccess::preview_texture_set_bake(
    const PaintedMesh &paint,const Material &material,const std::function<bool()> &cancelled) {
  if(!paint.texture_set_domain_||!paint.baked_||!paint.baked_->emission_field||!detail::native_hdr_grid(paint))
    return {};
  paint_compute::checkpoint(cancelled);
  material.validate();
  const CompositeFactors factors{material.color,material.emissive,material.metallic,material.roughness};
  auto &cache=*paint.composite_cache_;
  {
    std::lock_guard lock(cache.mutex);
    if(cache.texture_set_bake_token==paint.composite_token_&&cache.texture_set_bake_factors==factors)
      if(auto held=cache.texture_set_bake_preview.lock())
        if(NativeEmissionOwner{paint.mesh_owner(),{}}.matches(*held))return held;
  }
  TextureSetLimits limits;limits.cancelled=cancelled;size_t work=0;
  auto maps=detail::compile_native_material_grid(paint,material,work,limits,
      detail::TextureSetGridMode::NativeHdr);
  maps->baked_base=paint.baked_;
  maps->overlay=PaintOverlayFactory::capture_native_emission(paint,cancelled);
  paint_compute::checkpoint(cancelled);
  std::lock_guard lock(cache.mutex);
  cache.texture_set_bake_token=paint.composite_token_;
  cache.texture_set_bake_factors=factors;
  cache.texture_set_bake_preview=maps;
  return maps;
}
std::span<const int32_t> detail::EmissionExportAccess::writable_padding(const PaintedMesh &paint) {
  if(!paint.surface_||paint.editing_||!paint.valid_source_sampling_attachment()||
     !paint.valid_paint_sampling_attachment())
    throw std::invalid_argument("HDR raster needs its actual finished paint and sampling owners");
  const auto &surface=*paint.surface_;
  const size_t resolution=surface.resolution;
  if(resolution<64||resolution>2048||(resolution&(resolution-1))||
     surface.padding.size()!=resolution*resolution||paint.layers_.size()>16)
    throw std::invalid_argument("Invalid editable HDR export raster");
  if(paint.source_sampling_prepared()&&paint.baked_&&
     (paint.baked_->normal_interpretation!=BakedNormalInterpretation::CanonicalMikk||
      paint.baked_->normal_authority||paint.baked_->normal_field))
    throw std::invalid_argument("Prepared HDR raster needs its exact native normal-domain adapter");
  for(const auto &layer:paint.layers_)
    if(!std::isfinite(layer.opacity)||layer.opacity<0||layer.opacity>1||
       layer.tiles.size()!=surface.tiles.size()||
       (!layer.emission_tiles.empty()&&layer.emission_tiles.size()!=surface.tiles.size()))
      throw std::invalid_argument("Invalid editable HDR raster layer");
  return surface.padding;
}
std::shared_ptr<PaintedMesh> detail::EmissionExportAccess::without_emission(const PaintedMesh &paint) {
  // An altered temporary may never publish maps under the original's visual
  // content identity, including actual physical zero/hidden contributions.
  auto cache=std::make_shared<PaintedMesh::CompositeCache>();
  const auto token=next_composite_token();
  auto detached=paint.snapshot();
  detached->composite_cache_=std::move(cache);detached->composite_token_=token;
  if(detached->baked_&&detached->baked_->emission_field){
    auto baked=std::make_shared<BakedTextures>(*detached->baked_);
    baked->emission_field.reset();baked->maps.emission=blank_image(paint.resolution());
    detached->baked_=std::move(baked);
  }
  for(auto &layer:detached->layers_){layer.emission_tiles.clear();for(auto &tile:layer.tiles)if(tile){bool emission=false;for(const auto &pixel:tile->pixels)emission|=pixel.data[6]||pixel.data[7]||pixel.data[8]||pixel.data[12];if(emission){auto copy=std::make_shared<PaintTile>(*tile);for(auto &pixel:copy->pixels){pixel.data[6]=pixel.data[7]=pixel.data[8]=pixel.data[12]=0;}tile=std::move(copy);}}}
  return detached;
}
detail::NativeWeightPaintAccess::Paint detail::NativeWeightPaintAccess::paint(const PaintedMesh &paint,bool prospective) {
  if (!paint.surface_ || paint.editing_ || (!prospective&&(!paint.undo_.empty() || !paint.redo_.empty())))
    throw std::invalid_argument("Native weight input must be a detached current-paint snapshot");
  if (!detail::TextureSetPaintAccess::valid_raster(paint))
    throw std::invalid_argument("Native weight raster lost its texture-set membership");
  return {surface(paint.surface_),paint.sampling_attachment_,paint.color_projection_,
          paint.normal_projection_,paint.baked_};
}
detail::NativeWeightPaintAccess::Surface detail::NativeWeightPaintAccess::surface(
    const PaintSamplingAttachment &attachment) {
  auto stored=std::static_pointer_cast<const PaintedMesh::SurfaceMap>(attachment.surface_);
  if(!stored||!attachment.matches(stored->geometry,stored->paint_domain))
    throw std::invalid_argument("Native weight input lost its paint sampling attachment");
  return surface(stored);
}
detail::NativeWeightPaintAccess::Surface detail::NativeWeightPaintAccess::surface(
    const std::shared_ptr<const PaintedMesh::SurfaceMap> &owner) {
  const auto &surface=*owner;
  if(surface.options.cancelled)
    throw std::invalid_argument("Native weight input retained a mutable paint callback");
  size_t bytes=sizeof(PaintedMesh::SurfaceMap)+64;
  auto add=[&](size_t count,size_t unit){if(unit&&count>(SIZE_MAX-bytes)/unit)throw std::length_error("Native paint retained-byte overflow");bytes+=count*unit;if(count){const size_t overhead=count*unit>=128u*1024u?4096:64;if(overhead>SIZE_MAX-bytes)throw std::length_error("Native paint allocation allowance overflow");bytes+=overhead;}};
  add(surface.diagnostics.unpaintable_uv_faces.capacity(),sizeof(uint32_t));
  const auto surface_bytes = bytes;
  bytes = sizeof(PaintedMesh::SurfaceMap::Raster)+64;
  add(surface.texels.capacity(),sizeof(PaintedMesh::SurfaceMap::Texel));
  add(surface.extra.capacity(),sizeof(PaintedMesh::SurfaceMap::Extra));
  add(surface.tiles.capacity(),sizeof(PaintedMesh::SurfaceMap::Bounds));add(surface.padding.capacity(),sizeof(int32_t));
  add(surface.linked.size(),sizeof(PaintedMesh::SurfaceMap::Links::value_type)+3*sizeof(void*)+32);
  add(surface.linked.bucket_count(),sizeof(void*));
  Surface result;
  result.owner=owner;
  result.raster=surface.raster_data;
  result.geometry=surface.geometry;
  result.membership=surface.set_domain;
  result.domain=surface.paint_domain;
  result.bytes=surface_bytes;
  result.raster_bytes=bytes;
  if(const auto &original=surface.original_sampling){
    bytes=sizeof(*original);
    add(original->vertices.capacity(),sizeof(uint32_t));
    result.original_sampling=original;
    result.original_bytes=bytes;
    result.before=original->before;
    result.original_maps=original->current_maps;
    result.original_bake=original->baked;
    result.original_uvs={original->original_uv0,original->original_uv1,original->authored_uv1};
  }
  return result;
}
bool detail::NativeWeightPaintAccess::same_current(const PaintedMesh&a,const PaintedMesh&b) noexcept {
  using namespace native_weight_state;
  // Current authoring state only. Undo scratch, weak render caches and cache
  // tokens are not a source authority; immutable sparse tile owners are.
  if(a.editing_||b.editing_||!owner(a.surface_,b.surface_)||!owner(a.texture_set_domain_,b.texture_set_domain_)||!owner(a.sampling_attachment_,b.sampling_attachment_)||!owner(a.source_maps_,b.source_maps_)||!owner(a.color_projection_,b.color_projection_)||!owner(a.normal_projection_,b.normal_projection_)||!owner(a.baked_,b.baked_)||a.active_!=b.active_||a.next_!=b.next_||a.revision_!=b.revision_||a.allocated_tiles_!=b.allocated_tiles_||a.allocated_color_tiles_!=b.allocated_color_tiles_||a.layers_.size()!=b.layers_.size())return false;
  auto tiles=[](const auto&x,const auto&y){if(x.size()!=y.size())return false;for(size_t i=0;i<x.size();++i)if(!owner(x[i],y[i]))return false;return true;};
  for(size_t i=0;i<a.layers_.size();++i){const auto&x=a.layers_[i];const auto&y=b.layers_[i];if(x.id!=y.id||x.name!=y.name||x.visible!=y.visible||!scalar(x.opacity,y.opacity)||!tiles(x.tiles,y.tiles)||!tiles(x.pbr_tiles,y.pbr_tiles)||!tiles(x.emission_tiles,y.emission_tiles))return false;}
  return true;
}
std::shared_ptr<PaintedMesh> detail::NativeWeightPaintAccess::snapshot(const PaintedMesh &paint) {
  auto cache=std::make_shared<PaintedMesh::CompositeCache>();
  auto result=paint.snapshot();result->composite_cache_=std::move(cache);return result;
}
std::shared_ptr<PaintedMesh> PaintedMesh::snapshot() const {
  auto p = std::shared_ptr<PaintedMesh>(new PaintedMesh);
  p->composite_cache_=composite_cache_;
  p->composite_token_=composite_token_;
  p->surface_ = surface_;
  p->texture_set_domain_ = texture_set_domain_;
  p->sampling_attachment_ = sampling_attachment_;
  p->source_maps_ = source_maps_;
  p->color_projection_ = color_projection_;
  p->normal_projection_ = normal_projection_;
  p->layers_ = layers_;
  p->baked_ = baked_;
  p->active_ = active_;
  p->next_ = next_;
  p->revision_ = revision_;
  p->allocated_tiles_ = allocated_tiles_;
  p->allocated_color_tiles_ = allocated_color_tiles_;
  return p;
}
const std::shared_ptr<const TextureSetFaceDomain>&detail::TextureSetPaintAccess::domain(const PaintedMesh&paint)noexcept{return paint.texture_set_domain_;}
std::shared_ptr<const void>detail::TextureSetPaintAccess::raster_owner(const PaintedMesh&paint)noexcept{return paint.surface_;}
bool detail::TextureSetPaintAccess::valid_raster(const PaintedMesh&paint)noexcept{
 return paint.surface_&&paint.surface_->set_domain==paint.texture_set_domain_&&(!paint.texture_set_domain_||paint.texture_set_domain_->matches(paint.mesh_owner()));
}
size_t detail::TextureSetPaintAccess::snapshot_bytes(const PaintedMesh&paint){
 size_t bytes=sizeof(PaintedMesh)+sizeof(PaintedMesh::CompositeCache);
 auto add=[&](size_t n,size_t unit){if(unit&&n>(SIZE_MAX-bytes)/unit)throw std::length_error("Texture-set snapshot metadata overflow");bytes+=n*unit;};
 for(const auto&layer:paint.layers_){add(1,sizeof(PaintLayer));add(layer.name.size()+1,1);add(layer.tiles.size()+layer.pbr_tiles.size()+layer.emission_tiles.size(),sizeof(std::shared_ptr<const PaintTile>));}return bytes;
}
bool detail::TextureSetPaintAccess::same_current(const PaintedMesh&a,const PaintedMesh&b)noexcept{
 // Identity/value association only; no normal/source interpretation is granted.
 return NativeWeightPaintAccess::same_current(a,b);
}
bool detail::TextureSetPaintAccess::stroke_structure(const PaintedMesh&a,const PaintedMesh&b)noexcept{
 using namespace native_weight_state;
 if(a.editing_||!valid_raster(a)||!valid_raster(b)||!owner(a.surface_,b.surface_)||!owner(a.texture_set_domain_,b.texture_set_domain_)||!owner(a.sampling_attachment_,b.sampling_attachment_)||!owner(a.source_maps_,b.source_maps_)||!owner(a.color_projection_,b.color_projection_)||!owner(a.normal_projection_,b.normal_projection_)||!owner(a.baked_,b.baked_)||a.active_!=b.active_||a.next_!=b.next_||a.layers_.size()!=b.layers_.size())return false;
 for(size_t i=0;i<a.layers_.size();++i){const auto&x=a.layers_[i];const auto&y=b.layers_[i];if(x.id!=y.id||x.name!=y.name||x.visible!=y.visible||!scalar(x.opacity,y.opacity))return false;}return true;
}
void detail::TextureSetPaintAccess::swap_stroke(PaintedMesh&target,PaintedMesh&sealed){
 if(sealed.in_stroke()||!stroke_structure(target,sealed))throw std::invalid_argument("Texture-set stroke adoption lost its immutable structure");
 target.swap_state(sealed);
}
size_t detail::TextureSetPaintAccess::initial_raster_bytes(uint32_t r){
 if(r<64||r>2048||(r&(r-1)))throw std::invalid_argument("Invalid initial texture-set resolution");
 return sizeof(PaintedMesh)+sizeof(PaintedMesh::CompositeCache)+sizeof(PaintedMesh::SurfaceMap)+sizeof(PaintLayer)+256+
   size_t(r)*r*(sizeof(PaintedMesh::SurfaceMap::Texel)+2*sizeof(int32_t))+size_t(r/32)*(r/32)*(sizeof(PaintedMesh::SurfaceMap::Bounds)+sizeof(std::shared_ptr<const PaintTile>));
}
std::shared_ptr<PaintedMesh> detail::TextureSetPaintAccess::empty(std::shared_ptr<const TextureSetFaceDomain>domain,uint32_t r,const TextureSetLimits&limits){
 limits.validate();if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Initial texture-set raster cancelled");if(domain)domain->authenticate_owner(domain);
 if(!domain||!domain->topology()||!domain->topology()->geometry()||!domain->matches(domain->topology()->geometry()))throw std::invalid_argument("Initial paint raster needs its actual owning membership");
 if(initial_raster_bytes(r)>limits.maximum_bytes)throw std::length_error("Initial paint raster exceeds its prospective bytes");
 PaintSurfaceOptions options;options.policy=PaintUvPolicy::Strict;options.cancelled=limits.cancelled;
 options.max_raster_samples=std::min(options.max_raster_samples,limits.maximum_work);options.max_candidate_checks=std::min(options.max_candidate_checks,limits.maximum_work);
 auto geometry=domain->topology()->geometry();
 return std::shared_ptr<PaintedMesh>(new PaintedMesh(std::move(geometry),r,options,{},std::move(domain)));
}
size_t detail::TextureSetPaintAccess::working_bytes(const PaintedMesh&paint,uint32_t r){
  if(r<64||r>2048||(r&(r-1)))throw std::invalid_argument("Invalid texture-set resolution");
  size_t bytes=sizeof(PaintedMesh)+sizeof(PaintedMesh::CompositeCache);
  for(const auto&layer:paint.layers_){bytes+=sizeof(PaintLayer)+layer.name.size()+1;
    bytes+=(r==paint.resolution()?layer.tiles.size()+layer.pbr_tiles.size()+layer.emission_tiles.size():size_t(r/32)*(r/32))*sizeof(std::shared_ptr<const PaintTile>);}
  bytes+=sizeof(PaintedMesh::SurfaceMap)+size_t(r)*r*(sizeof(PaintedMesh::SurfaceMap::Texel)+2*sizeof(int32_t))+size_t(r/32)*(r/32)*sizeof(PaintedMesh::SurfaceMap::Bounds);
  return bytes;
}
bool detail::TextureSetPaintAccess::valid_source_member(const PaintedMesh&member,const PaintedMesh&original,
    const std::shared_ptr<const TextureSetFaceDomain>&domain,const PaintedMesh *current_bake)noexcept{
  using native_weight_state::owner;
  return !(!domain||!owner(member.texture_set_domain_,domain)||!member.surface_||!owner(member.surface_->set_domain,domain)||!domain->matches(original.mesh_owner())||!owner(member.mesh_owner(),original.mesh_owner())||member.editing_||original.editing_||member.resolution()!=original.resolution()||!member.valid_paint_sampling_attachment()||!original.valid_paint_sampling_attachment()||!member.valid_source_sampling_attachment()||!original.valid_source_sampling_attachment()||!owner(member.surface_->paint_domain,original.surface_->paint_domain)||!owner(member.surface_->original_sampling,original.surface_->original_sampling)||!owner(member.source_maps_,original.source_maps_)||!owner(member.color_projection_,original.color_projection_)||!owner(member.normal_projection_,original.normal_projection_)||!owner(member.baked_,current_bake?current_bake->baked_:original.baked_));
}
std::shared_ptr<const PaintedMesh>detail::TextureSetPaintAccess::field_source_view(const PaintedMesh&member,
    const PaintedMesh&original,const std::shared_ptr<const TextureSetFaceDomain>&domain,const TextureSetLimits&limits,const PaintedMesh *current_bake){
  using native_weight_state::owner;
  limits.validate();if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Texture-set source read view cancelled");
  if(domain)domain->authenticate_owner(domain);
  if(!valid_source_member(member,original,domain,current_bake))throw std::invalid_argument("Texture-set member cannot borrow another original raster/source attachment");
  if(snapshot_bytes(member)>limits.maximum_bytes)throw std::length_error("Texture-set source read metadata exceeds its byte quota");
  auto readable=member.snapshot();readable->surface_=original.surface_;readable->texture_set_domain_=original.texture_set_domain_;readable->sampling_attachment_=original.sampling_attachment_;readable->composite_cache_=std::make_shared<PaintedMesh::CompositeCache>();readable->composite_token_=next_composite_token();
  if(!valid_raster(*readable)||!readable->valid_paint_sampling_attachment()||!readable->valid_source_sampling_attachment())throw std::invalid_argument("Texture-set source view failed its actual original raster proof");
  if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Texture-set source read view cancelled");return readable;
}
void detail::TextureSetPaintAccess::restore_source_bake(PaintedMesh &paint,const PaintedMesh &original){
  const auto token=next_composite_token();
  paint.baked_=original.baked_;
  paint.composite_token_=token;
  ++paint.revision_;
}
std::shared_ptr<PaintedMesh>detail::TextureSetPaintAccess::bind_raster(const PaintedMesh&before,
    std::shared_ptr<const TextureSetFaceDomain>domain,const PaintedMesh&prototype,const TextureSetLimits&limits){
  using native_weight_state::owner;limits.validate();if(domain)domain->authenticate_owner(domain);
  if(!domain||!valid_raster(before)||!valid_raster(prototype)||!owner(prototype.texture_set_domain_,domain)||!owner(prototype.mesh_owner(),before.mesh_owner())||!domain->matches(before.mesh_owner())||prototype.in_stroke()||before.in_stroke()||prototype.resolution()!=before.resolution()||prototype.surface_->options.policy!=before.surface_->options.policy||!owner(prototype.surface_->paint_domain,before.surface_->paint_domain)||!owner(prototype.surface_->original_sampling,before.surface_->original_sampling))throw std::invalid_argument("Source member raster cannot borrow another real source/domain/resolution proof");
  if(snapshot_bytes(before)>limits.maximum_bytes)throw std::length_error("Source member raster metadata exceeds its quota");auto result=before.snapshot();result->surface_=prototype.surface_;result->texture_set_domain_=domain;result->sampling_attachment_=prototype.sampling_attachment_;result->composite_cache_=std::make_shared<PaintedMesh::CompositeCache>();result->composite_token_=next_composite_token();return result;
}
bool detail::TextureSetPaintAccess::can_resize_raw(const PaintedMesh &paint)noexcept{
  return valid_raster(paint)&&!paint.in_stroke()&&!paint.texture_set_domain_&&
    !paint.sampling_attachment_&&!paint.color_projection_&&!paint.normal_projection_&&!paint.baked_&&
    !paint.surface_->paint_domain&&!paint.surface_->original_sampling&&!paint.surface_->source_capture_only&&
    paint.surface_->options.policy!=PaintUvPolicy::PolygonAuthored;
}
bool detail::TextureSetPaintAccess::can_resize_unpainted(const PaintedMesh &paint)noexcept{
  return can_resize_raw(paint)&&!paint.allocated_tiles_&&!paint.allocated_color_tiles_&&
    !paint.has_physical_emission();
}
bool detail::TextureSetPaintAccess::grid_covered(const PaintedMesh &paint,size_t index)noexcept{
  return index<paint.surface_->texels.size()&&paint.surface_->texels[index].covered;
}
std::shared_ptr<PaintedMesh>detail::TextureSetPaintAccess::resize_unpainted(const PaintedMesh&before,
    uint32_t r,const TextureSetLimits&limits){
  limits.validate();auto stop=[&]{if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Target paint resolution change cancelled");};stop();
  if(r<64||r>2048||(r&(r-1))||!can_resize_unpainted(before))
    throw std::invalid_argument("Target resizing requires an unpainted writable UV grid");
  if(r==before.resolution())return before.snapshot();
  if(before.revision_==UINT64_MAX)throw std::overflow_error("Target paint revision exhausted");
  const size_t after_bytes=working_bytes(before,r),before_bytes=working_bytes(before,before.resolution());
  // Each additional authored candidate can allocate a vector slot, a hash
  // entry and buckets. Charge capacity growth and the old live raster as well.
  constexpr size_t candidate_bytes=2*sizeof(PaintedMesh::SurfaceMap::Extra)+
    sizeof(std::pair<const uint32_t,PaintedMesh::SurfaceMap::Head>)+8*sizeof(void*);
  if(before_bytes>limits.maximum_bytes||after_bytes>limits.maximum_bytes-before_bytes)
    throw std::length_error("Target resolution exceeds its prospective before/after raster byte budget");
  const auto available=limits.maximum_bytes-before_bytes-after_bytes;
  const auto existing=before.surface_->extra.size();
  if(existing>available/candidate_bytes)
    throw std::length_error("Target resolution exceeds its prospective linked-raster byte budget");
  const auto &mesh=before.mesh();size_t work=0,samples=0;
  auto charge=[&](size_t count,size_t unit){if(work>limits.maximum_work||(unit&&count>(limits.maximum_work-work)/unit))throw std::length_error("Target resolution exceeds its prospective work budget");work+=count*unit;};
  charge(mesh.positions.size()+mesh.normals.size()+mesh.uv.size()+mesh.indices.size(),4);charge(size_t(r)*r,20);
  for(size_t triangle=0;triangle<mesh.indices.size()/3;++triangle){
    if(!(triangle%64))stop();const auto a=mesh.uv.at(mesh.indices[triangle*3]),b=mesh.uv.at(mesh.indices[triangle*3+1]),c=mesh.uv.at(mesh.indices[triangle*3+2]);
    const int x0=std::max(0,int(std::floor(std::min({a[0],b[0],c[0]})*r))),x1=std::min(int(r)-1,int(std::ceil(std::max({a[0],b[0],c[0]})*r))),
      y0=std::max(0,int(std::floor(std::min({a[1],b[1],c[1]})*r))),y1=std::min(int(r)-1,int(std::ceil(std::max({a[1],b[1],c[1]})*r)));
    const size_t count=size_t(std::max(0,x1-x0+1))*size_t(std::max(0,y1-y0+1));charge(count,1);samples+=count;
  }
  auto options=before.surface_->options;options.cancelled=limits.cancelled;
  options.max_raster_samples=std::min({options.max_raster_samples,std::max(size_t(1),samples),limits.maximum_work});
  if(options.policy==PaintUvPolicy::SourceAuthored){
    if(work==limits.maximum_work)
      throw std::length_error("Target resolution leaves no work budget for linked source UVs");
    options.max_candidate_checks=std::min(options.max_candidate_checks,limits.maximum_work-work);
  }
  options.max_additional_candidates=std::min(options.max_additional_candidates,available/candidate_bytes-existing);
  auto surface=std::make_shared<PaintedMesh::SurfaceMap>(before.surface_->geometry,r,std::move(options));
  surface->options.max_raster_samples=before.surface_->options.max_raster_samples;
  surface->options.max_additional_candidates=before.surface_->options.max_additional_candidates;
  surface->options.max_candidate_checks=before.surface_->options.max_candidate_checks;
  auto result=before.snapshot();result->surface_=std::move(surface);
  const size_t tiles=result->surface_->tiles.size();
  for(auto &layer:result->layers_){layer.tiles.assign(tiles,{});layer.pbr_tiles.clear();layer.emission_tiles.clear();}
  result->allocated_tiles_=result->allocated_color_tiles_=0;
  result->composite_cache_=std::make_shared<PaintedMesh::CompositeCache>();result->composite_token_=next_composite_token();++result->revision_;
  if(result->mesh_owner()!=before.mesh_owner()||result->source_maps_!=before.source_maps_||
     !result->valid_paint_sampling_attachment()||!result->valid_source_sampling_attachment())
    throw std::invalid_argument("Target resolution lost its original geometry or image ownership");
  stop();return result;
}
std::shared_ptr<PaintedMesh>detail::TextureSetPaintAccess::bind(const PaintedMesh&before,
    std::shared_ptr<const TextureSetFaceDomain>domain,uint32_t r,const TextureSetLimits&limits){
  limits.validate();if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Texture-set paint binding cancelled");if(domain)domain->authenticate_owner(domain);
  if(!domain||!domain->matches(before.mesh_owner())||before.in_stroke()||!before.valid_paint_sampling_attachment()||!before.valid_source_sampling_attachment())throw std::invalid_argument("Texture-set paint needs its actual finished geometry/source owners");
  size_t peak=working_bytes(before,r);
  if(peak>limits.maximum_bytes)throw std::length_error("Texture-set paint binding exceeds its working-byte budget");
  if(r!=before.resolution()||before.surface_->set_domain!=domain){
    const auto&mesh=before.mesh();size_t work=0,samples=0;
    auto charge_work=[&](size_t count,size_t unit){if(work>limits.maximum_work||(unit&&count>(limits.maximum_work-work)/unit))throw std::length_error("Texture-set raster preparation exceeds its work forecast");work+=count*unit;};
    charge_work(mesh.positions.size()+mesh.normals.size()+mesh.uv.size(),4);charge_work(mesh.indices.size(),4);charge_work(size_t(r)*r,20);
    const auto uv=[&](size_t corner){if(const auto&basis=before.surface_->paint_domain){const auto source=basis->source_corners()[corner];return basis->basis()->uv.at(basis->basis()->indices.at(source));}return mesh.uv.at(mesh.indices.at(corner));};
    for(auto face:domain->triangles()){
      if((face&1023)==0&&limits.cancelled&&limits.cancelled())throw std::runtime_error("Texture-set raster forecast cancelled");
      const auto a=uv(size_t(face)*3),b=uv(size_t(face)*3+1),c=uv(size_t(face)*3+2);
      const int x0=std::max(0,int(std::floor(std::min({a[0],b[0],c[0]})*r))),x1=std::min(int(r)-1,int(std::ceil(std::max({a[0],b[0],c[0]})*r)));
      const int y0=std::max(0,int(std::floor(std::min({a[1],b[1],c[1]})*r))),y1=std::min(int(r)-1,int(std::ceil(std::max({a[1],b[1],c[1]})*r)));
      const size_t count=size_t(std::max(0,x1-x0+1))*std::max(0,y1-y0+1);charge_work(count,1);samples+=count;
    }
    const auto candidates=before.surface_->options.policy==PaintUvPolicy::Strict||domain->triangles().size()<2?0:std::min(samples,before.surface_->options.max_additional_candidates);
    const size_t stride=2*sizeof(PaintedMesh::SurfaceMap::Extra)+8*sizeof(void*)+sizeof(PaintedMesh::SurfaceMap::Head)+sizeof(uint32_t);
    if(candidates>(limits.maximum_bytes-peak)/stride)throw std::length_error("Texture-set overlapping raster candidates exceed their prospective storage bound");
    peak+=candidates*stride;
  }
  if(r!=before.resolution()&&(before.has_paint_data()||before.color_projection_||before.normal_projection_))throw std::logic_error("Changing a painted texture-set resolution requires the measured all-channel resampling worker");
  auto result=before.snapshot();
  // Independent sets must not share the single native-emission metadata slot:
  // different material factors/tokens would evict each other's live preview.
  // Pixel COW ownership and the generic value token remain unchanged.
  result->composite_cache_=std::make_shared<PaintedMesh::CompositeCache>();
  if(r!=before.resolution()||before.surface_->set_domain!=domain){
    auto options=before.surface_->options;options.cancelled=limits.cancelled;
    options.max_raster_samples=std::min(options.max_raster_samples,limits.maximum_work);
    options.max_candidate_checks=std::min(options.max_candidate_checks,limits.maximum_work);
    PaintSamplingLimits sampling_limits;sampling_limits.cancelled=limits.cancelled;
    auto raster=std::make_shared<PaintedMesh::SurfaceMap>(before.surface_->geometry,r,std::move(options),before.surface_->paint_domain,sampling_limits,domain);
    raster->original_sampling=before.surface_->original_sampling;
    result->surface_=std::move(raster);
    if(r!=before.resolution())for(auto&layer:result->layers_){layer.tiles.assign(result->surface_->tiles.size(),{});layer.pbr_tiles.clear();layer.emission_tiles.clear();}
    // The new membership owns its raster/padding; COW authored values remain
    // exact. Equal geometry does not authorize a different material's texels.
    result->composite_token_=next_composite_token();
    if(result->revision_==UINT64_MAX)throw std::overflow_error("Texture-set paint revision exhausted");++result->revision_;
  }
  result->texture_set_domain_=std::move(domain);result->seal_paint_sampling_attachment();
  if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Texture-set paint binding cancelled");return result;
}
std::shared_ptr<PaintedMesh> detail::TextureSetPaintAccess::resample_plain(const PaintedMesh&before,
    const TextureSetResolutionOptions&options,const TextureSetLimits&limits,TextureSetResolutionReport&report){
 options.validate();limits.validate();const auto domain=before.texture_set_domain_;
 if(domain)domain->authenticate_owner(domain);
 if((!domain&&!can_resize_raw(before))||(domain&&(!valid_raster(before)||before.in_stroke()||!domain->matches(before.mesh_owner())||
    before.paint_sampling_domain()||before.source_maps_||before.source_sampling_prepared()||
    before.sampling_attachment_||before.color_projection_||before.normal_projection_||before.baked_||
    before.surface_->original_sampling||before.surface_->source_capture_only)))
  throw std::invalid_argument("Resampling requires actual plain member geometry/grid owners");
 if(options.resolution==before.resolution())return before.snapshot();
 if(before.revision_==UINT64_MAX)throw std::overflow_error("Texture-set paint revision exhausted");
 // Bound actual authored candidates before allocating layer output or raster.
 // Mesh/domain still keep the original logical triangle sequence.
 const size_t old_candidates=before.surface_->extra.capacity()*sizeof(PaintedMesh::SurfaceMap::Extra)+before.surface_->linked.bucket_count()*sizeof(void*)+before.surface_->linked.size()*(sizeof(PaintedMesh::SurfaceMap::Head)+4*sizeof(void*)+sizeof(uint32_t));
 size_t candidate_bytes=old_candidates,samples=0;const auto r=options.resolution;const auto&mesh=before.mesh();
 if(domain&&before.surface_->options.policy!=PaintUvPolicy::Strict&&
    (before.surface_->options.policy!=PaintUvPolicy::PolygonAuthored||!domain->topology()->polygon_ids()))
  throw std::invalid_argument("Source-authored grid resizing needs its actual source transition");
 if(!domain){
  const size_t fixed=size_t(r)*r*20+4*(mesh.positions.size()+mesh.normals.size()+mesh.uv.size()+mesh.indices.size());
  if(fixed>limits.maximum_work-report.work)throw std::length_error("Native raster initialization exceeds its prospective work bound");
  report.work+=fixed;
 }
 const size_t triangle_count=domain?domain->triangles().size():mesh.indices.size()/3;
 for(size_t index=0;index<triangle_count;++index){
  if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Texture-set candidate forecast cancelled");
  const auto triangle=domain?domain->triangles()[index]:index;
  const auto a=mesh.uv.at(mesh.indices.at(size_t(triangle)*3)),b=mesh.uv.at(mesh.indices.at(size_t(triangle)*3+1)),c=mesh.uv.at(mesh.indices.at(size_t(triangle)*3+2));
  const int x0=std::max(0,int(std::floor(std::min({a[0],b[0],c[0]})*r))),x1=std::min(int(r)-1,int(std::ceil(std::max({a[0],b[0],c[0]})*r)));
  const int y0=std::max(0,int(std::floor(std::min({a[1],b[1],c[1]})*r))),y1=std::min(int(r)-1,int(std::ceil(std::max({a[1],b[1],c[1]})*r)));
  const size_t n=size_t(std::max(0,x1-x0+1))*std::max(0,y1-y0+1);
  if(n>limits.maximum_work-samples)throw std::length_error("Texture-set raster samples exceed their prospective work bound");samples+=n;
 }
 const bool authored=before.surface_->options.policy!=PaintUvPolicy::Strict;
 const size_t max_checks=authored?std::min(before.surface_->options.max_candidate_checks,samples*before.surface_->options.max_candidates_per_texel):0;
 const bool bounded_source=!domain&&authored;
 const size_t reserved_checks=bounded_source?0:max_checks;
 if(samples>limits.maximum_work-report.work||reserved_checks>limits.maximum_work-report.work-samples)throw std::length_error("Texture-set raster/candidate work exceeds the prospective complete budget");
 auto filter_limits=limits;filter_limits.maximum_work-=samples+reserved_checks;
 constexpr size_t stride=2*sizeof(PaintedMesh::SurfaceMap::Extra)+8*sizeof(void*)+sizeof(PaintedMesh::SurfaceMap::Head)+sizeof(uint32_t);
 if(authored&&!bounded_source){
  const size_t candidates=triangle_count<2?0:std::min(samples,before.surface_->options.max_additional_candidates);
  if(candidate_bytes>limits.maximum_bytes||candidates>(limits.maximum_bytes-candidate_bytes)/stride)throw std::length_error("Texture-set candidate storage exceeds its prospective byte bound");candidate_bytes+=candidates*stride;
 }
 if(candidate_bytes>=filter_limits.maximum_bytes)throw std::length_error("Texture-set candidates exhaust its peak budget");filter_limits.maximum_bytes-=candidate_bytes;
 auto layers=detail::resample_texture_set_layers(before,options,filter_limits,report,!domain);report.peak_bytes+=candidate_bytes;
 auto raster_options=before.surface_->options;raster_options.cancelled=limits.cancelled;
 raster_options.max_raster_samples=std::min({raster_options.max_raster_samples,std::max(size_t(1),samples),limits.maximum_work-report.work});
 raster_options.max_candidate_checks=std::min(raster_options.max_candidate_checks,std::max(size_t(1),max_checks));
 if(bounded_source){
  raster_options.max_additional_candidates=std::min(raster_options.max_additional_candidates,(limits.maximum_bytes-report.peak_bytes)/stride);
  raster_options.max_candidate_checks=std::max(size_t(1),std::min(raster_options.max_candidate_checks,limits.maximum_work-report.work-samples));
 }
 PaintSamplingLimits sampling_limits;sampling_limits.cancelled=limits.cancelled;
 auto surface=std::make_shared<PaintedMesh::SurfaceMap>(before.surface_->geometry,options.resolution,std::move(raster_options),nullptr,sampling_limits,domain);
 // The exact current-grid forecast bounds this construction only. Retain the
 // original caller ceilings for a later larger grid, rather than persisting a
 // downsample's smaller sample/check forecast as a lifetime size restriction.
 surface->options.max_raster_samples=before.surface_->options.max_raster_samples;
 surface->options.max_candidate_checks=before.surface_->options.max_candidate_checks;
 surface->options.max_additional_candidates=before.surface_->options.max_additional_candidates;
 if(bounded_source)report.peak_bytes+=surface->extra.size()*stride;
 const auto raster_work=surface->diagnostics.raster_samples+surface->diagnostics.candidate_checks;
 if(raster_work>limits.maximum_work-report.work)throw std::length_error("Texture-set resolution raster exceeds complete work budget");report.work+=raster_work;
 auto result=before.snapshot();result->surface_=std::move(surface);result->layers_=std::move(layers);
 result->allocated_tiles_=result->allocated_color_tiles_=0;
 for(const auto&l:result->layers_){result->allocated_tiles_+=tile_count(l);result->allocated_color_tiles_+=color_tile_count(l);}
 result->composite_cache_=std::make_shared<PaintedMesh::CompositeCache>();result->composite_token_=next_composite_token();++result->revision_;
 if(result->storage_bytes()>128*1024*1024)throw std::length_error("Resampled field exceeds the native per-field storage bound");
 if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Texture-set resolution cancelled");return result;
}
struct SourcePaintSamplingAccess {
  static size_t composite_work(const PaintedMesh &,detail::NativeGridComposite,size_t,const std::function<bool()> &);
  static const void *scene_raster(const PaintedMesh &paint) noexcept {
    const auto &surface=paint.surface_;
    if(!surface||!surface->paint_domain||surface->paint_domain->version()!=1||
       surface->options.policy!=PaintUvPolicy::Strict||surface->set_domain||
       !surface->sampling_raster_proven)return nullptr;
    return surface->raster_data.get();
  }

  static detail::NativeBakeConversion convert_native_bake(const SculptObject &object,
      const NormalPreparationOptions &options) {
    const auto stop=[&]{if(options.cancelled&&options.cancelled())throw std::runtime_error("Native Bake conversion cancelled");};
    stop();
    if(!detail::native_bake_conversion_supported(object))
      throw std::invalid_argument("Native Bake conversion requires its own historical paint and authored normal frame");
    if(!std::isfinite(options.max_angular_error)||options.max_angular_error<0||options.max_angular_error>.25||
        !options.max_working_bytes||options.max_working_bytes>512*1024*1024||
        !options.max_samples||options.max_samples>64'000'000)
      throw std::invalid_argument("Invalid native Bake conversion limits");
    const auto &paint=*object.painted;
    const auto &binding=*object.material_bindings;
    const auto &surface=*paint.surface_;
    const auto &bake=*paint.baked_;
    if(paint.revision_==UINT64_MAX)throw std::overflow_error("Paint revision exhausted");
    if(surface.diagnostics.policy!=PaintUvPolicy::Strict||surface.diagnostics.additional_candidates||
        !surface.diagnostics.unpaintable_uv_faces.empty())
      throw std::invalid_argument("Native Bake conversion requires the strict retained writable grid");
    bake.validate(paint.resolution());
    // The output will share paint layers, the raster and FLOAT emission.
    // Charge the actual copied byte planes and the per-layer pointer tables.
    size_t copy_bytes=bake.maps.color.rgba.size()+bake.maps.metal_rough.rgba.size()+
        bake.maps.emission.rgba.size()+bake.coverage.size();
    for(const auto &layer:paint.layers_)
      copy_bytes+=(layer.tiles.size()+layer.pbr_tiles.size()+layer.emission_tiles.size())*sizeof(std::shared_ptr<const void>);
    if(copy_bytes>=options.max_working_bytes||
        paint.mesh().indices.size()*256+surface.padding.size()*8>options.max_working_bytes-copy_bytes)
      throw std::length_error("Native Bake conversion exceeds the paint and raster workspace budget");
    auto reduced=options;
    reduced.max_working_bytes-=copy_bytes;
    binding.native_paint_frames->validate_binding(binding,false,options.cancelled);
    detail::normal_frame_current_authority(binding,paint.mesh(),object.deformation.get(),options.cancelled);
    const auto raster=detail::native_paint_bake_mesh(paint.mesh(),*paint.paint_sampling_domain(),options.cancelled);
    const auto texel=[&](size_t at){
      const auto &point=surface.texels.at(at);
      if(!point.covered)throw std::logic_error("Native Bake conversion found an unowned paint texel");
      return detail::NativeBakeTexel{point.face,{point.bary[0],point.bary[1],1.-point.bary[0]-point.bary[1]}};
    };
    auto converted=detail::convert_native_bake_grid(raster,*binding.normal_frames->pbr,bake,surface.padding,texel,reduced);
    stop();
    auto output=std::make_shared<BakedTextures>();
    output->maps.color=bake.maps.color;
    output->maps.metal_rough=bake.maps.metal_rough;
    output->maps.emission=bake.maps.emission;
    output->maps.transparent=bake.maps.transparent;
    output->maps.normal=std::move(converted.normal);
    output->coverage=bake.coverage;
    output->color_interpretation=bake.color_interpretation;
    output->normal_interpretation=BakedNormalInterpretation::NativeAttributes;
    output->emission_field=bake.emission_field;
    output->validate(paint.resolution());
    auto result=paint.snapshot();
    result->baked_=std::move(output);
    result->composite_token_=next_composite_token();
    ++result->revision_;
    stop();
    return {std::move(result),converted.maximum_sample_error};
  }

  static size_t surface_retained_bytes(size_t vertices,size_t corners,uint32_t r){
    (void)surface_working_bytes(vertices,corners,r);
    return sizeof(PaintedMesh::SurfaceMap)+sizeof(PaintedMesh)+sizeof(PaintSamplingAttachment)+
      vertices*(2*sizeof(Vec3)+sizeof(std::array<float,2>)+1)+corners*(sizeof(uint32_t)+1)+
      size_t(r)*r*sizeof(PaintedMesh::SurfaceMap::Texel)+
      size_t(r/32)*(r/32)*sizeof(PaintedMesh::SurfaceMap::Bounds);
  }
  static size_t surface_working_bytes(size_t vertices,size_t corners,uint32_t r){
    if(!vertices||vertices>2'000'000||!corners||corners>6'000'000||corners%3||r<64||r>2048||(r&(r-1)))
      throw std::length_error("Retained strict surface dimensions exceed their bounded allocation policy");
    // Authored geometry, temporary deindexed raster, complete texel grid and
    // both padding generations coexist while the private constructor runs.
    return sizeof(PaintedMesh::SurfaceMap)+sizeof(PaintedMesh)+
      vertices*(2*sizeof(Vec3)+sizeof(std::array<float,2>)+1)+corners*(sizeof(uint32_t)+1)+
      corners*(2*sizeof(Vec3)+sizeof(std::array<float,2>)+sizeof(uint32_t))+
      size_t(r)*r*(sizeof(PaintedMesh::SurfaceMap::Texel)+2*sizeof(int32_t))+
      size_t(r/32)*(r/32)*sizeof(PaintedMesh::SurfaceMap::Bounds);
  }
  static std::shared_ptr<PaintedMesh> normal(const PaintedMesh &paint,const SourceNormalSampling &proof,
      const PaintSamplingLimits &limits){
    if(!paint.valid_paint_sampling_attachment()||paint.in_stroke()||!proof.before_paint||!proof.metadata_sampling_domain||
       paint.paint_sampling_domain()!=proof.metadata_sampling_domain||paint.normal_projection()!=proof.before_paint->normal_projection()||
       paint.baked()!=proof.before_paint->baked())throw std::invalid_argument("Retained normal transition requires the exact own paint basis and captured metadata");
    proof.validate(true,limits.cancelled);proof.validate_current(paint.mesh(),nullptr,limits.cancelled);
    auto result=paint.snapshot();result->normal_projection_=proof.projection;
    if(result->baked_){auto baked=std::make_shared<BakedTextures>(*result->baked_);
      baked->normal_authority=proof.projection->sampling_domain->detail;
      baked->validate(paint.resolution());result->baked_=std::move(baked);}
    result->composite_cache_=std::make_shared<PaintedMesh::CompositeCache>();result->composite_token_=next_composite_token();
    if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Retained normal adoption cancelled");return result;
  }
  static std::shared_ptr<PaintedMesh> rebind(const PaintedMesh &paint,
      std::shared_ptr<const PaintSamplingAttachment> attachment,const PaintSamplingLimits &limits){
    auto stop=[&]{if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Writable paint geometry rebind cancelled");};stop();
    if(paint.in_stroke()||!attachment||!attachment->surface_||!paint.valid_paint_sampling_attachment())
      throw std::invalid_argument("Writable paint rebind requires finished independently sealed geometry");
    const auto domain=paint.paint_sampling_domain();
    if(!domain||domain!=attachment->domain()||domain.owner_before(attachment->domain())||attachment->domain().owner_before(domain))
      throw std::invalid_argument("Writable paint rebind cannot replace its exact domain owner");
    const auto surface=std::static_pointer_cast<const PaintedMesh::SurfaceMap>(attachment->surface_);
    const auto &a=paint.mesh(),&b=surface->mesh;
    if(paint.resolution()!=surface->resolution||paint.surface_->options.policy!=surface->options.policy||
       !surface->sampling_raster_proven||!attachment->matches(surface->geometry,surface->paint_domain)||
       a.positions.size()!=b.positions.size()||a.normals.size()!=b.normals.size()||a.indices!=b.indices||a.uv!=b.uv||a.uv_pins!=b.uv_pins||a.uv_seams!=b.uv_seams)
      throw std::invalid_argument("Writable paint rebind differs in current layout, resolution or policy");
    domain->validate_current(a,limits);
    for(size_t v=0;v<a.positions.size();++v){if((v&1023)==0)stop();if(a.positions[v].x!=b.positions[v].x||a.positions[v].y!=b.positions[v].y||a.positions[v].z!=b.positions[v].z||a.normals[v].x!=b.normals[v].x||a.normals[v].y!=b.normals[v].y||a.normals[v].z!=b.normals[v].z)
      throw std::invalid_argument("Writable paint rebind differs from exact REST geometry");}
    auto result=paint.snapshot();result->surface_=surface;result->sampling_attachment_=std::move(attachment);stop();return result;
  }
  template<class Geometry> static std::shared_ptr<PaintedMesh> retain(const PaintedMesh &before,Geometry &&authored,
      std::shared_ptr<const PaintSamplingDomain> domain,const PaintSamplingLimits &limits,bool native_grid=false){
    const Mesh&target=[&]()->const Mesh&{if constexpr(std::is_same_v<std::remove_cvref_t<Geometry>,std::shared_ptr<const Mesh>>){if(!authored)throw std::invalid_argument("Writable paint shared geometry is absent");return *authored;}else return authored;}();
    auto stop=[&]{if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Writable paint-grid attachment cancelled");};stop();
    if(before.in_stroke()||!domain||!before.valid_paint_sampling_attachment())
      throw std::invalid_argument("Writable paint-grid attachment requires a finished valid source");
    if(before.source_maps_&&before.source_maps_->uses_uv1())
      throw std::invalid_argument("Retained paint with original UV1 requires its own coordinate-role adapter");
    if(!before.normal_projection_&&((before.source_maps_&&before.source_maps_->normal.image)||
       (!native_grid&&before.baked_&&!before.baked_->maps.normal.rgba.empty())))
      throw std::invalid_argument("Retained paint with raw normal data requires independent prepared-normal authority");
    if(before.revision_==UINT64_MAX)throw std::overflow_error("Paint geometry identity exhausted");
    domain->validate_current(target,limits);
    if(const auto previous=before.paint_sampling_domain()){
      if(domain->basis()!=previous->basis()||domain->basis().owner_before(previous->basis())||previous->basis().owner_before(domain->basis()))
        throw std::invalid_argument("Repeated UV edits must retain their exact original paint basis owner");
    }else{
      const auto &basis=*domain->basis(),&source=before.mesh();
      auto vectors=[](const auto &a,const auto &b){return a.size()==b.size()&&(!a.size()||std::memcmp(a.data(),b.data(),a.size()*sizeof(a[0]))==0);};
      if(!vectors(basis.positions,source.positions)||!vectors(basis.normals,source.normals)||basis.indices!=source.indices||basis.uv!=source.uv||basis.uv_pins!=source.uv_pins||basis.uv_seams!=source.uv_seams)
        throw std::invalid_argument("Writable paint basis is not the exact detached original authoring mesh");
    }
    if(native_grid){
      const auto &original=*before.surface_;
      if(domain->version()!=1||original.diagnostics.policy!=PaintUvPolicy::Strict||original.set_domain||original.source_capture_only)
        throw std::invalid_argument("Native UV retention requires its strict complete writable raster");
      // The domain check certifies REST positions and normals. Verify the
      // old atlas corner order as well, because texel records address faces
      // by ordered ID. Display UV edits and seam splits can then keep the
      // existing immutable sampling, padding, and picking bounds.
      const auto &prior=before.paint_sampling_domain();
      const auto corners=domain->source_corners();
      for(size_t c=0;c<corners.size();++c){
        if((c&1023)==0)stop();
        if(corners[c]!=(prior?prior->source_corners()[c]:uint32_t(c)))
          throw std::invalid_argument("Native UV retention changed the ordered writable atlas corners");
      }
      std::shared_ptr<const Mesh> geometry;
      if constexpr(std::is_same_v<std::remove_cvref_t<Geometry>,std::shared_ptr<const Mesh>>)
        geometry=std::forward<Geometry>(authored);
      else geometry=std::make_shared<const Mesh>(std::forward<Geometry>(authored));
      auto result=before.snapshot();
      result->surface_=std::make_shared<PaintedMesh::SurfaceMap>(original,std::move(geometry),std::move(domain));
      result->seal_paint_sampling_attachment();
      ++result->revision_;
      stop();return result;
    }
    const size_t pixels=size_t(before.resolution())*before.resolution();
    const size_t raster_bytes=target.indices.size()*(2*sizeof(Vec3)+sizeof(std::array<float,2>)+sizeof(uint32_t));
    const size_t grid_bytes=pixels*(sizeof(PaintedMesh::SurfaceMap::Texel)+2*sizeof(int32_t))+
      size_t(before.resolution()/32)*(before.resolution()/32)*sizeof(PaintedMesh::SurfaceMap::Bounds);
    const auto owned=domain->storage_bytes(limits);
    if(owned>limits.max_working_bytes||raster_bytes>limits.max_working_bytes-owned||grid_bytes>limits.max_working_bytes-owned-raster_bytes)
      throw std::length_error("Writable paint domain/raster/grid exceed the combined working budget");
    auto options=before.surface_->options;options.cancelled=limits.cancelled;
    auto surface=std::make_shared<PaintedMesh::SurfaceMap>(std::forward<Geometry>(authored),before.resolution(),options,std::move(domain),limits);
    auto result=before.snapshot();result->surface_=std::move(surface);result->seal_paint_sampling_attachment();
    // Paint values, their one grid, and every image/projection owner are exact.
    // Sampling geometry lives outside TextureMaps, so this stage needs neither
    // a composite rebuild nor a texture upload. A later normal/colour authority
    // change must independently invalidate the relevant interpretation.
    ++result->revision_;stop();return result;
  }
  static std::shared_ptr<PaintedMesh> retain_borrowed(const PaintedMesh &before,const Mesh &authored,
      std::shared_ptr<const PaintSamplingDomain> domain,const PaintSamplingLimits &limits,bool native_grid=false){
    const size_t bytes=authored.positions.size()*(2*sizeof(Vec3)+sizeof(std::array<float,2>))+
        authored.indices.size()*sizeof(uint32_t)+authored.uv_pins.size()+authored.uv_seams.size();
    if(bytes>=limits.max_working_bytes)throw std::length_error("Borrowed writable geometry exhausts working budget before allocation");
    auto budget=limits;budget.max_working_bytes-=bytes;budget.max_basis_bytes=std::min(budget.max_basis_bytes,budget.max_working_bytes);
    return retain(before,authored,std::move(domain),budget,native_grid);
  }
};
const void *detail::retained_scene_paint_raster(const PaintedMesh &paint) noexcept {
  return SourcePaintSamplingAccess::scene_raster(paint);
}
bool detail::native_bake_conversion_supported(const SculptObject &o) noexcept {
  const auto &binding=o.material_bindings;
  if(!o.mesh_active||!o.painted||o.painted->in_stroke()||o.appearance||o.texture_sets||o.polygon||
      !binding||!binding->native_paint_sampling||!binding->native_paint_frames||!binding->normal_frames||
      !binding->normal_frames->pbr||binding->normal_frames->pbr->tangents.empty()||binding->normal_frames->canonical)
    return false;
  const auto bake=o.painted->baked();
  const auto &owner=binding->native_paint_sampling;
  return bake&&bake->normal_interpretation==BakedNormalInterpretation::CanonicalMikk&&
      !bake->maps.normal.rgba.empty()&&!bake->normal_authority&&!bake->normal_field&&
      bake->color_interpretation==BakedColorInterpretation::VertexComponent&&
      owner->valid_owner(owner)&&owner->matches_current(*o.painted)&&
      binding->native_paint_frames->valid_owner(binding->native_paint_frames);
}
detail::NativeBakeConversion detail::convert_native_baked_normals(const SculptObject &object,
    const NormalPreparationOptions &options) {
  return SourcePaintSamplingAccess::convert_native_bake(object,options);
}
size_t paint_sampling_surface_retained_bytes(size_t vertices,size_t corners,uint32_t resolution){
  return SourcePaintSamplingAccess::surface_retained_bytes(vertices,corners,resolution);
}
size_t paint_sampling_surface_working_bytes(size_t vertices,size_t corners,uint32_t resolution){
  return SourcePaintSamplingAccess::surface_working_bytes(vertices,corners,resolution);
}
std::shared_ptr<PaintedMesh> retain_paint_grid(const PaintedMesh &before,Mesh authored,
    std::shared_ptr<const PaintSamplingDomain> domain,const PaintSamplingLimits &limits){
  return SourcePaintSamplingAccess::retain(before,std::move(authored),std::move(domain),limits);
}
std::shared_ptr<PaintedMesh> retain_paint_grid_shared(const PaintedMesh&before,std::shared_ptr<const Mesh>geometry,
    std::shared_ptr<const PaintSamplingDomain>domain,const PaintSamplingLimits&limits){
  return SourcePaintSamplingAccess::retain(before,std::move(geometry),std::move(domain),limits);
}
std::shared_ptr<PaintedMesh> retain_paint_grid_borrowed(const PaintedMesh &before,const Mesh &authored,
    std::shared_ptr<const PaintSamplingDomain> domain,const PaintSamplingLimits &limits){
  return SourcePaintSamplingAccess::retain_borrowed(before,authored,std::move(domain),limits);
}
static void check_native_writable_grid(const PaintedMesh &before,
    const std::shared_ptr<const detail::NativePaintSampling> &owner) {
  if(owner){
    if(!owner->valid_owner(owner)||!owner->matches_current(before))
      throw std::invalid_argument("Native UV retention needs its exact previous writable owner");
  }else if(before.baked()||before.paint_sampling_domain()){
    throw std::invalid_argument("Native UV retention lost its existing writable owner");
  }
}
std::shared_ptr<PaintedMesh> detail::retain_native_writable_grid(const PaintedMesh &before,const Mesh &authored,
    std::shared_ptr<const PaintSamplingDomain> domain,const std::shared_ptr<const NativePaintSampling> &owner,
    const PaintSamplingLimits &limits){
  check_native_writable_grid(before,owner);
  return SourcePaintSamplingAccess::retain_borrowed(before,authored,std::move(domain),limits,true);
}
std::shared_ptr<PaintedMesh> detail::retain_native_writable_grid(const PaintedMesh &before,Mesh &&authored,
    std::shared_ptr<const PaintSamplingDomain> domain,const std::shared_ptr<const NativePaintSampling> &owner,
    const PaintSamplingLimits &limits){
  check_native_writable_grid(before,owner);
  return SourcePaintSamplingAccess::retain(before,std::move(authored),std::move(domain),limits,true);
}
std::shared_ptr<PaintedMesh> adopt_retained_paint_normal(const PaintedMesh &paint,const SourceNormalSampling &proof,
    const PaintSamplingLimits &limits){return SourcePaintSamplingAccess::normal(paint,proof,limits);}
std::shared_ptr<PaintedMesh> rebind_paint_sampling_geometry(const PaintedMesh &paint,
    std::shared_ptr<const PaintSamplingAttachment> attachment,const PaintSamplingLimits &limits){
  return SourcePaintSamplingAccess::rebind(paint,std::move(attachment),limits);
}
struct SourceKeyPaintAccess {
  // A declared key-domain alias names a complete SurfaceMap, including Source4
  // ownership. Authenticate that independent proof before restoring the alias;
  // ordinary equal geometry never grants permission to merge source histories.
  static bool source_alias(const PaintedMesh &paint, const PaintedMesh &anchor,
                           const std::function<bool()> &cancelled) {
    const auto a = paint.surface_->original_sampling, b = anchor.surface_->original_sampling;
    auto stop = [&] {
      if (cancelled && cancelled())
        throw std::runtime_error("Key source-sampling alias replay cancelled");
    };
    auto verify = [&](const PaintedMesh &current, const auto &proof) {
      if (!proof)
        return;
      if (!current.valid_source_sampling_attachment())
        throw std::invalid_argument("Key source-sampling alias lost its exact current attachment");
      SourcePaintAccess::pristine(*proof->before);
      SourcePaintOptions options;
      options.cancelled = cancelled;
      options.max_working_bytes = SourcePaintOptions::maximum_working_bytes;
      SourcePaintAccess::budget(*proof->before, current.mesh(), options, proof->authored_uv1);
      SourcePaintAccess::authored(*proof->before, proof->authored_uv1, options);
      if (proof->authored_uv1 && SourcePaintAccess::maps(*proof->before).uv1)
        throw std::invalid_argument("Key source-sampling alias has redundant authored UV1");
      if (SourcePaintAccess::working_bytes(current) > SourcePaintOptions::maximum_working_bytes)
        throw std::length_error("Key source-sampling alias exceeds original working quota");
      auto expected = detail::transport_source_sampling(
          proof->before->mesh(), SourcePaintAccess::maps(*proof->before), current.mesh(), proof->vertices,
          SourcePaintAccess::limits(options, proof->vertices.size(), proof->authored_uv1 ? proof->authored_uv1->size() : 0));
      if (!SourcePaintAccess::same(*expected, *current.source_maps_))
        throw std::invalid_argument(
            "Key source-sampling alias differs from complete original replay");
      const auto &old = SourcePaintAccess::maps(*proof->before), &now = *current.source_maps_;
      const std::array<const PaintSourceTexture *, 5> original{
          &old.color, &old.metal_rough, &old.emission, &old.normal, &old.occlusion},
          transported{&now.color, &now.metal_rough, &now.emission, &now.normal, &now.occlusion};
      std::shared_ptr<const std::vector<std::array<float, 2>>> uv0;
      for (size_t slot = 0; slot < original.size(); ++slot)
        if (original[slot]->image && original[slot]->texcoord == 0) {
          const auto carrier = transported[slot]->texcoord;
          if (carrier == 1)
            uv0 = now.uv1;
          else {
            const auto found = now.extra_uv_sets.find(uint32_t(carrier));
            if (found == now.extra_uv_sets.end())
              throw std::invalid_argument(
                  "Key source-sampling alias lost actual logical UV0 carrier");
            uv0 = found->second;
          }
          break;
        }
      if (!proof->original_uv0 || proof->original_uv0->size() != proof->vertices.size())
        throw std::invalid_argument("Key source-sampling alias lost complete logical UV0");
      for (size_t v = 0; v < proof->vertices.size(); ++v) {
        if ((v & 1023) == 0)
          stop();
        const auto value = uv0 ? uv0->at(v) : proof->before->mesh().uv.at(proof->vertices[v]);
        if (std::memcmp(&value, &proof->original_uv0->at(v), sizeof(value)))
          throw std::invalid_argument("Key source-sampling alias changed original logical UV0");
      }
      const auto &before_uv1 = proof->authored_uv1 ? proof->authored_uv1 : old.uv1;
      if (bool(proof->original_uv1) != bool(before_uv1) ||
          (before_uv1 && proof->original_uv1->size() != proof->vertices.size()))
        throw std::invalid_argument("Key source-sampling alias changed actual logical UV1 ownership");
      if (old.uv1 && (!now.uv1 || proof->original_uv1 != now.uv1 ||
                     proof->original_uv1.owner_before(now.uv1) || now.uv1.owner_before(proof->original_uv1)))
        throw std::invalid_argument("Key source-sampling alias changed actual PBR UV1 owner");
      if (before_uv1) for (size_t v = 0; v < proof->vertices.size(); ++v) {
        if ((v & 1023) == 0) stop();
        const auto value = before_uv1->at(proof->vertices[v]);
        if (std::memcmp(&value, &proof->original_uv1->at(v), sizeof(value)))
          throw std::invalid_argument("Key source-sampling alias changed authored UV1 correspondence");
      }
      stop();
    };
    verify(paint, a);
    verify(anchor, b);
    if (bool(a) != bool(b))
      return false;
    if (!a)
      return true;
    auto same = [](const auto &x, const auto &y) {
      return x.size() == y.size() &&
             (x.empty() || !std::memcmp(x.data(), y.data(), x.size() * sizeof(x[0])));
    };
    auto coordinates = [&](const auto &x, const auto &y) {
      return bool(x) == bool(y) && (!x || same(*x, *y));
    };
    const auto &x = *a->before, &y = *b->before;
    const auto &xm = x.mesh(), &ym = y.mesh();
    if (a->vertices != b->vertices || !coordinates(a->original_uv0, b->original_uv0) ||
        !coordinates(a->original_uv1, b->original_uv1) ||
        !coordinates(a->authored_uv1, b->authored_uv1) || x.resolution() != y.resolution() ||
        x.surface_diagnostics().policy != y.surface_diagnostics().policy ||
        !same(xm.positions, ym.positions) || !same(xm.normals, ym.normals) || xm.uv != ym.uv ||
        xm.indices != ym.indices || xm.uv_pins != ym.uv_pins || xm.uv_seams != ym.uv_seams ||
        !SourcePaintAccess::same(SourcePaintAccess::maps(x), SourcePaintAccess::maps(y)) ||
        !SourcePaintAccess::same(*paint.source_maps_, *anchor.source_maps_) ||
        x.active_ != y.active_ || x.next_ != y.next_ || x.layers_.size() != y.layers_.size())
      return false;
    auto tiles = [&](const auto &l, const auto &r) {
      for (size_t i = 0; i < std::max(l.size(), r.size()); ++i) {
        if ((i & 31) == 0)
          stop();
        const auto *a = i < l.size() ? l[i].get() : nullptr,
                   *b = i < r.size() ? r[i].get() : nullptr;
        if (bool(a) != bool(b))
          return false;
        if (a && a != b && std::memcmp(a, b, sizeof(*a)))
          return false;
      }
      return true;
    };
    for (size_t i = 0; i < x.layers_.size(); ++i) {
      const auto &l = x.layers_[i], &r = y.layers_[i];
      if (l.id != r.id || l.name != r.name || l.visible != r.visible ||
          std::bit_cast<uint32_t>(l.opacity) != std::bit_cast<uint32_t>(r.opacity) ||
          !tiles(l.tiles, r.tiles) || !tiles(l.pbr_tiles, r.pbr_tiles) ||
          !tiles(l.emission_tiles, r.emission_tiles))
        return false;
      stop();
    }
    return true;
  }
  static std::shared_ptr<PaintedMesh> source(const PaintedMesh &paint,
                                             const SourceMaterialBindings &binding,
                                             const CreatorAppearance *appearance,
                                             const DeformationState *deformation,
                                             const std::function<bool()> &cancelled) {
    auto stop = [&] {
      if (cancelled && cancelled())
        throw std::runtime_error("Native key source-alias adoption cancelled");
    };
    stop();
    if (!binding.uv_origin || !binding.uv_origin->paint || paint.in_stroke() ||
        !paint.valid_paint_sampling_attachment())
      throw std::invalid_argument(
          "Native key source alias requires an actual finished source raster");
    binding.validate(true);
    detail::source_paint_pairing(&paint, &binding, appearance, true);
    detail::normal_frame_current_authority(binding, paint.mesh(), deformation, cancelled);
    const auto &anchor = *binding.uv_origin->paint;
    const auto &a = paint.mesh(), &b = anchor.mesh();
    auto same = [](const auto &a, const auto &b) {
      return a.size() == b.size() &&
             (a.empty() || std::memcmp(a.data(), b.data(), a.size() * sizeof(a[0])) == 0);
    };
    const auto domain = paint.paint_sampling_domain(), prior = anchor.paint_sampling_domain();
    const bool same_domain =
        domain == prior && !domain.owner_before(prior) && !prior.owner_before(domain);
    if (anchor.in_stroke() || !anchor.valid_paint_sampling_attachment() || !same_domain ||
        paint.resolution() != anchor.resolution() ||
        paint.surface_diagnostics().policy != anchor.surface_diagnostics().policy ||
        !same(a.positions, b.positions) || !same(a.normals, b.normals) || !same(a.uv, b.uv) ||
        a.indices != b.indices || a.uv_pins != b.uv_pins || a.uv_seams != b.uv_seams)
      throw std::invalid_argument(
          "Native key source alias differs from its exact declared current raster");
    // The explicit domain2 witness names this source SurfaceMap. Full current
    // material/field replay above independently grants its own authority; all
    // current layers/maps/projections/baked event and cache remain current.
    if (!source_alias(paint, anchor, cancelled))
      throw std::invalid_argument(
          "Native key declared source alias differs from complete original sampling ownership");
    auto result = paint.snapshot();
    result->surface_ = anchor.surface_;
    result->sampling_attachment_ = anchor.sampling_attachment_;
    if (anchor.source_sampling_prepared()) {
      const bool same_maps = result->source_maps_ == anchor.source_maps_ &&
                             !result->source_maps_.owner_before(anchor.source_maps_) &&
                             !anchor.source_maps_.owner_before(result->source_maps_);
      result->source_maps_ = anchor.source_maps_;
      if (!same_maps) {
        result->composite_cache_ = std::make_shared<PaintedMesh::CompositeCache>();
        result->composite_token_ = next_composite_token();
      }
    }
    detail::source_paint_pairing(result.get(), &binding, appearance, true);
    detail::normal_frame_current_authority(binding, result->mesh(), deformation, cancelled);
    stop();
    return result;
  }
  static std::shared_ptr<PaintedMesh> adopt(const PaintedMesh &paint,
                                            const SourceMaterialBindings &binding,
                                            const CreatorAppearance *appearance,
                                            const DeformationState *deformation,
                                            const std::function<bool()> &cancelled) {
    auto stop = [&] {
      if (cancelled && cancelled())
        throw std::runtime_error("Native key geometry adoption cancelled");
    };
    stop();
    const auto node = binding.key_derivation;
    if (!node || !node->valid_owner(node) || node->frames() != binding.normal_frames)
      throw std::invalid_argument(
          "Native key geometry adoption requires its actual owning material node");
    node->validate(true, cancelled);
    detail::source_paint_pairing(&paint, &binding, appearance, true);
    detail::normal_frame_current_authority(binding, paint.mesh(), deformation, cancelled);
    const auto &anchor = *node->before_paint();
    auto same = [](const auto &a, const auto &b) {
      return a.size() == b.size() &&
             (a.empty() || std::memcmp(a.data(), b.data(), a.size() * sizeof(a[0])) == 0);
    };
    const auto &a = paint.mesh(), &b = anchor.mesh();
    const auto domain = paint.paint_sampling_domain(), prior = anchor.paint_sampling_domain();
    const bool same_domain =
        domain == prior && !domain.owner_before(prior) && !prior.owner_before(domain);
    auto result = paint.snapshot();
    if (!same_domain || paint.resolution() != anchor.resolution() ||
        paint.surface_diagnostics().policy != anchor.surface_diagnostics().policy ||
        !same(a.positions, b.positions) || !same(a.normals, b.normals) || !same(a.uv, b.uv) ||
        a.indices != b.indices || a.uv_pins != b.uv_pins || a.uv_seams != b.uv_seams)
      return result;
    // Full source/field replay above proves the owning model. Exact ordered
    // geometry + actual independent grid-domain identity proves this raster
    // is unchanged; geometry equality alone never grants field authority.
    if (!source_alias(paint, anchor, cancelled))
      return result;
    result->surface_ = anchor.surface_;
    result->sampling_attachment_ = anchor.sampling_attachment_;
    if (anchor.source_sampling_prepared()) {
      const bool same_maps = result->source_maps_ == anchor.source_maps_ &&
                             !result->source_maps_.owner_before(anchor.source_maps_) &&
                             !anchor.source_maps_.owner_before(result->source_maps_);
      result->source_maps_ = anchor.source_maps_;
      if (!same_maps) {
        result->composite_cache_ = std::make_shared<PaintedMesh::CompositeCache>();
        result->composite_token_ = next_composite_token();
      }
    }
    stop();
    return result;
  }
};

namespace detail {
std::shared_ptr<PaintedMesh> adopt_key_paint_geometry(const PaintedMesh &paint,const SourceMaterialBindings &binding,const CreatorAppearance *appearance,const DeformationState *deformation,const std::function<bool()> &cancelled){return SourceKeyPaintAccess::adopt(paint,binding,appearance,deformation,cancelled);}
std::shared_ptr<PaintedMesh> adopt_key_source_geometry(const PaintedMesh &paint,const SourceMaterialBindings &binding,const CreatorAppearance *appearance,const DeformationState *deformation,const std::function<bool()> &cancelled){return SourceKeyPaintAccess::source(paint,binding,appearance,deformation,cancelled);}
}
struct PolygonPaintAccess {
  static std::shared_ptr<PaintedMesh> rebuild(const PaintedMesh &source, Mesh target,
                                              const PaintSurfaceOptions &options) {
    if(source.editing_)throw std::logic_error("Finish the paint stroke before rebuilding polygon geometry");
    if(source.paint_sampling_domain())throw std::logic_error("Polygon topology changes require an explicit retained paint-domain adapter");
    if(source.color_projection_)throw std::logic_error("Polygon topology changes cannot yet preserve this mesh's prepared source colors");
    if(source.normal_projection_)throw std::logic_error("Polygon topology changes require explicit prepared-normal frame transport");
    if(source.revision_==UINT64_MAX)throw std::overflow_error("Paint geometry identity exhausted");
    if(options.cancelled&&options.cancelled())throw std::runtime_error("Polygon paint preparation cancelled");
    auto surface=std::make_shared<PaintedMesh::SurfaceMap>(std::move(target),source.resolution(),options);
    auto result=source.snapshot();
    result->surface_=std::move(surface);
    result->composite_cache_=std::make_shared<PaintedMesh::CompositeCache>();
    result->composite_token_=next_composite_token();
    ++result->revision_;
    if(options.cancelled&&options.cancelled())throw std::runtime_error("Polygon paint preparation cancelled");
    return result;
  }
};
std::shared_ptr<PaintedMesh> polygon_paint_geometry(const PaintedMesh &source,Mesh target,const PaintSurfaceOptions &options) {
  return PolygonPaintAccess::rebuild(source,std::move(target),options);
}
void PaintNormalProjection::validate(uint32_t resolution) const {
  validate(resolution,true);
}
void PaintNormalProjection::validate(uint32_t resolution,bool geometry) const {
  if((version!=1&&version!=2)||!pbr||!mtoon||bool(sampling_domain)!=(version==2))throw std::invalid_argument("Prepared normal projection is incomplete");
  for(const auto &image:{pbr,mtoon}){image->validate();if(image->width!=resolution||image->height!=resolution)throw std::invalid_argument("Prepared normal projection grid differs from paint");}
  if(sampling_domain){sampling_domain->validate(geometry);if(sampling_domain->resolution!=resolution)throw std::invalid_argument("Retained normal domain resolution differs from paint");}
}
std::shared_ptr<const PaintNormalProjection> SourceNormalPreparationAccess::resolved(
    const PaintedMesh &source,const std::function<bool()> &cancelled) {
  auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Normal field resolution cancelled");};stop();
  if(source.editing_||!source.normal_projection_)
    throw std::invalid_argument("Normal field resolution requires finished intrinsic normal planes");
  const auto r=source.resolution();source.normal_projection_->validate(r);
  const auto baked=source.baked_;
  if(source.normal_projection_->version==2){
    const auto &domain=*source.normal_projection_->sampling_domain;
    if(!baked||baked->maps.normal.rgba.empty())return source.normal_projection_;
    const auto detail=domain.detail;
    auto same=[](const Image &a,const Image &b){return a.width==b.width&&a.height==b.height&&a.rgba==b.rgba;};
    if(!detail||baked->normal_authority!=detail||
       baked->normal_interpretation!=BakedNormalInterpretation::PreparedAttributes||
       !same(baked->maps.normal,*detail->pbr)||
       !same(baked->maps.mtoon_normal,*detail->mtoon))
      throw std::invalid_argument("Changed normal detail requires an explicit retained-field bake transition");
    auto result=std::make_shared<PaintNormalProjection>();
    result->pbr=domain.resolved_pbr;result->mtoon=domain.resolved_mtoon;
    result->validate(r);stop();return result;
  }
  if(!baked||baked->maps.normal.rgba.empty())return source.normal_projection_;
  baked->validate(r);
  if(baked->normal_interpretation!=BakedNormalInterpretation::PreparedAttributes)
    throw std::invalid_argument("Retained normal sampling cannot reinterpret a historical canonical bake");
  auto pbr=std::make_shared<Image>(*source.normal_projection_->pbr);
  auto toon=std::make_shared<Image>(*source.normal_projection_->mtoon);
  // Keep this version1 evaluation byte-for-byte equivalent to the historical
  // prepared branch of composite(). It deliberately runs before filtering.
  for(size_t at=0;at<size_t(r)*r;++at){
    if((at&1023)==0)stop();
    const auto owner=source.surface_->padding[at];if(owner<0)continue;
    const float coverage=unit(baked->coverage[size_t(owner)]);
    for(auto pair:{std::pair{pbr.get(),&baked->maps.normal},std::pair{toon.get(),&baked->maps.mtoon_normal}}){
      const size_t a=at*4,b=size_t(owner)*4;
      Vec3 original{unit(pair.first->rgba[a])*2-1,unit(pair.first->rgba[a+1])*2-1,unit(pair.first->rgba[a+2])*2-1};
      Vec3 detail{unit(pair.second->rgba[b])*2-1,unit(pair.second->rgba[b+1])*2-1,unit(pair.second->rgba[b+2])*2-1};
      auto value=original*(1-coverage)+detail*coverage;
      value=length(value)>1e-8f?normalized(value):Vec3{0,0,1};
      pair.first->rgba[a]=byte(value.x*.5f+.5f);pair.first->rgba[a+1]=byte(value.y*.5f+.5f);
      pair.first->rgba[a+2]=byte(value.z*.5f+.5f);pair.first->rgba[a+3]=255;
    }
  }
  auto result=std::make_shared<PaintNormalProjection>();result->pbr=std::move(pbr);result->mtoon=std::move(toon);
  result->validate(r);stop();return result;
}
std::shared_ptr<PaintedMesh> SourceNormalPreparationAccess::adopt_canonical(const PaintedMesh &source,
    std::shared_ptr<const PaintNormalProjection> projection,const std::function<bool()> &cancelled){
  auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Canonical normal adoption cancelled");};stop();
  if(source.in_stroke()||source.normal_projection_||!source.baked_||
      source.baked_->normal_interpretation!=BakedNormalInterpretation::CanonicalMikk||source.baked_->maps.normal.rgba.empty()||!projection)
    throw std::invalid_argument("Canonical normal adoption requires its genuine completed historical input");
  projection->validate(source.resolution());
  source.baked_->validate(source.resolution());
  auto result=source.snapshot();auto baked=std::make_shared<BakedTextures>();
  baked->maps.color=source.baked_->maps.color;baked->maps.metal_rough=source.baked_->maps.metal_rough;
  baked->maps.emission=source.baked_->maps.emission;baked->maps.pbr_color=source.baked_->maps.pbr_color;
  baked->maps.transparent=source.baked_->maps.transparent;baked->coverage=source.baked_->coverage;
  baked->emission_field=source.baked_->emission_field;
  baked->color_interpretation=source.baked_->color_interpretation;
  // Only the normal role moves into the explicitly converted intrinsic plane.
  // Material channels, coverage, editable sparse layers and source maps remain.
  baked->normal_interpretation=BakedNormalInterpretation::PreparedAttributes;baked->validate(source.resolution());
  result->baked_=std::move(baked);result->normal_projection_=std::move(projection);
  if(result->storage_bytes()>128*1024*1024)throw std::length_error("Converted normal planes exceed paint storage budget");
  result->composite_token_=next_composite_token();++result->revision_;stop();return result;
}
std::shared_ptr<PaintedMesh> SourceNormalPreparationAccess::adopt_baked_field(const PaintedMesh &source,
    std::shared_ptr<const detail::BakedNormalField> field,const std::function<bool()> &cancelled){
  if(cancelled&&cancelled())throw std::runtime_error("Baked normal adoption cancelled");
  if(!field||source.in_stroke()||source.normal_projection_||!source.baked_||
      source.baked_->normal_interpretation!=BakedNormalInterpretation::CanonicalMikk||source.baked_->maps.normal.rgba.empty())
    throw std::invalid_argument("Baked field adoption requires its actual completed canonical before-state");
  field->validate();
  if(field->origin()->before_paint()->mesh_owner()!=source.mesh_owner()||field->resolution()!=source.resolution())
    throw std::invalid_argument("Baked field adoption borrowed another geometry/grid owner");
  auto result=source.snapshot();auto baked=std::make_shared<BakedTextures>(*source.baked_);
  baked->maps.normal={};baked->maps.mtoon_normal={};baked->normal_interpretation=BakedNormalInterpretation::PreparedAttributes;
  baked->normal_field=std::move(field);baked->validate(source.resolution());result->baked_=std::move(baked);
  if(result->storage_bytes()>128*1024*1024)throw std::length_error("Independent baked normals exceed paint storage budget");
  result->composite_token_=next_composite_token();++result->revision_;
  if(cancelled&&cancelled())throw std::runtime_error("Baked normal adoption cancelled");return result;
}
std::shared_ptr<const Image> SourceNormalPreparationAccess::canonical_intrinsic(
    const PaintedMesh &source,const std::function<bool()> &cancelled){
  if(source.in_stroke()||source.normal_projection_)throw std::invalid_argument("Intrinsic canonical baseline needs finished original paint");
  auto image=std::make_shared<Image>(blank_image(source.resolution(),true));
  const PaintSourceMaps empty;const auto &maps=source.source_maps_?*source.source_maps_:empty;
  std::vector<Tangent> corners;
  if(maps.normal.image&&maps.normal_scale>0)corners=tangent_corners(source.mesh(),cancelled);
  for(size_t at=0;at<size_t(source.resolution())*source.resolution();++at){
    if((at&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Intrinsic canonical baseline cancelled");
    const auto owner=source.surface_->padding[at];if(owner<0)continue;Vec3 n{0,0,1};
    if(!corners.empty()){
      const auto &texel=source.surface_->texels[size_t(owner)];
      const std::array<float,3> bary{texel.bary[0],texel.bary[1],1-texel.bary[0]-texel.bary[1]};
      n=tangent_frame(source.mesh(),corners,texel.face,bary).to_tangent(source_texture_normal(source.mesh(),maps,texel.face,bary));
    }
    n=length(n)>1e-8f?normalized(n):Vec3{0,0,1};
    image->rgba[at*4]=byte(n.x*.5f+.5f);image->rgba[at*4+1]=byte(n.y*.5f+.5f);image->rgba[at*4+2]=byte(n.z*.5f+.5f);
  }
  return image;
}
std::shared_ptr<PaintedMesh> SourceNormalPreparationAccess::upgrade_canonical(const PaintedMesh &source,
    std::shared_ptr<const detail::BakedNormalField> field,const std::function<bool()> &cancelled){
  if(cancelled&&cancelled())throw std::runtime_error("Canonical layer upgrade cancelled");
  if(source.in_stroke()||!field||!source.normal_projection_||source.normal_projection_->version!=1||
      source.normal_projection_->pbr!=field->origin()->projection()->pbr||
      source.normal_projection_->mtoon!=field->origin()->projection()->mtoon)
    throw std::invalid_argument("Canonical layer upgrade needs its actual unchanged owning legacy projection");
  auto result=source.snapshot();result->normal_projection_.reset();
  if(source.baked_){
    if(source.baked_->normal_field||source.baked_->normal_authority||!source.baked_->maps.normal.rgba.empty())
      throw std::invalid_argument("Legacy canonical layer upgrade needs its explicit later-bake/UV adapter");
    auto baked=std::make_shared<BakedTextures>(*source.baked_);baked->normal_field=std::move(field);
    baked->validate(source.resolution());result->baked_=std::move(baked);
  }
  result->composite_token_=next_composite_token();++result->revision_;
  if(cancelled&&cancelled())throw std::runtime_error("Canonical layer upgrade cancelled");return result;
}
PaintNormalPreparationResult SourceNormalPreparationAccess::prepare(const PaintedMesh &source,
    const SourceMaterialBindings *binding,const NormalPreparationOptions &options) {
  auto check=[&]{if(options.cancelled&&options.cancelled())throw std::runtime_error("Normal preparation cancelled");};check();
  if(source.editing_||source.normal_projection_)throw std::invalid_argument("Normal preparation requires a finished unprepared paint surface");
  if(source.surface_->diagnostics.policy==PaintUvPolicy::PolygonAuthored||source.surface_->diagnostics.additional_candidates||!source.surface_->diagnostics.unpaintable_uv_faces.empty())
    throw std::invalid_argument("Prepare a unique strict writable UV atlas before preparing normal attributes");
  if(source.baked_&&!source.baked_->maps.normal.rgba.empty())throw std::invalid_argument("Historical canonical normal pixels require an explicit source-retaining clear/rebake before normal preparation");
  const auto r=source.resolution();const size_t pixels=size_t(r)*r;
  const size_t bytes_per_texel=8+(source.surface_->diagnostics.policy==PaintUvPolicy::Strict?0:sizeof(PaintedMesh::SurfaceMap::Texel)+2*sizeof(int32_t)+2);
  if(pixels>options.max_samples||pixels>options.max_working_bytes/bytes_per_texel)
    throw std::length_error("Intrinsic normal projection exceeds its worker budget");
  auto atlas=source.surface_;
  if(atlas->diagnostics.policy!=PaintUvPolicy::Strict){PaintSurfaceOptions strict;strict.cancelled=options.cancelled;atlas=std::make_shared<PaintedMesh::SurfaceMap>(source.mesh(),r,strict);}
  auto projection=std::make_shared<PaintNormalProjection>();
  auto pbr=std::make_shared<Image>(blank_image(r,true)),toon=std::make_shared<Image>(blank_image(r,true));
  const PaintSourceMaps empty;const auto &maps=source.source_maps_?*source.source_maps_:empty;
  if(maps.normal.image&&(maps.normal.texcoord!=0||maps.normal.uv_transform!=std::array<float,6>{1,0,0,1,0,0}))
    throw std::invalid_argument("Prepare original normal sampling UVs before authoring the explicit normal model");
  if(binding&&binding->has_mtoon){const auto &material=binding->material;
    if(material.parameters.scroll!=std::array<float,2>{}||material.parameters.rotation_speed!=0)
      throw std::invalid_argument("Freeze animated normal sampling with explicit source UV preparation first");
    if(material.version()==MToonVersion::V0){const auto &main=material.textures[0];
      if(main.offset!=std::array<float,2>{}||main.scale!=std::array<float,2>{1,1})
        throw std::invalid_argument("Prepare shared legacy MToon sampling ST before normal preparation");
    }
    const auto &normal=material.textures[size_t(MToonSlot::Normal)];
    if(normal.texcoord!=0||(normal.transform_texcoord>0)||normal.offset!=std::array<float,2>{}||normal.scale!=std::array<float,2>{1,1}||normal.rotation!=0)
      throw std::invalid_argument("Prepare MToon normal sampling UVs before the explicit normal model");
  }
  NormalPreparationReceipt receipt;receipt.resolution=r;receipt.accepted_angular_error=options.max_angular_error;
  if(binding&&binding->has_mtoon)receipt.mtoon=binding->material.version()==MToonVersion::V0?PreparedMToonNormalEvaluation::LegacyRawInterpolated:PreparedMToonNormalEvaluation::MToon1;
  auto encode=[&](Image &image,size_t at,const PaintSourceTexture &view,float scale,std::array<float,2> uv){
    Vec3 desired{0,0,1};
    if(view.image){const auto value=sample_source_texture(view,uv[0],uv[1],false);desired={(value[0]*2-1)*scale,(value[1]*2-1)*scale,value[2]*2-1};
      if(!finite(desired)||length(desired)<1e-8f)throw std::invalid_argument("Intrinsic source normal has a singular decoder direction");
      desired=normalized(desired);
    }
    image.rgba[at*4]=byte(desired.x*.5f+.5f);image.rgba[at*4+1]=byte(desired.y*.5f+.5f);image.rgba[at*4+2]=byte(desired.z*.5f+.5f);image.rgba[at*4+3]=255;
    Vec3 decoded{unit(image.rgba[at*4])*2-1,unit(image.rgba[at*4+1])*2-1,unit(image.rgba[at*4+2])*2-1};decoded=normalized(decoded);
    const double error=std::atan2(length(cross(desired,decoded)),std::clamp(double(dot(desired,decoded)),-1.,1.));
    receipt.observed_initial_error=std::max(receipt.observed_initial_error,error);
  };
  for(size_t at=0;at<pixels;++at){if((at&1023)==0)check();const auto owner=atlas->padding[at];if(owner<0)continue;
    const auto &texel=atlas->texels[size_t(owner)];if(!texel.covered)throw std::logic_error("Prepared normal padding lost its owning surface texel");
    const float bary[3]={texel.bary[0],texel.bary[1],1-texel.bary[0]-texel.bary[1]};std::array<float,2> uv{};
    for(size_t k=0;k<3;++k){const auto q=source.mesh().uv[source.mesh().indices[texel.face*3+k]];uv[0]+=q[0]*bary[k];uv[1]+=q[1]*bary[k];}
    encode(*pbr,at,maps.normal,maps.normal_scale,uv);
    PaintSourceTexture normal;float scale=1;
    if(binding&&binding->has_mtoon&&binding->material.parameters.normal_enabled){normal=binding->roles[size_t(MToonSlot::Normal)];scale=binding->material.parameters.normal_scale;}
    encode(*toon,at,normal,scale,uv);++receipt.sampled_texels;
  }
  receipt.validate();projection->pbr=std::move(pbr);projection->mtoon=std::move(toon);projection->validate(r);
  auto output=source.snapshot();output->surface_=std::move(atlas);output->normal_projection_=std::move(projection);
  if(output->baked_){auto baked=std::make_shared<BakedTextures>(*output->baked_);baked->normal_interpretation=BakedNormalInterpretation::PreparedAttributes;output->baked_=std::move(baked);}
  if(output->storage_bytes()>128*1024*1024)throw std::length_error("Prepared normal planes exceed paint storage budget");
  output->composite_token_=next_composite_token();++output->revision_;check();return {std::move(output),receipt};
}
PaintNormalUvProjectionResult SourceNormalPreparationAccess::reproject(const PaintedMesh &source,
    const Mesh &target,std::span<const uint32_t> source_vertices,const NormalPreparationOptions &options) {
  auto stop=[&]{if(options.cancelled&&options.cancelled())throw std::runtime_error("Prepared normal UV reprojection cancelled");};stop();
  if(source.editing_||!source.normal_projection_)throw std::invalid_argument("Normal UV reprojection requires finished intrinsic normal planes");
  if(source.normal_projection_->version!=1)throw std::invalid_argument("Retained normal fields require their source-owning correspondence adapter, not raster reinterpretation");
  if(!std::isfinite(options.max_angular_error)||options.max_angular_error<0||options.max_angular_error>.25||!options.max_samples||options.max_samples>64'000'000||!options.max_working_bytes||options.max_working_bytes>512*1024*1024)
    throw std::invalid_argument("Invalid prepared normal UV reprojection bounds");
  const auto &before=source.mesh();target.validate();
  if(target.positions.size()>2'000'000||target.indices.size()>6'000'000||target.indices.size()!=before.indices.size()||source_vertices.size()!=target.positions.size()||target.uv.size()!=target.positions.size())
    throw std::invalid_argument("Normal UV reprojection requires exact bounded ordered-corner geometry");
  for(size_t v=0;v<source_vertices.size();++v){if((v&1023)==0)stop();auto old=source_vertices[v];
    if(old>=before.positions.size()||target.positions[v].x!=before.positions[old].x||target.positions[v].y!=before.positions[old].y||target.positions[v].z!=before.positions[old].z||target.normals[v].x!=before.normals[old].x||target.normals[v].y!=before.normals[old].y||target.normals[v].z!=before.normals[old].z)
      throw std::invalid_argument("Normal UV reprojection cannot infer changed REST geometry/directions");
  }
  for(size_t c=0;c<target.indices.size();++c){if((c&1023)==0)stop();if(source_vertices[target.indices[c]]!=before.indices[c])throw std::invalid_argument("Normal UV reprojection lost exact source corner identity");}
  const auto r=source.resolution();const size_t pixels=size_t(r)*r;
  const size_t geometry_bytes=target.positions.size()*sizeof(Vec3)+target.normals.size()*sizeof(Vec3)+target.uv.size()*sizeof(std::array<float,2>)+target.indices.size()*sizeof(uint32_t)+target.uv_pins.size()+target.uv_seams.size()+target.indices.size()/3;
  const size_t tile_bytes=size_t(r/32)*(r/32)*sizeof(PaintedMesh::SurfaceMap::Bounds);
  if(geometry_bytes>options.max_working_bytes||tile_bytes>options.max_working_bytes-geometry_bytes||pixels>options.max_samples||pixels>(options.max_working_bytes-geometry_bytes-tile_bytes)/(8+sizeof(PaintedMesh::SurfaceMap::Texel)+2*sizeof(int32_t)+2))
    throw std::length_error("Prepared normal UV reprojection exceeds worker storage/sampling budget");
  PaintSurfaceOptions strict;strict.cancelled=options.cancelled;strict.max_raster_samples=options.max_samples;auto atlas=std::make_shared<PaintedMesh::SurfaceMap>(target,r,strict);
  auto pbr=std::make_shared<Image>(blank_image(r,true)),toon=std::make_shared<Image>(blank_image(r,true));PaintNormalUvProjectionResult result;
  source.normal_projection_->validate(r);
  auto project=[&](Image &out,size_t at,const std::shared_ptr<const Image> &image,std::array<float,2> uv){
    PaintSourceTexture view;view.image=image;view.wrap_s=view.wrap_t=33071;view.min_filter=view.mag_filter=9729;
    const auto value=sample_source_texture(view,uv[0],uv[1],false);Vec3 original{value[0]*2-1,value[1]*2-1,value[2]*2-1};
    if(!finite(original)||length(original)<1e-8f)throw std::invalid_argument("Prepared normal UV sampling has a singular decoder direction");
    // Preserve sampled vector magnitude: normalizing here would change later
    // interpolation and would silently author a different normal-field model.
    for(size_t c=0;c<4;++c)out.rgba[at*4+c]=byte(value[c]);
    const auto wanted=normalized(original);const auto decoded=normalized(Vec3{unit(out.rgba[at*4])*2-1,unit(out.rgba[at*4+1])*2-1,unit(out.rgba[at*4+2])*2-1});
    result.observed_sample_error=std::max(result.observed_sample_error,std::atan2(double(length(cross(wanted,decoded))),std::clamp(double(dot(wanted,decoded)),-1.,1.)));
  };
  for(size_t at=0;at<pixels;++at){if((at&1023)==0)stop();auto owner=atlas->padding[at];if(owner<0)continue;const auto &texel=atlas->texels[size_t(owner)];
    const float bary[]={texel.bary[0],texel.bary[1],1-texel.bary[0]-texel.bary[1]};std::array<float,2> uv{};
    for(size_t k=0;k<3;++k){const auto q=before.uv[before.indices[size_t(texel.face)*3+k]];uv[0]+=q[0]*bary[k];uv[1]+=q[1]*bary[k];}
    project(*pbr,at,source.normal_projection_->pbr,uv);project(*toon,at,source.normal_projection_->mtoon,uv);++result.sampled_texels;
  }
  // Audit every original face constraint, including coincident geometry that
  // the ordinary paint raster may legitimately collapse to one owner. Distinct
  // old normal fields must not disappear merely because positions coincide.
  size_t raster_work=atlas->diagnostics.raster_samples;std::vector<uint8_t> covered(target.indices.size()/3);
  auto audit=[&](const Image &out,size_t at,const std::shared_ptr<const Image> &image,std::array<float,2> uv){
    PaintSourceTexture view;view.image=image;view.wrap_s=view.wrap_t=33071;view.min_filter=view.mag_filter=9729;auto value=sample_source_texture(view,uv[0],uv[1],false);
    Vec3 wanted{value[0]*2-1,value[1]*2-1,value[2]*2-1};if(length(wanted)<1e-8f)throw std::invalid_argument("Prepared normal UV constraint has a singular decoder direction");wanted=normalized(wanted);
    const auto decoded=normalized(Vec3{unit(out.rgba[at*4])*2-1,unit(out.rgba[at*4+1])*2-1,unit(out.rgba[at*4+2])*2-1});
    result.observed_sample_error=std::max(result.observed_sample_error,std::atan2(double(length(cross(wanted,decoded))),std::clamp(double(dot(wanted,decoded)),-1.,1.)));
  };
  for(size_t face=0;face<covered.size();++face){stop();const auto a=target.uv[target.indices[face*3]],b=target.uv[target.indices[face*3+1]],c=target.uv[target.indices[face*3+2]];
    const double denominator=(double(b[1])-c[1])*(a[0]-c[0])+(double(c[0])-b[0])*(a[1]-c[1]);
    if(std::abs(denominator)<1e-12)throw std::invalid_argument("Normal UV projection has an unrepresented destination face");
    const int x0=std::max(0,int(std::floor(std::min({a[0],b[0],c[0]})*r))),x1=std::min(int(r)-1,int(std::ceil(std::max({a[0],b[0],c[0]})*r)));
    const int y0=std::max(0,int(std::floor(std::min({a[1],b[1],c[1]})*r))),y1=std::min(int(r)-1,int(std::ceil(std::max({a[1],b[1],c[1]})*r)));
    for(int y=y0;y<=y1;++y)for(int x=x0;x<=x1;++x){if(++raster_work>options.max_samples)throw std::length_error("Normal UV source-constraint raster exceeds its work budget");if((raster_work&1023)==0)stop();double u=(x+.5)/r,v=(y+.5)/r;float w[]={float(((double(b[1])-c[1])*(u-c[0])+(double(c[0])-b[0])*(v-c[1]))/denominator),float(((double(c[1])-a[1])*(u-c[0])+(double(a[0])-c[0])*(v-c[1]))/denominator),0};w[2]=1-w[0]-w[1];if(std::min({w[0],w[1],w[2]})<-.000001f)continue;
      const size_t at=size_t(y)*r+size_t(x);if(!atlas->texels[at].covered)throw std::logic_error("Normal UV source constraint lost destination ownership");covered[face]=1;++result.source_constraints;
      if(atlas->texels[at].face==face)continue;std::array<float,2> uv{};for(size_t k=0;k<3;++k){const auto q=before.uv[before.indices[face*3+k]];uv[0]+=q[0]*w[k];uv[1]+=q[1]*w[k];}audit(*pbr,at,source.normal_projection_->pbr,uv);audit(*toon,at,source.normal_projection_->mtoon,uv);
    }
    if(!covered[face])for(const auto weights:{std::array<float,3>{1,0,0},std::array<float,3>{0,1,0},std::array<float,3>{0,0,1},std::array<float,3>{1.f/3,1.f/3,1.f/3}}){
      if(++raster_work>options.max_samples)throw std::length_error("Normal UV subpixel source constraints exceed work budget");
      std::array<float,2> old_uv{},new_uv{};for(size_t k=0;k<3;++k){auto oq=before.uv[before.indices[face*3+k]],nq=target.uv[target.indices[face*3+k]];for(size_t c=0;c<2;++c){old_uv[c]+=oq[c]*weights[k];new_uv[c]+=nq[c]*weights[k];}}
      for(const auto &images:{std::pair{std::shared_ptr<const Image>(pbr),source.normal_projection_->pbr},std::pair{std::shared_ptr<const Image>(toon),source.normal_projection_->mtoon}}){PaintSourceTexture old_view,new_view;old_view.image=images.second;new_view.image=images.first;old_view.wrap_s=old_view.wrap_t=new_view.wrap_s=new_view.wrap_t=33071;
        auto o=sample_source_texture(old_view,old_uv[0],old_uv[1],false),n=sample_source_texture(new_view,new_uv[0],new_uv[1],false);Vec3 wanted{o[0]*2-1,o[1]*2-1,o[2]*2-1},actual{n[0]*2-1,n[1]*2-1,n[2]*2-1};if(length(wanted)<1e-8f||length(actual)<1e-8f)throw std::invalid_argument("Subpixel normal UV constraint has a singular direction");wanted=normalized(wanted);actual=normalized(actual);
        result.observed_sample_error=std::max(result.observed_sample_error,std::atan2(double(length(cross(wanted,actual))),std::clamp(double(dot(wanted,actual)),-1.,1.)));
      }++result.source_constraints;
    }
  }
  result.raster_samples=raster_work;
  if(!result.sampled_texels||result.observed_sample_error>options.max_angular_error)throw std::invalid_argument("Prepared normal UV quantization/source constraints exceed the declared sample quality bound");
  auto projection=std::make_shared<PaintNormalProjection>();projection->pbr=std::move(pbr);projection->mtoon=std::move(toon);projection->validate(r);result.projection=std::move(projection);stop();return result;
}
void BakedTextures::validate(uint32_t resolution) const {
  if (maps.current_grid_preview || maps.overlay || maps.source_maps || maps.source_only || maps.baked_base || maps.color_projection || maps.pbr_transparent || !maps.emission_coverage.rgba.empty() || maps.emission_coverage.width || maps.emission_coverage.height || !maps.color_coverage.rgba.empty() || maps.color_coverage.width || maps.color_coverage.height || !maps.baked_normal.rgba.empty() || maps.baked_normal.width || maps.baked_normal.height || !maps.baked_coverage.rgba.empty() || maps.baked_coverage.width || maps.baked_coverage.height) throw std::invalid_argument("Attach original maps through from_maps, not a baked payload");
  if(size_t(color_interpretation)>1 || (color_interpretation==BakedColorInterpretation::ResolvedPbr)!=!maps.pbr_color.rgba.empty())
    throw std::invalid_argument("Baked PBR color interpretation is incomplete");
  if(size_t(normal_interpretation)>2||maps.normal_projection||
     (normal_interpretation!=BakedNormalInterpretation::PreparedAttributes&&!maps.mtoon_normal.rgba.empty())||
     (normal_interpretation==BakedNormalInterpretation::NativeAttributes&&
      (maps.normal.rgba.empty()||color_interpretation!=BakedColorInterpretation::VertexComponent))||
     (normal_interpretation==BakedNormalInterpretation::PreparedAttributes&&maps.normal.rgba.empty()!=maps.mtoon_normal.rgba.empty()))
    throw std::invalid_argument("Baked normal interpretation is incomplete");
  if(normal_authority){
    if(normal_interpretation!=BakedNormalInterpretation::PreparedAttributes||
       maps.normal.rgba.empty()||maps.mtoon_normal.rgba.empty())
      throw std::invalid_argument("Retained normal identity requires prepared detail channels");
    normal_authority->validate(resolution);
  }
  if(normal_field){
    normal_field->validate();
    if(normal_field->resolution()!=resolution||normal_authority||maps.normal_projection||
       normal_interpretation!=BakedNormalInterpretation::PreparedAttributes||!maps.normal.rgba.empty()||!maps.mtoon_normal.rgba.empty())
      throw std::invalid_argument("Independent baked normal field has contradictory role/coverage ownership");
  }
  if (resolution < 64 || resolution > 2048 ||
      coverage.size() != size_t(resolution) * resolution)
    throw std::invalid_argument("Baked coverage differs from the painting surface");
  if(emission_field&&(emission_field->resolution()!=resolution||!maps.emission.rgba.empty()||maps.emission.width||maps.emission.height))
    throw std::invalid_argument("FLOAT baked emission needs its exact grid and no duplicate RGB8 authority");
  for (auto image : {&maps.color, &maps.metal_rough, &maps.emission, &maps.normal, &maps.pbr_color,&maps.mtoon_normal}) {
    if(image==&maps.emission&&emission_field)continue;
    if ((image == &maps.normal || image == &maps.pbr_color||image==&maps.mtoon_normal) && image->rgba.empty() && !image->width && !image->height)
      continue;
    image->validate();
    if (image->width != resolution || image->height != resolution)
      throw std::invalid_argument("Baked image dimensions differ from the painting surface");
  }
}
size_t BakedTextures::storage_bytes() const {
  return maps.color.rgba.size() + maps.metal_rough.rgba.size() +
         maps.emission.rgba.size() + maps.normal.rgba.size() + maps.mtoon_normal.rgba.size()+maps.pbr_color.rgba.size() + coverage.size()+(normal_field?normal_field->retained_bytes():0)+(emission_field?emission_field->storage_bytes():0);
}
size_t PaintedMesh::storage_bytes() const {
  size_t result = (baked_ ? baked_->storage_bytes() : 0)+(color_projection_?color_projection_->vertex_rgba->rgba.size():0);
  if(source_maps_&&source_maps_->uv1)result+=source_maps_->uv1->size()*sizeof(std::array<float,2>);
  if(source_maps_)for(const auto &[set,uv]:source_maps_->extra_uv_sets)result+=uv->size()*sizeof(std::array<float,2>);
  if(normal_projection_)result+=normal_projection_->pbr->rgba.size()+normal_projection_->mtoon->rgba.size();
  for (const auto &layer : layers_)
    for (const auto &tile : layer.tiles)
      if (tile) result += sizeof(PaintTile);
  for(const auto &layer:layers_)result+=color_tile_count(layer)*sizeof(PaintColorTile);
  if(source_sampling_prepared()) {
    const auto original=source_sampling_original();const auto &m=original->mesh();
    if(const auto &uv1=surface_->original_sampling->authored_uv1)result+=(uv1->size()+surface_->original_sampling->original_uv1->size())*sizeof(std::array<float,2>);
    result+=original->storage_bytes()+source_sampling_vertices().size()*(sizeof(uint32_t)+sizeof(std::array<float,2>))+
      m.positions.size()*sizeof(Vec3)+m.normals.size()*sizeof(Vec3)+m.uv.size()*sizeof(std::array<float,2>)+
      m.indices.size()*sizeof(uint32_t)+m.uv_pins.size()+m.uv_seams.size();
  }
  for(const auto &layer:layers_)result+=emission_tile_count(layer)*sizeof(PaintEmissionTile);
  return result;
}
void PaintedMesh::install_bake(BakedTextures incoming) {
  install_bake(std::move(incoming),BakedNormalWrite::Replace);
}

void detail::TextureSetPaintAccess::reproject_retopology(const PaintedMesh &from,
    PaintedMesh &replacement, RetopoTransfer &transfer,bool prepared_normals) {
  const auto &old_domain = from.texture_set_domain_;
  const auto &new_domain = replacement.texture_set_domain_;
  if (bool(old_domain) != bool(new_domain) || (old_domain && old_domain->set() != new_domain->set()))
    throw std::invalid_argument("Retopology paint must stay in its own material slot");
  if (old_domain) {
    old_domain->authenticate_owner(old_domain);
    new_domain->authenticate_owner(new_domain);
    if (!old_domain->matches(from.mesh_owner()) || !new_domain->matches(replacement.mesh_owner()))
      throw std::invalid_argument("Retopology paint lost its owning material geometry");
  }
  if (from.in_stroke() || from.source_maps_ || (from.sampling_attachment_ && !prepared_normals) ||
      bool(from.color_projection_) != bool(replacement.color_projection_) || from.normal_projection_ ||
      from.surface_->options.policy != PaintUvPolicy::Strict)
    throw std::invalid_argument("Retopology cannot reproject this retained material grid yet");
  const bool resolved_color = bool(replacement.color_projection_);
  const auto bake = from.baked_;
  if (bake && (bake->normal_authority || bool(bake->normal_field)!=prepared_normals ||
      bake->normal_interpretation != (prepared_normals?BakedNormalInterpretation::PreparedAttributes:
                                       BakedNormalInterpretation::CanonicalMikk) ||
      bake->color_interpretation != (resolved_color ? BakedColorInterpretation::ResolvedPbr :
                                                     BakedColorInterpretation::VertexComponent)))
    throw std::invalid_argument("Retopology cannot reproject this baked material interpretation yet");
  std::vector<std::array<float,2>> writable_coordinates;
  if (const auto domain=from.paint_sampling_domain()) {
    if (!from.valid_paint_sampling_attachment())
      throw std::invalid_argument("Retopology lost the authenticated writable paint grid");
    transfer.reserve(from.mesh().indices.size(),sizeof(std::array<float,2>));
    PaintSamplingLimits limits;
    limits.max_working_bytes=transfer.remaining_bytes();
    limits.cancelled=transfer.cancelled();
    writable_coordinates=domain->corner_coordinates(from.mesh(),limits);
  }
  // All writes target the detached new patch. Old layer IDs, active layer,
  // opacity, visibility and the monotonic allocation cursor stay meaningful.
  const auto &surface = *replacement.surface_;
  const auto texels = surface.texels.size();
  const auto tile_slots = surface.tiles.size();
  const auto resolution = surface.resolution;
  transfer.reserve(from.layers_.size(), sizeof(PaintLayer) +
      (resolved_color ? 6 : 4) * tile_slots * sizeof(std::shared_ptr<const PaintTile>));
  if (from.revision_ >= UINT64_MAX - bool(bake)) throw std::overflow_error("Paint revision exhausted");
  replacement.composite_cache_ = std::make_shared<PaintedMesh::CompositeCache>();
  replacement.composite_token_ = next_composite_token();
  replacement.active_ = from.active_;
  replacement.next_ = from.next_;
  replacement.revision_ = from.revision_ + 1;
  replacement.layers_.clear();
  replacement.layers_.reserve(from.layers_.size());

  struct WritableLayer {
    size_t slot;
    std::vector<std::shared_ptr<PaintTile>> common;
    std::vector<std::shared_ptr<PaintEmissionTile>> emission;
    std::vector<std::shared_ptr<PaintColorTile>> pbr;
  };
  std::vector<WritableLayer> writable;
  for (size_t slot = 0; slot < from.layers_.size(); ++slot) {
    const auto &layer = from.layers_[slot];
    replacement.layers_.push_back({layer.id, layer.name, layer.visible, layer.opacity,
                                   std::vector<std::shared_ptr<const PaintTile>>(tile_slots), {}, {}});
    if (resolved_color) replacement.layers_.back().pbr_tiles.resize(tile_slots);
    if (tile_count(layer) || emission_tile_count(layer)) {
      writable.push_back({slot, {}, {}, {}});
      writable.back().common.resize(tile_slots);
    }
  }
  if (writable.empty() && !bake) return;

  std::optional<BakedTextures> baked;
  std::vector<BakedEmissionSample> float_bake;
  std::vector<Tangent> source_tangents, target_tangents;
  size_t layer_bytes = resolved_color ? replacement.color_projection_->vertex_rgba->rgba.size() : 0;
  constexpr size_t paint_limit = 128 * 1024 * 1024;
  if (bake) {
    const bool normals = !bake->maps.normal.rgba.empty();
    transfer.reserve(texels, 13 + ((normals || prepared_normals) ? 4 : 0) + (resolved_color ? 4 : 0));
    baked.emplace();
    baked->coverage.resize(texels);
    baked->maps.color = blank_image(resolution);
    baked->maps.metal_rough = blank_image(resolution);
    baked->maps.emission = blank_image(resolution);
    if (resolved_color) {
      baked->color_interpretation = BakedColorInterpretation::ResolvedPbr;
      baked->maps.pbr_color = blank_image(resolution);
    }
    if (prepared_normals) baked->maps.normal = blank_image(resolution,true);
    if (normals) {
      transfer.reserve(from.mesh().indices.size() + replacement.mesh().indices.size(), 4 * sizeof(Tangent));
      baked->maps.normal = blank_image(resolution, true);
      source_tangents = tangent_corners(from.mesh(), transfer.cancelled());
      target_tangents = tangent_corners(replacement.mesh(), transfer.cancelled());
    }
    if (bake->emission_field) {
      transfer.reserve(texels, 2 * sizeof(BakedEmissionSample));
      float_bake.resize(texels);
    }
  }

  auto allocate_tile = [&](auto &tile) {
    if (tile) return;
    using Tile = typename std::decay_t<decltype(tile)>::element_type;
    if (sizeof(Tile) > paint_limit - layer_bytes)
      throw std::length_error("Retopology exceeds editable paint storage");
    layer_bytes += sizeof(Tile);
    transfer.reserve(1, sizeof(Tile));
    tile = std::make_shared<Tile>();
  };
  size_t hits = 0;
  for (size_t at = 0; at < texels; ++at) {
    if ((at & 1023) == 0) paint_compute::checkpoint(transfer.cancelled());
    const auto &texel = surface.texels[at];
    if (!texel.covered) continue;
    const auto &w = texel.bary;
    const auto hit = transfer.match_patch(texel.position,texel.normal,texel.face,
                                         {w[0],w[1],1-w[0]-w[1]},old_domain.get());
    if (!hit) continue;
    ++hits;
    std::array<float, 2> uv{};
    for (size_t corner = 0; corner < 3; ++corner) {
      const auto index = size_t(hit->triangle) * 3 + corner;
      const auto value = writable_coordinates.empty()?from.mesh().uv[from.mesh().indices[index]]:
                                                      writable_coordinates[index];
      for (size_t axis = 0; axis < 2; ++axis)
        uv[axis] += value[axis] * hit->barycentric[corner];
    }
    const auto taps = paint_bilinear_taps(from.resolution(), uv, from.surface_->padding);
    transfer.work((resolved_color ? 48 : 32) + 128 * writable.size());
    if (bake) {
      BakedPixel values{};
      std::array<float,4> pbr{};
      float coverage = 0;
      for (const auto &tap : taps) {
        const auto weight = float(tap.weight);
        const auto alpha = weight * unit(bake->coverage[tap.texel]);
        if (alpha > 0) {
          const auto sample = baked_pixel(*bake, tap.texel);
          for (size_t i = 0; i < values.size(); ++i) values[i] += sample[i] * alpha;
          if (resolved_color)
            for (size_t k = 0; k < pbr.size(); ++k) {
              const auto color = unit(bake->maps.pbr_color.rgba[tap.texel * 4 + k]);
              pbr[k] += (k < 3 ? srgb_to_linear(color) : color) * alpha;
            }
          coverage += alpha;
        }
        if (!float_bake.empty() && weight > 0) {
          const auto sample = bake->emission_field->texel(tap.texel);
          auto &dest = float_bake[at];
          for (size_t i = 0; i < 3; ++i) {
            dest.legacy[i] += sample.legacy[i] * weight;
            dest.absolute[i] += sample.absolute[i] * weight;
          }
          dest.coverage += sample.coverage * weight;
        }
      }
      if (!float_bake.empty()) {
        auto &sample = float_bake[at];
        sample.coverage = std::clamp(sample.coverage, 0.f, 1.f);
        for (size_t k = 0; k < 3; ++k) {
          sample.legacy[k] = std::min(sample.legacy[k], sample.coverage);
          sample.absolute[k] = std::min(sample.absolute[k], sample.coverage * maximum_native_emission);
        }
      }
      if (coverage > 0) {
        for (auto &value : values) value /= coverage;
        if (!source_tangents.empty()) {
          const auto old_frame = tangent_frame(from.mesh(), source_tangents, hit->triangle, hit->barycentric);
          const auto new_frame = tangent_frame(replacement.mesh(), target_tangents, texel.face,
              {texel.bary[0], texel.bary[1], 1 - texel.bary[0] - texel.bary[1]});
          const Vec3 old_normal = old_frame.to_world({values[10], values[11], values[12]});
          const auto changed = transfer.placement_change().normal({old_normal.x, old_normal.y, old_normal.z});
          const auto new_normal = new_frame.to_tangent({float(changed[0]), float(changed[1]), float(changed[2])});
          values[10] = new_normal.x; values[11] = new_normal.y; values[12] = new_normal.z;
        }
        baked->coverage[at] = byte(coverage);
        baked_pixel(*baked, at, values);
        if (resolved_color)
          for (size_t k = 0; k < pbr.size(); ++k) {
            const auto value = pbr[k] / coverage;
            baked->maps.pbr_color.rgba[at * 4 + k] = byte(k < 3 ? linear_to_srgb(value) : value);
          }
      }
    }
    const auto x = at % resolution, y = at / resolution;
    const auto tile = y / 32 * surface.columns + x / 32;
    const auto pixel = y % 32 * 32 + x % 32;
    for (auto &layer : writable) {
      const auto &old_layer = from.layers_[layer.slot];
      auto sample = filter_paint_layer(old_layer, from.resolution(), taps);
      const bool emission = !old_layer.emission_tiles.empty() && sample.emission != PaintEmissionValue{};
      if (!old_layer.emission_tiles.empty()) {
        // One FLOAT texel makes the entire tile authoritative. Canonicalize
        // the byte fallback even for neighbors sampled from legacy-only tiles.
        sample.common.data[12] = byte(sample.emission.coverage);
        for (size_t k = 0; k < 3; ++k)
          sample.common.data[6 + k] = byte(linear_to_srgb(sample.emission.legacy[k]));
      }
      if (sample.common == PaintPixel{} && !emission && !sample.pbr_present) continue;
      auto &dest = replacement.layers_[layer.slot];
      auto &common = layer.common[tile];
      allocate_tile(common);
      dest.tiles[tile] = common;
      common->pixels[pixel] = sample.common;
      if (emission) {
        if (layer.emission.empty()) { layer.emission.resize(tile_slots); dest.emission_tiles.resize(tile_slots); }
        allocate_tile(layer.emission[tile]);
        layer.emission[tile]->pixels[pixel] = sample.emission;
        dest.emission_tiles[tile] = layer.emission[tile];
      }
      if (resolved_color) {
        // A tile is authoritative for all its pixels, including neighbors
        // sampled from common-only tiles. The filter supplies their fallback.
        if (layer.pbr.empty()) layer.pbr.resize(tile_slots);
        auto &pbr = layer.pbr[tile];
        allocate_tile(pbr);
        dest.pbr_tiles[tile] = pbr;
        pbr->pixels[pixel] = sample.pbr;
      }
    }
  }
  if (!hits && (!old_domain || (!old_domain->triangles().empty() && !new_domain->triangles().empty())))
    throw std::invalid_argument("No matching old surface for the retopology paint or Bake");
  for (const auto &layer : replacement.layers_) {
    replacement.allocated_tiles_ += tile_count(layer);
    replacement.allocated_color_tiles_ += color_tile_count(layer);
  }
  if (baked) {
    if (!float_bake.empty()) {
      const auto other = layer_bytes + baked->storage_bytes() - baked->maps.emission.rgba.size();
      if (other >= paint_limit) throw std::length_error("Retopology Bake exceeds paint storage");
      baked->emission_field = BakedEmissionField::generate_compact(resolution,
          [&](size_t at) { return float_bake[at]; }, {paint_limit - other, transfer.cancelled()});
      baked->maps.emission = {};
    }
    SourcePaintAccess::merge_bake(replacement, std::move(*baked), BakedNormalWrite::Replace, transfer.cancelled());
  }
  paint_compute::checkpoint(transfer.cancelled());
}

namespace {
using ToonUv = std::array<float,2>;

struct RetopoToonUv {
  const MToonTexture &texture;
  double cosine, sine;

  explicit RetopoToonUv(const MToonTexture &t)
      : texture(t), cosine(std::cos(double(t.rotation))), sine(std::sin(double(t.rotation))) {}

  ToonUv direction(ToonUv uv) const {
    const double u=double(uv[0])*texture.scale[0], v=double(uv[1])*texture.scale[1];
    return {float(cosine*u-sine*v),float(sine*u+cosine*v)};
  }
  ToonUv point(ToonUv uv) const {
    auto q=direction(uv);
    q[0]+=texture.offset[0];
    q[1]+=texture.offset[1];
    return q;
  }
};

struct RetopoToonDerivative { Vec3 tangent{}, bitangent{}; };

std::vector<RetopoToonDerivative> retopo_toon_derivatives(const Mesh &mesh,
    const detail::AffineInteractionFrame &placement, const RetopoToonUv &mapping,
    detail::RetopoTransfer &transfer) {
  transfer.reserve(mesh.indices.size()/3,sizeof(RetopoToonDerivative));
  std::vector<RetopoToonDerivative> output(mesh.indices.size()/3);
  for (size_t face=0;face<output.size();++face) {
    if ((face&1023)==0) paint_compute::checkpoint(transfer.cancelled());
    transfer.work(16);
    const auto a=mesh.indices[face*3], b=mesh.indices[face*3+1], c=mesh.indices[face*3+2];
    const auto u=mapping.direction({mesh.uv[b][0]-mesh.uv[a][0],mesh.uv[b][1]-mesh.uv[a][1]});
    const auto v=mapping.direction({mesh.uv[c][0]-mesh.uv[a][0],mesh.uv[c][1]-mesh.uv[a][1]});
    const auto determinant=u[0]*v[1]-u[1]*v[0];
    if (std::abs(determinant)<=1e-8f*std::max(std::hypot(u[0],u[1])*std::hypot(v[0],v[1]),1e-20f))
      continue;
    const auto x=detail::affine_position(placement.world().direction(detail::affine_vector(mesh.positions[b]-mesh.positions[a])));
    const auto y=detail::affine_position(placement.world().direction(detail::affine_vector(mesh.positions[c]-mesh.positions[a])));
    const auto sign=std::copysign(1.f,determinant);
    output[face]={(x*v[1]-y*u[1])*sign,(y*u[0]-x*v[0])*sign};
  }
  return output;
}

struct RetopoToonFrame {
  Vec3 t,b,n;
  bool valid=false;
  Vec3 mapped(Vec3 normal, float side=1) const {
    return valid ? normalized(t*normal.x+(b*normal.y+n*normal.z)*side) : n*side;
  }
};

RetopoToonFrame retopo_toon_frame(const Mesh &mesh,const detail::AffineInteractionFrame &placement,
    const RetopoToonDerivative &derivative,size_t face,std::array<float,3> bary) {
  Vec3 n{};
  for (size_t k=0;k<3;++k)
    n+=placed_normal(placement,mesh.normals[mesh.indices[face*3+k]])*bary[k];
  n=normalized(n);
  const auto projected=derivative.tangent-n*dot(n,derivative.tangent);
  if (length(projected)==0) return {{},{},n};
  const auto t=normalized(projected);
  const auto b=cross(n,t);
  return {t,b*(dot(b,derivative.bitangent)<0?-1.f:1.f),n,true};
}

std::array<float,4> retopo_toon_normal(std::array<float,4> encoded,
    const RetopoToonFrame &source,const RetopoToonFrame &target,float scale,bool back) {
  const auto input=mtoon_normal_map({encoded[0],encoded[1],encoded[2]},scale);
  const auto world=source.mapped(input);
  std::array<double,3> raw{0,0,dot(target.n,world)};
  if (scale!=0) {
    raw[0]=double(dot(target.t,world))/scale;
    raw[1]=double(dot(target.b,world))/scale;
  }
  const auto maximum=std::max({std::abs(raw[0]),std::abs(raw[1]),std::abs(raw[2]),1e-20});
  for (size_t i=0;i<3;++i) encoded[i]=unit(byte(float(.5+.5*raw[i]/maximum)));
  const auto result=mtoon_normal_map({encoded[0],encoded[1],encoded[2]},scale);
  // V1 reverses B and N on back faces. One atlas image can carry both
  // orientations only when the two angle checks pass after quantization.
  constexpr float accepted_cosine=.9998f;
  if (dot(world,target.mapped(result))<accepted_cosine ||
      (back && dot(source.mapped(input,-1),target.mapped(result,-1))<accepted_cosine))
    throw std::invalid_argument("MToon retopology cannot preserve the typed normal frame within its angular bound");
  return encoded;
}
}

std::shared_ptr<const SourceMaterialBindings> detail::TextureSetPaintAccess::reproject_retopology_mtoon(
    const PaintedMesh &from,const PaintedMesh &replacement,
    const std::shared_ptr<const SourceMaterialBindings> &binding,RetopoTransfer &transfer) {
  const auto &material=binding->material;
  const auto &parameters=material.parameters;
  const bool legacy=material.version()==MToonVersion::V0;
  const bool animated=parameters.scroll!=ToonUv{} || parameters.rotation_speed!=0;
  constexpr auto matcap=size_t(MToonSlot::Matcap), normal=size_t(MToonSlot::Normal);
  std::vector<size_t> roles;
  uint32_t resolution=replacement.resolution();
  for (size_t role=0;role<binding->roles.size();++role) {
    const auto &source=binding->roles[role];
    if (!source.image || role==matcap) continue;
    if (animated && role!=size_t(MToonSlot::UvMask) && !(legacy && role==size_t(MToonSlot::OutlineWidth)))
      throw std::invalid_argument("MToon retopology needs a live sampling adapter for animated texture roles");
    resolution=std::max({resolution,source.image->width,source.image->height});
    roles.push_back(role);
  }
  if (roles.empty()) return binding;
  if (resolution>2048) throw std::length_error("MToon retopology role images exceed its atlas resolution limit");
  resolution=std::bit_ceil(resolution);
  transfer.reserve(1,sizeof(SourceMaterialBindings)+
      material.copy_working_bytes(transfer.remaining_bytes(),transfer.cancelled()));
  transfer.reserve(size_t(resolution)*resolution,4*roles.size());
  std::shared_ptr<PaintedMesh> separate_grid;
  if (resolution!=replacement.resolution()) {
    transfer.reserve(1,initial_raster_bytes(resolution));
    TextureSetLimits limits;
    limits.cancelled=transfer.cancelled();
    limits.maximum_work=transfer.remaining_work();
    separate_grid=empty(replacement.texture_set_domain_,resolution,limits);
    const auto &stats=separate_grid->surface_diagnostics();
    transfer.work(stats.raster_samples+stats.candidate_checks);
  }
  const auto &surface=*(separate_grid?separate_grid->surface_:replacement.surface_);
  const auto &old_mesh=from.mesh(), &new_mesh=replacement.mesh();
  const AffineInteractionFrame previous(transfer.previous_metric()), next(transfer.replacement_metric());
  const MToonTexture identity;
  const auto normal_slot=legacy?size_t(MToonSlot::Base):normal;
  const bool reorient=binding->roles[normal].image && parameters.normal_enabled;
  const bool check_back=!legacy && (parameters.cull!=MToonCull::Back ||
      (parameters.outline!=MToonOutline::None && parameters.outline_width>0 &&
       parameters.outline_cull!=MToonCull::Back));
  std::vector<RetopoToonDerivative> old_derivatives,new_derivatives;
  if (reorient) {
    old_derivatives=retopo_toon_derivatives(old_mesh,previous,RetopoToonUv(material.textures[normal_slot]),transfer);
    new_derivatives=retopo_toon_derivatives(new_mesh,next,RetopoToonUv(identity),transfer);
  }
  auto result=std::make_shared<SourceMaterialBindings>(*binding);
  std::array<std::shared_ptr<Image>,size_t(MToonSlot::Count)> images;
  std::array<std::optional<RetopoToonUv>,size_t(MToonSlot::Count)> mappings;
  for (const auto role:roles) {
    mappings[role].emplace(material.textures[legacy?size_t(MToonSlot::Base):role]);
    auto &image=images[role];
    image=std::make_shared<Image>(blank_image(resolution));
    result->roles[role].image=image;
    result->roles[role].wrap_s=result->roles[role].wrap_t=33071;
    auto &texture=result->material.textures[role];
    texture.texcoord=0;
    texture.transform_texcoord=-1;
    texture.offset={};
    texture.scale={1,1};
    texture.rotation=0;
  }
  if (legacy) {
    auto &main=result->material.textures[size_t(MToonSlot::Base)];
    main.offset={};
    main.scale={1,1};
    main.rotation=0;
  }
  size_t hits=0;
  for (size_t index=0;index<surface.texels.size();++index) {
    if ((index&1023)==0) paint_compute::checkpoint(transfer.cancelled());
    transfer.work(1+roles.size());
    const auto &texel=surface.texels[index];
    if (!texel.covered && surface.padding[index]>=0) continue;
    const auto &w=texel.bary;
    const auto hit=texel.covered ? transfer.match_patch(texel.position,texel.normal,texel.face,
        {w[0],w[1],1-w[0]-w[1]},from.texture_set_domain_.get()) : std::nullopt;
    ToonUv uv{};
    if (hit) {
      ++hits;
      for (size_t c=0;c<3;++c) {
        const auto &corner=old_mesh.uv[old_mesh.indices[size_t(hit->triangle)*3+c]];
        for (size_t axis=0;axis<2;++axis) uv[axis]+=hit->barycentric[c]*corner[axis];
      }
      transfer.work(16*roles.size()+(reorient?48:0));
    }
    for (const auto role:roles) {
      auto value=mtoon_default_texel(MToonSlot(role));
      const bool srgb=material.textures[role].encoding==MToonEncoding::Srgb;
      if (hit) {
        const auto q=mappings[role]->point(uv);
        value=sample_source_texture(binding->roles[role],q[0],q[1],srgb);
        if (role==normal && reorient)
          value=retopo_toon_normal(value,
              retopo_toon_frame(old_mesh,previous,old_derivatives[hit->triangle],hit->triangle,hit->barycentric),
              retopo_toon_frame(new_mesh,next,new_derivatives[texel.face],texel.face,
                  {texel.bary[0],texel.bary[1],1-texel.bary[0]-texel.bary[1]}),
              parameters.normal_scale,check_back);
      }
      for (size_t channel=0;channel<4;++channel)
        images[role]->rgba[index*4+channel]=byte(srgb&&channel<3?linear_to_srgb(value[channel]):value[channel]);
    }
  }
  if (!hits && !from.texture_set_domain_->triangles().empty() &&
      !replacement.texture_set_domain_->triangles().empty())
    throw std::invalid_argument("No matching old surface for the retopology MToon textures");
  for (size_t index=0;index<surface.texels.size();++index) {
    if ((index&1023)==0) paint_compute::checkpoint(transfer.cancelled());
    if (surface.texels[index].covered || surface.padding[index]<0) continue;
    transfer.work(roles.size());
    for (const auto role:roles) {
      auto &pixels=images[role]->rgba;
      std::copy_n(pixels.begin()+size_t(surface.padding[index])*4,4,pixels.begin()+index*4);
    }
  }
  result->validate();
  return result;
}

std::shared_ptr<const detail::BakedNormalField> detail::TextureSetPaintAccess::reproject_retopology_normals(
    const PaintedMesh &from,const SourceMaterialBindings &before,
    const PaintedMesh &replacement,const SourceMaterialBindings &after,RetopoTransfer &transfer) {
  const auto source=from.baked()?from.baked()->normal_field:nullptr;
  const auto baseline=replacement.baked()?replacement.baked()->normal_field:nullptr;
  if (!source || !baseline || source->origin()!=before.canonical_attributes ||
      baseline->origin()!=after.canonical_attributes || !before.normal_frames || !before.normal_frames->pbr ||
      !after.normal_frames || !after.normal_frames->pbr ||
      before.normal_frames->pbr->normals.size()!=from.mesh().indices.size() ||
      after.normal_frames->pbr->normals.size()!=replacement.mesh().indices.size())
    throw std::invalid_argument("Retopology lost its original or replacement prepared normal basis");
  // Reconstruct the actual placed columns used by the renderer on both
  // atlases. Transforming the final sampled vector cannot account for shear
  // or the discrete tangent sign of reflected source triangles.
  const auto columns=[&](const NormalFrames &frames,const AffineTransform &matrix) {
    transfer.reserve(frames.normals.size(),sizeof(NormalBakeVertex));
    const AffineInteractionFrame placement(matrix);
    std::vector<NormalBakeVertex> result;
    result.reserve(frames.normals.size());
    for (size_t corner=0;corner<frames.normals.size();++corner) {
      if ((corner&1023)==0) paint_compute::checkpoint(transfer.cancelled());
      transfer.work();
      const auto t=placed_tangent(placement,frames.tangents.at(corner));
      result.push_back(normal_bake_vertex(placed_normal(placement,frames.normals[corner]),
                                         {t[0],t[1],t[2]},t[3],NormalVertexBitangent::UnitCross));
    }
    return result;
  };
  const auto source_columns=columns(*before.normal_frames->pbr,transfer.previous_metric());
  const auto target_columns=columns(*after.normal_frames->pbr,transfer.replacement_metric());
  const auto basis=[](const std::vector<NormalBakeVertex> &values,uint32_t face,std::array<double,3> bary) {
    std::array<NormalBakeVertex,3> v;
    std::copy_n(values.begin()+size_t(face)*3,3,v.begin());
    return normal_bake_basis(v,bary,NormalFragmentBasis::UnitColumns);
  };
  const auto &surface=*replacement.surface_;
  const auto count=surface.texels.size();
  transfer.reserve(count,9);
  std::array<Image,2> planes{blank_image(surface.resolution,true),blank_image(surface.resolution,true)};
  std::vector<uint8_t> coverage(count);
  const auto &old_mesh=from.mesh();
  const auto &normal_uv=before.canonical_uv;
  const auto bound=before.canonical_attributes->receipt().accepted_angular_error;
  size_t hits=0;
  for (size_t index=0;index<count;++index) {
    if ((index&1023)==0) paint_compute::checkpoint(transfer.cancelled());
    const auto &texel=surface.texels[index];
    if (!texel.covered) continue;
    const auto &w=texel.bary;
    const auto hit=transfer.match_patch(texel.position,texel.normal,texel.face,{w[0],w[1],1-w[0]-w[1]});
    if (!hit) continue;
    transfer.work(64);
    ++hits;
    std::array<float,2> uv{};
    for (size_t corner=0;corner<3;++corner) {
      const auto i=size_t(hit->triangle)*3+corner;
      const auto source_uv=normal_uv?normal_uv->coordinates().at(normal_uv->mesh()->indices.at(i)):
                                     old_mesh.uv.at(old_mesh.indices.at(i));
      for (size_t axis=0;axis<uv.size();++axis) uv[axis]+=source_uv[axis]*hit->barycentric[corner];
    }
    const auto &b=hit->barycentric;
    const auto sampled=source->sample(basis(source_columns,hit->triangle,{b[0],b[1],b[2]}),uv);
    const auto target=basis(target_columns,texel.face,{w[0],w[1],1.-w[0]-w[1]});
    for (size_t role=0;role<sampled.size();++role) {
      const auto value=encode_normal_bake(target,sampled[role]);
      if (!value || value->quantized_angle_radians>bound)
        throw std::invalid_argument("Retopology exceeds the prepared normal encoding bound");
      std::copy(value->rgba.begin(),value->rgba.end(),planes[role].rgba.begin()+index*4);
    }
    coverage[index]=255;
  }
  if (!hits) throw std::invalid_argument("No matching old surface for the prepared retopology normals");
  // Preserve the chart's filtering footprint independently of material
  // coverage. A ray miss inside a chart uses the neutral baseline.
  for (size_t index=0;index<count;++index) {
    if ((index&1023)==0) paint_compute::checkpoint(transfer.cancelled());
    const auto owner=surface.padding[index];
    if (owner<0 || size_t(owner)==index || !coverage[size_t(owner)]) continue;
    for (auto &plane:planes)
      std::copy_n(plane.rgba.begin()+size_t(owner)*4,4,plane.rgba.begin()+index*4);
    coverage[index]=255;
  }
  BakedNormalFieldOptions options;
  options.max_working_bytes=transfer.remaining_bytes();
  options.cancelled=transfer.cancelled();
  auto result=baseline->replace_planes(planes[0],planes[1],coverage,options);
  transfer.reserve(1,result->retained_bytes());
  return result;
}
void PaintedMesh::install_bake(BakedTextures incoming,BakedNormalWrite normal_write) {
  if(source_sampling_prepared())throw std::logic_error("Independent source paint requires its own source-aware bake adapter");
  if(source_maps_&&source_maps_->uses_alternate_uv())
    throw std::logic_error("Prepare genuine source atlas ownership before installing baked data on original coordinates");
  SourcePaintAccess::merge_bake(*this,std::move(incoming),normal_write);
}
void SourcePaintAccess::merge_bake(PaintedMesh &paint,BakedTextures incoming,BakedNormalWrite normal_write,const std::function<bool()> &cancelled) {
  paint_compute::checkpoint(cancelled);
  if(paint.surface_->source_capture_only)
    throw std::logic_error(detail::editable_texture_atlas_notice);
  if (paint.editing_) throw std::logic_error("Finish the paint stroke before baking");
  if(size_t(normal_write)>1)throw std::invalid_argument("Invalid normal bake write intent");
  incoming.validate(paint.resolution());
  if(incoming.normal_authority)
    throw std::invalid_argument("Retained normal identity requires a source-owning field transition");
  if(bool(paint.color_projection_)!=(incoming.color_interpretation==BakedColorInterpretation::ResolvedPbr))
    throw std::invalid_argument("Baked color interpretation must match explicit current color preparation");
  if(bool(paint.normal_projection_)||incoming.normal_field){
    if(incoming.normal_interpretation!=BakedNormalInterpretation::PreparedAttributes)
      throw std::invalid_argument("Baked field/preparation needs prepared normal interpretation");
  }else if(incoming.normal_interpretation==BakedNormalInterpretation::PreparedAttributes)
    throw std::invalid_argument("Baked normal interpretation must match explicit normal preparation");
  if(incoming.normal_interpretation==BakedNormalInterpretation::NativeAttributes&&
     (!paint.paint_sampling_domain()||paint.paint_sampling_domain()->version()!=1||
      !paint.valid_paint_sampling_attachment()||paint.source_maps_||paint.normal_projection_))
    throw std::invalid_argument("Authored native Bake requires its retained writable atlas");
  if(paint.normal_projection_&&incoming.normal_field)throw std::invalid_argument("Independent baked field cannot overwrite genuine intrinsic preparation");
  if(paint.baked_&&paint.baked_->normal_field&&(!incoming.normal_field||
      (incoming.normal_field!=paint.baked_->normal_field&&incoming.normal_field->prior()!=paint.baked_->normal_field)))
    throw std::invalid_argument("Independent normal rebake must retain its actual previous field");
  const bool retained=paint.normal_projection_&&paint.normal_projection_->version==2;
  const bool preserve_normal=normal_write==BakedNormalWrite::PreserveRetained;
  if(preserve_normal&&(!retained||!incoming.maps.normal.rgba.empty()||!incoming.maps.mtoon_normal.rgba.empty()))
    throw std::invalid_argument("Color-only normal preservation requires an explicit retained field and no incoming normal payload");
  if(retained&&!preserve_normal&&!incoming.maps.normal.rgba.empty()&&
     std::any_of(incoming.coverage.begin(),incoming.coverage.end(),[](auto a){return a!=0;}))
    throw std::invalid_argument("New normal baking requires an explicit retained-field node transition");
  if(retained&&!incoming.maps.normal.rgba.empty()&&
     (!paint.baked_||!paint.baked_->normal_authority))
    throw std::invalid_argument("A new normal payload requires an explicit retained-field node transition");
  if(retained&&paint.baked_&&!paint.baked_->maps.normal.rgba.empty()&&
     (!paint.baked_->normal_authority||paint.baked_->normal_authority!=paint.normal_projection_->sampling_domain->detail))
    throw std::invalid_argument("Current normal bake lost its retained interpretation identity");
  auto token = next_composite_token();
  if (!preserve_normal && paint.baked_ && !paint.baked_->maps.normal.rgba.empty() && incoming.maps.normal.rgba.empty())
    throw std::invalid_argument("A replacement bake must retain the existing normal channel");
  if (paint.storage_bytes() - (paint.baked_ ? paint.baked_->storage_bytes() : 0) +
          incoming.storage_bytes() > 128 * 1024 * 1024)
    throw std::length_error("Baked maps exceed paint storage budget");
  if (paint.baked_) {
    if(paint.baked_->color_interpretation!=incoming.color_interpretation)
      throw std::invalid_argument("Prepare old baked color authority before combining different interpretations");
    if(paint.baked_->normal_interpretation!=incoming.normal_interpretation)
      throw std::invalid_argument("Different normal interpretations require explicit source-retaining conversion");
    auto merged = *paint.baked_;
    merged.normal_field=incoming.normal_field;
    if(merged.emission_field||incoming.emission_field){
      // Independent FLOAT coverage, including values far below one R8 step.
      // Other channels continue their original common-coverage merge below.
      constexpr size_t maximum=128*1024*1024;
      const auto other=paint.storage_bytes()-paint.baked_->storage_bytes()+
          merged.storage_bytes()-merged.maps.emission.rgba.size()-
          (merged.emission_field?merged.emission_field->storage_bytes():0);
      if(other>=maximum)
        throw std::length_error("Merged FLOAT baked emission exceeds paint storage budget");
      const auto generate=paint.texture_set_domain_?BakedEmissionField::generate_compact:BakedEmissionField::generate;
      merged.emission_field=generate(paint.resolution(),[&](size_t i){
        return BakedEmissionField::over(detail::baked_emission_texel(*paint.baked_,i),
                                       detail::baked_emission_texel(incoming,i));
      },{maximum-other,cancelled});
      merged.maps.emission={};
    }
    if (merged.maps.normal.rgba.empty() && !incoming.maps.normal.rgba.empty())
      merged.maps.normal = blank_image(paint.resolution(), true);
    if(merged.normal_interpretation==BakedNormalInterpretation::PreparedAttributes&&merged.maps.mtoon_normal.rgba.empty()&&!incoming.maps.mtoon_normal.rgba.empty())merged.maps.mtoon_normal=blank_image(paint.resolution(),true);
    for (size_t i = 0; i < incoming.coverage.size(); ++i) {
      if((i&1023)==0)paint_compute::checkpoint(cancelled);
      float a = unit(incoming.coverage[i]), b = unit(paint.baked_->coverage[i]);
      if (!a) continue;
      float coverage = a + b * (1 - a);
      auto next = baked_pixel(incoming, i), prior = baked_pixel(*paint.baked_, i);
      if(incoming.color_interpretation==BakedColorInterpretation::ResolvedPbr)for(int c=0;c<4;++c) {
        const auto at=i*4+c;
        float x=unit(incoming.maps.pbr_color.rgba[at]),y=unit(paint.baked_->maps.pbr_color.rgba[at]);
        if(c<3){x=srgb_to_linear(x);y=srgb_to_linear(y);}
        const float value=(x*a+y*b*(1-a))/coverage;
        merged.maps.pbr_color.rgba[at]=byte(c<3?linear_to_srgb(value):value);
      }
      for (int c = 0; c < (preserve_normal?10:13); ++c) {
        next[c] = (next[c] * a + prior[c] * b * (1 - a)) / coverage;
      }
      baked_pixel(merged, i, next,!preserve_normal);
      if(incoming.normal_interpretation==BakedNormalInterpretation::PreparedAttributes&&!incoming.maps.mtoon_normal.rgba.empty())
        for(size_t c=0;c<3;++c){const auto at=i*4+c;const float x=unit(incoming.maps.mtoon_normal.rgba[at])*2-1,y=unit(merged.maps.mtoon_normal.rgba[at])*2-1;
          merged.maps.mtoon_normal.rgba[at]=byte(((x*a+y*b*(1-a))/coverage)*.5f+.5f);
        }
      merged.coverage[i] = byte(coverage);
    }
    incoming = std::move(merged);
  }
  if (paint.storage_bytes() - (paint.baked_ ? paint.baked_->storage_bytes() : 0) +
          incoming.storage_bytes() > 128 * 1024 * 1024)
    throw std::length_error("Baked maps exceed paint storage budget");
  paint.baked_ = std::make_shared<const BakedTextures>(std::move(incoming));
  paint.composite_token_ = token;
  ++paint.revision_;
}

std::shared_ptr<PaintedMesh> SourcePaintAccess::install(const PaintedMesh &current,BakedTextures incoming,
    const SourcePaintOptions &options){
  auto stop=[&]{paint_compute::checkpoint(options.cancelled);};stop();
  if(current.in_stroke()||!current.source_sampling_prepared()||!current.valid_source_sampling_attachment()||
     current.paint_sampling_domain()||current.color_projection_||current.normal_projection_)
    throw std::invalid_argument("Source Bake requires its genuine current source/atlas owner");
  if(current.surface_->diagnostics.additional_candidates||!current.surface_->diagnostics.unpaintable_uv_faces.empty())
    throw std::invalid_argument("Source Bake requires one exact writable atlas owner per current texel");
  const auto before=current.source_sampling_original();pristine(*before);
  budget(*before,current.mesh(),options,current.source_sampling_authored_uv1());
  authored(*before,current.source_sampling_authored_uv1(),options);
  auto expected=detail::transport_source_sampling(before->mesh(),maps(*before),current.mesh(),
    current.source_sampling_vertices(),limits(options,current.mesh().positions.size(),current.source_sampling_authored_uv1()?current.source_sampling_authored_uv1()->size():0));
  if(!same(*expected,maps(current)))throw std::invalid_argument("Source Bake differs from exact original geometry/role transport");
  incoming.validate(current.resolution());
  if(incoming.normal_interpretation!=BakedNormalInterpretation::CanonicalMikk||incoming.normal_authority||incoming.normal_field||
     incoming.color_interpretation!=BakedColorInterpretation::VertexComponent)
    throw std::invalid_argument("Source Bake requires its explicit current canonical normal/component color interpretation");
  auto future=footprint(current);const size_t old=current.baked_?current.baked_->storage_bytes():0;
  // Forecast both sparse/old and incoming payloads, immutable proof/raster copy,
  // merge copy and independent FLOAT coefficient scratch before allocation.
  const size_t scratch=incoming.storage_bytes()+old+
    ((current.baked_&&(incoming.emission_field||current.baked_->emission_field))?
      2*BakedEmissionField::required_bytes(current.resolution()):0);
  future.tiles+=incoming.storage_bytes();const auto prior=footprint(*before);
  const size_t remap=current.source_sampling_vertices().size()*sizeof(uint32_t),
    authored_bytes=current.source_sampling_authored_uv1()?current.source_sampling_authored_uv1()->size()*sizeof(std::array<float,2>):0;
  const auto working=working_bytes(prior,future,remap,authored_bytes);
  const auto copy=surface_copy_bytes(current);
  if(working>options.max_working_bytes||scratch>options.max_working_bytes-working||copy>options.max_working_bytes-working-scratch||
     retained_bytes(prior,future,remap,authored_bytes)>128*1024*1024||
     current.storage_bytes()+incoming.storage_bytes()>128*1024*1024)
    throw std::length_error("Source Bake exceeds prospective retained/merge/replay working budgets");
  auto result=current.snapshot();merge_bake(*result,std::move(incoming),BakedNormalWrite::Replace,options.cancelled);stop();
  auto surface=std::make_shared<PaintedMesh::SurfaceMap>(*current.surface_);
  auto proof=std::make_shared<PaintedMesh::SurfaceMap::OriginalSampling>(*current.surface_->original_sampling);
  proof->baked=result->baked_;surface->original_sampling=std::move(proof);result->surface_=std::move(surface);
  if(!result->valid_source_sampling_attachment())throw std::logic_error("Source Bake failed its immutable current owner pairing");
  stop();return result;
}
std::shared_ptr<PaintedMesh> detail::install_source_bake(const PaintedMesh &current,BakedTextures incoming,
    const SourcePaintOptions &options){return SourcePaintAccess::install(current,std::move(incoming),options);}
void SourcePaintAccess::clear(PaintedMesh &current){
  if(current.editing_||!current.valid_source_sampling_attachment())throw std::logic_error("Source Bake clear requires its exact finished current owner");
  const auto working=working_bytes(current);
  if(working>SourcePaintOptions::maximum_working_bytes||surface_copy_bytes(current)>SourcePaintOptions::maximum_working_bytes-working)
    throw std::length_error("Source Bake clear exceeds immutable proof copy budget");
  auto surface=std::make_shared<PaintedMesh::SurfaceMap>(*current.surface_);
  auto proof=std::make_shared<PaintedMesh::SurfaceMap::OriginalSampling>(*current.surface_->original_sampling);
  proof->baked.reset();surface->original_sampling=std::move(proof);
  const auto token=next_composite_token();current.surface_=std::move(surface);current.baked_.reset();current.composite_token_=token;++current.revision_;
}

void PaintedMesh::clear_bake() {
  if(source_sampling_prepared()&&baked_){SourcePaintAccess::clear(*this);return;}
  if (editing_) throw std::logic_error("Finish the paint stroke before clearing a bake");
  if (baked_) { auto token = next_composite_token(); baked_.reset(); composite_token_ = token; ++revision_; }
}
std::shared_ptr<PaintedMesh> PaintedMesh::from_mesh(Mesh mesh,uint32_t resolution,const PaintSurfaceOptions &options) {
  if(options.policy!=PaintUvPolicy::Strict)throw std::invalid_argument("Untextured source-authored UV persistence requires explicit original-source authority");
  return std::shared_ptr<PaintedMesh>(new PaintedMesh(std::move(mesh),resolution,options));
}
std::shared_ptr<PaintedMesh> PaintedMesh::from_maps(Mesh mesh,
    std::shared_ptr<const PaintSourceMaps> maps,uint32_t resolution) {
  return from_maps(std::move(mesh),std::move(maps),resolution,{});
}
std::shared_ptr<PaintedMesh> PaintedMesh::from_maps(Mesh mesh,
    std::shared_ptr<const PaintSourceMaps> maps,uint32_t resolution,const PaintSurfaceOptions &options) {
  if(!maps || maps->empty())throw std::invalid_argument("Original paint maps are empty");
  maps->validate_surface(mesh);
  auto result=std::shared_ptr<PaintedMesh>(new PaintedMesh(std::move(mesh),resolution,options));
  // Freeze the small bundle of references/metadata. The shared image pixels must
  // already be immutable, allowing one original image cache across materials.
  result->source_maps_=std::make_shared<const PaintSourceMaps>(*maps);
  return result;
}
std::shared_ptr<const TextureMaps> PaintedMesh::preview(const Material &material) const {return preview(material,{});}
std::shared_ptr<const TextureMaps> PaintedMesh::preview(const Material &material,const std::function<bool()> &cancelled) const {
  paint_compute::checkpoint(cancelled);material.validate();
  if(surface_->paint_domain&&surface_->paint_domain->version()==3)throw std::invalid_argument("Polygon surface preview/export requires its actual current field adapter");
  if(source_sampling_prepared()&&baked_)return detail::PaintOverlayFactory::capture_source_baked(*this,cancelled);
  if(detail::EmissionExportAccess::requires_preview(*this,material,nullptr))
    return detail::EmissionExportAccess::preview(*this,material,cancelled);
  if(baked_&&baked_->normal_authority&&(!normal_projection_||normal_projection_->version!=2))
    throw std::invalid_argument("Retained normal identity requires its own source sampling interpretation");
  if(source_maps_ && !baked_ && !color_projection_ && !normal_projection_){
    std::lock_guard lock(composite_cache_->mutex);
    if(composite_cache_->preview_token!=composite_token_){
      bool contributing=false;
      for(const auto &l:layers_)if(l.visible&&l.opacity>0){
        for(const auto &tile:l.tiles)if(tile){
          for(const auto &p:tile->pixels)if(p.data[13]&&(p.data[9]||p.data[10]||p.data[11]||p.data[12])){contributing=true;break;}
          if(contributing)break;
        }
        if(contributing)break;
      }
      if(source_sampling_prepared()&&!contributing)for(const auto &l:layers_)if(l.visible&&l.opacity>0)
        for(const auto &tile:l.emission_tiles)if(tile)for(const auto &p:tile->pixels)if(p.coverage>0){contributing=true;break;}
      if(source_sampling_prepared())composite_cache_->source_preview.reset();
      composite_cache_->source_eligible=!contributing;composite_cache_->preview_token=composite_token_;
    }
    if(composite_cache_->source_eligible||source_sampling_prepared()){
      if(!valid_source_sampling_attachment())throw std::invalid_argument("Source paint preview lost its actual immutable attachment");
      auto preview=composite_cache_->source_preview.lock();
      if(!preview){
        auto marker=std::make_shared<TextureMaps>();marker->source_maps=source_maps_;marker->source_only=true;
        if(source_sampling_prepared()){
          marker->overlay=detail::PaintOverlayFactory::capture(*this,cancelled);
          for(const auto &l:layers_)if(l.visible&&l.opacity>0)for(const auto &tile:l.tiles)if(tile)
            for(const auto &p:tile->pixels)if(p.data[13]&&p.data[9]&&p.data[3]!=255){marker->transparent=true;break;}
        }
        if(auto image=source_maps_->color.image)for(size_t i=3;i<image->rgba.size();i+=4)if(image->rgba[i]!=255){marker->transparent=true;break;}
        composite_cache_->source_preview=marker;preview=std::move(marker);
      }
      return preview;
    }
  }
  return composite(material,cancelled);
}
PaintColorProjectionResult SourceColorPreparationAccess::project(
    const PaintedMesh &source,std::span<const std::array<float,4>> colors,
    const ColorPreparationOptions &options) {
  const auto check=[&]{if(options.cancelled && options.cancelled())throw std::runtime_error("Color projection cancelled");};
  check();
  if(source.in_stroke() || colors.size()!=source.mesh().positions.size())
    throw std::invalid_argument("Color projection requires immutable paint and complete source color identities");
  for(size_t v=0;v<colors.size();++v){if((v&4095)==0)check();for(float c:colors[v])if(!std::isfinite(c)||c<0||c>1)throw std::invalid_argument("Color projection requires finite unit RGBA");}
  const auto &diagnostics=source.surface_->diagnostics;
  if(diagnostics.additional_candidates || !diagnostics.unpaintable_uv_faces.empty())
    throw std::invalid_argument("Prepare a nonoverlapping writable UV atlas for all colored faces before baking");
  if(!std::isfinite(options.max_projection_error)||options.max_projection_error<0||options.max_projection_error>1)
    throw std::invalid_argument("Invalid color projection quality threshold");
  const uint32_t r=source.resolution();const size_t projection_bytes=size_t(r)*r*4;
  if(projection_bytes>options.max_working_bytes || projection_bytes>options.max_layer_bytes)
    throw std::length_error("Color projection exceeds its declared memory budget");
  auto vertex_image=std::make_shared<Image>(Image{r,r,std::vector<uint8_t>(projection_bytes)});
  PaintColorProjectionResult result;
  std::array<float,4> maximum_error{};
  for(size_t at=0;at<source.surface_->texels.size();++at) {
    if((at&4095)==0)check();
    const auto &texel=source.surface_->texels[at];if(!texel.covered)continue;
    const float bary[]{texel.bary[0],texel.bary[1],1-texel.bary[0]-texel.bary[1]};
    for(size_t component=0;component<4;++component) {
      float value=0;for(size_t k=0;k<3;++k)value+=colors[source.mesh().indices[size_t(texel.face)*3+k]][component]*bary[k];
      vertex_image->rgba[at*4+component]=byte(value);
    }
    ++result.projected_texels;
  }
  for(size_t at=0;at<source.surface_->texels.size();++at) {
    if((at&4095)==0)check();
    if(source.surface_->texels[at].covered)continue;
    auto owner=source.surface_->padding[at];if(owner>=0)std::copy_n(vertex_image->rgba.begin()+size_t(owner)*4,4,vertex_image->rgba.begin()+at*4);
  }
  const auto bound=detail::color_projection_bound(source.mesh(),colors,*vertex_image,options.max_quality_cells,options.cancelled);
  for(size_t c=0;c<4;++c) {
    maximum_error[c]=std::min(1.f,std::nextafter(float(bound.maximum_error[c]),INFINITY));
    if(maximum_error[c]>options.max_projection_error)
      throw std::invalid_argument("Vertex-color projection exceeds its declared error bound; prepare a higher-resolution atlas with separated color seams");
  }
  auto projection=std::make_shared<PaintColorProjection>();projection->vertex_rgba=vertex_image;
  projection->maximum_error=maximum_error;projection->quality_cells=bound.tested_cells;projection->validate(r);
  result.projection=std::move(projection);check();return result;
}
PaintColorProjectionResult SourceColorPreparationAccess::project_uv(
    const PaintedMesh &before,const Mesh &target,
    std::span<const std::array<float,4>> colors,
    const ColorPreparationOptions &options,uint64_t max_raster_samples) {
  if(!max_raster_samples||max_raster_samples>512'000'000)
    throw std::invalid_argument("Invalid current-colour UV raster budget");
  const size_t pixels=size_t(before.resolution())*before.resolution();
  const size_t geometry=target.positions.size()*(2*sizeof(Vec3)+sizeof(std::array<float,2>))+target.indices.size()*4+target.uv_pins.size()+target.uv_seams.size();
  const size_t working=pixels*(sizeof(PaintedMesh::SurfaceMap::Texel)+sizeof(int32_t)+4)+geometry+
      pixels/(PaintTile::size*PaintTile::size)*sizeof(PaintedMesh::SurfaceMap::Bounds);
  if(working>options.max_working_bytes)
    throw std::length_error("Current-colour UV projection exceeds its declared raster working budget");
  PaintSurfaceOptions surface;surface.cancelled=options.cancelled;surface.max_raster_samples=max_raster_samples;
  auto atlas=std::make_shared<PaintedMesh::SurfaceMap>(target,before.resolution(),surface);
  auto canvas=before.snapshot();canvas->surface_=std::move(atlas);
  auto result=project(*canvas,colors,options);result.raster_samples=canvas->surface_->diagnostics.raster_samples;
  return result;
}
PaintColorPreparationResult SourceColorPreparationAccess::prepare(
    const PaintedMesh &source,std::span<const std::array<float,4>> colors,
    const Material &material,const ColorPreparationOptions &options,SourceColorField source_field,
    std::shared_ptr<const PaintColorProjection> retained_projection) {
  const auto check=[&]{if(options.cancelled && options.cancelled())throw std::runtime_error("Color preparation cancelled");};
  check();material.validate();
  if(source.in_stroke() || source.color_projection_ || colors.size()!=source.mesh().positions.size())
    throw std::invalid_argument("Color preparation requires an immutable unprepared source and complete color identities");
  const auto &diagnostics=source.surface_->diagnostics;
  if(diagnostics.additional_candidates || !diagnostics.unpaintable_uv_faces.empty())
    throw std::invalid_argument("Prepare a nonoverlapping writable UV atlas for all colored faces before baking");
  const uint32_t r=source.resolution();
  const size_t projection_bytes=size_t(r)*r*4;
  const size_t layer_bytes=source.allocated_tiles_*sizeof(PaintColorTile);
  const size_t baked_bytes=source.baked_?source.baked_->maps.color.rgba.size():0;
  const size_t storage=source.storage_bytes()+projection_bytes+layer_bytes+baked_bytes;
  if(storage>options.max_layer_bytes)throw std::length_error("Color preparation exceeds painted storage budget");
  auto dimensions=[&](std::initializer_list<const PaintSourceTexture*> views) {
    size_t width=r,height=r;for(auto view:views)if(view && view->image){width=std::max(width,size_t(view->image->width));height=std::max(height,size_t(view->image->height));}
    if(width*height>16*1024*1024)throw std::length_error("Prepared PBR image exceeds pixel budget");
    return width*height*4;
  };
  const PaintSourceMaps empty;const auto &maps=source.source_maps_?*source.source_maps_:empty;
  const auto color_bytes=dimensions({&maps.color});
  size_t composite_bytes=color_bytes+dimensions({&maps.metal_rough,&maps.occlusion})+dimensions({&maps.emission});
  // Conservative preflight includes full color/emission coverage grids, both
  // before/after composites and the copied old baked payload before allocation.
  composite_bytes+=color_bytes+dimensions({&maps.emission});
  if(source.baked_ && !source.baked_->maps.normal.rgba.empty())composite_bytes+=size_t(r)*r*12;
  const size_t working=2*composite_bytes+color_bytes+projection_bytes+layer_bytes+baked_bytes+(source.baked_?source.baked_->storage_bytes():0);
  if(working>options.max_working_bytes)throw std::length_error("Color preparation exceeds bounded composite working memory");
  PaintColorProjectionResult projected;
  if(retained_projection){retained_projection->validate(r);projected.projection=std::move(retained_projection);projected.projected_texels=source.surface_->diagnostics.covered_texels;}
  else projected=project(source,colors,options);
  auto projection=projected.projection;auto vertex_image=projection->vertex_rgba;
  ColorPreparationReceipt receipt;receipt.resolution=r;receipt.projected_texels=projected.projected_texels;
  receipt.accepted_projection_error=options.max_projection_error;
  receipt.source_field=source_field;
  receipt.version=source_field==SourceColorField::PolygonCorners?2:1;
  receipt.quality_material={material.color,material.emissive,int(material.alpha),material.alpha_cutoff,material.double_sided};
  receipt.projection_error_bound=projection->maximum_error;receipt.tested_quality_cells=projection->quality_cells;
  auto result=source.snapshot();result->color_projection_=projection;result->allocated_color_tiles_=0;
  for(auto &layer:result->layers_) {
    check();layer.pbr_tiles.assign(layer.tiles.size(),{});
    for(size_t tile=0;tile<layer.tiles.size();++tile)if(layer.tiles[tile]) {
      check();auto output=std::make_shared<PaintColorTile>();
      const size_t tx=tile%source.surface_->columns*32,ty=tile/source.surface_->columns*32;
      for(size_t y=0;y<32;++y)for(size_t x=0;x<32;++x) {
        const auto &p=layer.tiles[tile]->pixels[y*32+x];const size_t at=(ty+y)*r+tx+x;
        for(size_t c=0;c<4;++c) {
          float value=unit(p.data[c]);if(c<3)value=srgb_to_linear(value);
          value*=unit(vertex_image->rgba[at*4+c]);output->pixels[y*32+x][c]=byte(c<3?linear_to_srgb(value):value);
        }
      }
      layer.pbr_tiles[tile]=std::move(output);++result->allocated_color_tiles_;
    }
  }
  if(source.baked_) {
    auto baked=std::make_shared<BakedTextures>(*source.baked_);
    if(baked->color_interpretation!=BakedColorInterpretation::VertexComponent)
      throw std::invalid_argument("Unprepared target contains an incompatible resolved bake");
    baked->maps.pbr_color=blank_image(r);
    for(size_t at=0;at<size_t(r)*r;++at) {
      if((at&4095)==0)check();
      for(size_t c=0;c<4;++c) {
        float value=unit(baked->maps.color.rgba[at*4+c]);if(c<3)value=srgb_to_linear(value);
        value*=unit(vertex_image->rgba[at*4+c]);baked->maps.pbr_color.rgba[at*4+c]=byte(c<3?linear_to_srgb(value):value);
      }
    }
    baked->color_interpretation=BakedColorInterpretation::ResolvedPbr;baked->validate(r);result->baked_=std::move(baked);
  }
  result->composite_token_=next_composite_token();++result->revision_;
  auto before=source.preview(material,options.cancelled);
  // Preparation explicitly needs the actual colour pixels for its quality
  // proof. A render-only current-grid marker is not that image authority.
  if(before->current_grid_preview)before=source.composite(material,options.cancelled);
  // Only colour is being prepared. Retain original FLOAT emission owners and
  // editable/hidden layers in result; stage emission-free OTHER channels for
  // the existing colour quality check instead of quantizing physical energy.
  const auto after=(result->has_physical_emission()||detail::hdr_emission(material.emissive))
      ?detail::EmissionExportAccess::other_channels(*result,material,options.cancelled)
      :result->composite(material,options.cancelled);
  if(!before->source_only && before->color.rgba!=after->color.rgba)
    throw std::logic_error("Color preparation changed the common MToon paint authority");
  auto view=[](const std::shared_ptr<const TextureMaps> &owner,const Image *image) {
    PaintSourceTexture output;output.image=std::shared_ptr<const Image>(owner,image);output.wrap_s=output.wrap_t=33071;output.min_filter=output.mag_filter=9729;return output;
  };
  const auto old_view=before->source_only?before->source_maps->color:view(before,&before->color);
  const auto new_view=view(after,&after->pbr_color);
  auto probe=[&](uint32_t face,std::array<float,3> bary) {
    if(++receipt.probe_samples>options.max_probe_samples)throw std::length_error("Color preparation quality probe budget exceeded");
    if((receipt.probe_samples&4095)==0)check();
    std::array<float,2> uv{};std::array<float,4> vertex{};
    for(size_t k=0;k<3;++k){auto id=source.mesh().indices[size_t(face)*3+k];for(size_t c=0;c<2;++c)uv[c]+=source.mesh().uv[id][c]*bary[k];for(size_t c=0;c<4;++c)vertex[c]+=colors[id][c]*bary[k];}
    auto a=sample_source_texture(old_view,uv[0],uv[1],true),b=sample_source_texture(new_view,uv[0],uv[1],true);
    for(size_t c=0;c<4;++c){if(before->source_only)a[c]*=material.color[c];a[c]*=vertex[c];receipt.observed_base_error[c]=std::max(receipt.observed_base_error[c],std::min(1.f,std::abs(a[c]-b[c])));}
    if(material.alpha==Material::AlphaMode::Mask && (a[3]>=material.alpha_cutoff)!=(b[3]>=material.alpha_cutoff))++receipt.observed_mask_disagreements;
  };
  for(const auto &texel:source.surface_->texels)if(texel.covered)probe(texel.face,{texel.bary[0],texel.bary[1],1-texel.bary[0]-texel.bary[1]});
  for(size_t face=0;face<source.mesh().indices.size()/3;++face) {
    check();probe(uint32_t(face),{1.f/3,1.f/3,1.f/3});for(size_t corner=0;corner<3;++corner){std::array<float,3> bary{};bary[corner]=1;probe(uint32_t(face),bary);}
  }
  receipt.validate();check();return {std::move(result),receipt};
}
std::shared_ptr<const TextureMaps>
PaintedMesh::composite(const Material &material) const {return composite(material,{});}
std::shared_ptr<const TextureMaps>
PaintedMesh::composite(const Material &material,const std::function<bool()> &cancelled) const {
  paint_compute::checkpoint(cancelled);material.validate();
  if(surface_->paint_domain&&surface_->paint_domain->version()==3)throw std::invalid_argument("Polygon surface preview/export requires its actual current field adapter");
  if(surface_->source_capture_only) {
    if(!source_maps_||baked_||color_projection_||normal_projection_||
       std::any_of(layers_.begin(),layers_.end(),[](const auto &layer){return tile_count(layer)||color_tile_count(layer);}))
      throw std::invalid_argument("Initial original-coordinate capture cannot contain writable paint data");
    return preview(material,cancelled);
  }
  CompositeFactors factors{material.color,material.emissive,material.metallic,material.roughness};
  std::lock_guard lock(composite_cache_->mutex);paint_compute::checkpoint(cancelled);
  if(composite_cache_->token==composite_token_&&composite_cache_->factors==factors)
    if(auto maps=composite_cache_->maps.lock()){composite_retention().retain(maps,composite_token_,factors,true);return maps;}
  auto maps=composite_retention().find(composite_token_,factors);
  if(maps){composite_cache_->token=composite_token_;composite_cache_->factors=factors;composite_cache_->maps=maps;return maps;}
  maps=build_composite(material,cancelled);
  composite_cache_->token=composite_token_;composite_cache_->factors=factors;composite_cache_->maps=maps;
  composite_retention().retain(maps,composite_token_,factors,false);return maps;
}
namespace {
bool gpu_plain_composite_eligible(uint32_t r,const std::vector<PaintLayer> &layers){
 if(compute_backend()=="CPU"||r<512||r>2048)return false;
 size_t contributing=0;
 for(const auto &l:layers){
  if(std::any_of(l.pbr_tiles.begin(),l.pbr_tiles.end(),[](const auto &p){return bool(p);}))return false;
  if(l.visible&&l.opacity>0&&std::any_of(l.tiles.begin(),l.tiles.end(),[](const auto &p){return bool(p);}))++contributing;
 }
 return contributing&&uint64_t(r)*r*contributing<=128*1024*1024;
}
std::shared_ptr<TextureMaps> gpu_plain_composite(uint32_t r,const std::vector<PaintLayer> &layers,
    const std::vector<int32_t> &padding,const Material &material,const std::function<bool()> &cancelled){
 using namespace paint_compute;checkpoint(cancelled);Input in;auto &p=in.parameters;
 p.resolution=r;p.tiles=((r+31)/32)*((r+31)/32);std::copy(material.color.begin(),material.color.end(),p.color);std::copy(material.emissive.begin(),material.emissive.end(),p.emission);p.metallic=material.metallic;p.roughness=material.roughness;
 bool color_coverage=false,emission_coverage=false;
 for(const auto &l:layers)if(l.visible&&l.opacity>0&&std::any_of(l.tiles.begin(),l.tiles.end(),[](const auto &p){return bool(p);})){
  checkpoint(cancelled);in.layers.push_back({l.opacity,uint32_t(in.tiles.size())});
  for(const auto &tile:l.tiles){
   checkpoint(cancelled);if(!tile){in.tiles.push_back(absent);continue;}
   if(in.pixels.size()>maximum_tile_bytes/sizeof(Pixel)-1024)throw std::length_error("Paint composite packed tile budget exceeded");
   in.tiles.push_back(uint32_t(in.pixels.size()/1024));const auto first=in.pixels.size();in.pixels.resize(first+1024);
   for(size_t i=0;i<1024;++i){const auto &v=tile->pixels[i].data;std::copy(v.begin(),v.end(),in.pixels[first+i].data);color_coverage|=v[13]&&v[9];emission_coverage|=v[13]&&v[12];}
  }
 }
 p.layers=uint32_t(in.layers.size());in.padding=padding;checkpoint(cancelled);
 // No model/cache/renderer memory is passed to the backend. All readbacks
 // complete before any output image is constructed or exposed.
 const auto pixels=paint_compute::composite(in,cancelled);auto maps=std::make_shared<TextureMaps>();
 for(auto image:{&maps->color,&maps->metal_rough,&maps->emission})*image={r,r,std::vector<uint8_t>(size_t(r)*r*4)};
 auto coverage=[&](Image &im,bool full){const uint32_t n=full?r:1;im={n,n,std::vector<uint8_t>(size_t(n)*n*4)};for(size_t at=3;at<im.rgba.size();at+=4)im.rgba[at]=255;};
 coverage(maps->color_coverage,color_coverage);coverage(maps->emission_coverage,emission_coverage);
 for(size_t i=0;i<pixels.size();++i){
  if((i&4095)==0)checkpoint(cancelled);const auto &v=pixels[i];if(v.reserved||v.transparent>1)throw std::runtime_error("Paint composite backend returned invalid metadata");
  std::copy_n(v.color,4,maps->color.rgba.data()+i*4);std::copy_n(v.orm,4,maps->metal_rough.rgba.data()+i*4);std::copy_n(v.emission,4,maps->emission.rgba.data()+i*4);maps->transparent|=v.transparent;
  if(color_coverage)maps->color_coverage.rgba[i*4]=v.color_coverage;if(emission_coverage)maps->emission_coverage.rgba[i*4]=v.emission_coverage;
 }
 checkpoint(cancelled);return maps;
}
}
std::shared_ptr<TextureMaps>
PaintedMesh::build_composite(const Material &material) const {return build_composite(material,{});}
namespace {
std::vector<uint16_t> composite_tile_layers(const std::vector<PaintLayer> &layers,size_t tiles) {
  if(layers.size()>16)throw std::invalid_argument("Composite exceeds sixteen ordered paint layers");
  std::vector<uint16_t> masks(tiles);
  for(size_t i=0;i<layers.size();++i){
    const auto &layer=layers[i];
    if(!layer.visible||!(layer.opacity>0))continue;
    for(size_t tile=0;tile<tiles;++tile)
      if(layer.tiles[tile])masks[tile]|=uint16_t(1u<<i);
  }
  return masks;
}
size_t composite_tile(uint32_t x,uint32_t y,uint32_t columns) noexcept {
  return size_t(y/PaintTile::size)*columns+x/PaintTile::size;
}
}
size_t SourcePaintSamplingAccess::composite_work(const PaintedMesh &paint,detail::NativeGridComposite channels,size_t maximum,
    const std::function<bool()> &cancelled){
  const auto r=paint.resolution();
  const auto &surface=*paint.surface_;
  if(!surface.paint_domain||surface.options.policy!=PaintUvPolicy::Strict||
     paint.source_maps_||paint.color_projection_||paint.normal_projection_||
     (channels==detail::NativeGridComposite::Full&&paint.has_physical_emission())||(paint.baked_&&paint.baked_->normal_field))
    throw std::invalid_argument("Native composite bound requires the actual plain retained grid");
  const size_t pixels=size_t(r)*r,tiles=surface.tiles.size();
  size_t work=0;
  auto charge=[&](size_t count,size_t unit){
    if(count>(maximum-work)/unit)
      throw std::length_error("Native nonemission compilation exceeds cumulative sample budget");
    work+=count*unit;
  };
  // Per channel allow padding, bake RGBA/coverage and output/coverage
  // transfers. Also charge the preflight padding walk and the two coverage
  // scans. No source sampler, projection or bilinear taps enter this route.
  charge(pixels,3*8+1+2);
  charge(tiles,std::max<size_t>(1,paint.layers_.size()));
  if(paint.baked_&&!paint.baked_->maps.normal.rgba.empty())charge(pixels,8);
  if(channels==detail::NativeGridComposite::WithoutEmission){
    // The temporary emission-free composite walks the same common tiles and
    // can copy any occupied tile while clearing its legacy emission bytes.
    for(const auto &layer:paint.layers_){
      paint_compute::checkpoint(cancelled);
      for(const auto &tile:layer.tiles)
        if(tile)charge(PaintTile::size*PaintTile::size,2);
    }
    charge(tiles,std::max<size_t>(1,paint.layers_.size()));
  }
  const auto masks=composite_tile_layers(paint.layers_,tiles);
  size_t populated=0;
  for(auto mask:masks)populated+=std::popcount(mask);
  // Both colour/emission coverage scans may visit every pixel of each
  // occupied visible tile, even for a grid with sparse or no Bake coverage.
  charge(populated,2*PaintTile::size*PaintTile::size);
  for(size_t at=0;at<pixels;++at){
    if((at&4095)==0)paint_compute::checkpoint(cancelled);
    const int32_t owner=surface.padding[at];
    if(owner<0)continue;
    const auto mask=masks[composite_tile(uint32_t(owner)%r,uint32_t(owner)/r,surface.columns)];
    // Three channels, each reading the ordered tile once and applying its
    // mask/coverage to that channel. Padding can reference one tile repeatedly.
    charge(size_t(std::popcount(mask)),3*4);
  }
  return work;
}
size_t detail::native_grid_composite_work(const PaintedMesh &paint,NativeGridComposite channels,size_t maximum,
    const std::function<bool()> &cancelled){
  return SourcePaintSamplingAccess::composite_work(paint,channels,maximum,cancelled);
}
std::shared_ptr<TextureMaps>
PaintedMesh::build_composite(const Material &material,const std::function<bool()> &cancelled) const {
  paint_compute::checkpoint(cancelled);
  if(has_physical_emission()||std::any_of(material.emissive.begin(),material.emissive.end(),[](float v){return v>1;}))throw std::logic_error("Physical emission requires its linear composite adapter");
  // This packet owns only the legacy grid/padding. Independent paint domains,
  // normal metadata/events, actual original images and prepared projections need
  // their existing CPU interpretation until a genuine packet adapter exists.
  if(!surface_->paint_domain&&!sampling_attachment_&&(!source_maps_||source_maps_->empty())&&!baked_&&!color_projection_&&!normal_projection_&&gpu_plain_composite_eligible(resolution(),layers_))
    return gpu_plain_composite(resolution(),layers_,surface_->padding,material,cancelled);
  if(source_maps_&&source_maps_->uses_alternate_uv())throw std::invalid_argument(source_maps_->extra_uv_sets.empty()?"UV1 source compositing requires explicit source-coordinate preparation":"Extra source UV compositing requires explicit source-coordinate preparation");
  auto maps = std::make_shared<TextureMaps>();
  maps->source_maps=source_maps_;
  maps->color_projection=color_projection_;
  maps->normal_projection=normal_projection_;
  uint32_t r=resolution();
  const PaintSourceMaps empty;
  const auto &source=source_maps_?*source_maps_:empty;
  auto dimensions=[&](Image &image,std::initializer_list<const PaintSourceTexture*> views){
    image.width=image.height=r;
    for(auto view:views)if(view->image){image.width=std::max(image.width,view->image->width);image.height=std::max(image.height,view->image->height);}
    if(size_t(image.width)*image.height>16*1024*1024)
      throw std::length_error("Composited original texture exceeds output pixel budget");
    image.rgba.resize(size_t(image.width)*image.height*4);
  };
  dimensions(maps->color,{&source.color});
  PaintSourceTexture projected_vertex_color;
  if(color_projection_) {
    color_projection_->validate(r);
    dimensions(maps->pbr_color,{&source.color});
    projected_vertex_color.image=color_projection_->vertex_rgba;
    projected_vertex_color.wrap_s=projected_vertex_color.wrap_t=33071;
    projected_vertex_color.min_filter=projected_vertex_color.mag_filter=9729;
  }
  dimensions(maps->metal_rough,{&source.metal_rough,&source.occlusion});
  dimensions(maps->emission,{&source.emission});
  auto make_coverage=[&](Image &coverage,const Image &base,int component) {
    bool covered=baked_&&std::any_of(baked_->coverage.begin(),baked_->coverage.end(),[](uint8_t x){return x!=0;});
    for(const auto &layer:layers_)if(layer.visible&&layer.opacity>0) {
      for(const auto &tile:layer.tiles)if(tile) {
        for(const auto &pixel:tile->pixels)if(pixel.data[13]&&pixel.data[component]){covered=true;break;}
        if(covered)break;
      }
      if(covered)break;
    }
    coverage.width=covered?base.width:1;coverage.height=covered?base.height:1;
    coverage.rgba.assign(size_t(coverage.width)*coverage.height*4,0);
    for(size_t at=3;at<coverage.rgba.size();at+=4)coverage.rgba[at]=255;
  };
  make_coverage(maps->color_coverage,maps->color,9);
  make_coverage(maps->emission_coverage,maps->emission,12);
  maps->baked_base=baked_;
  if(baked_&&baked_->normal_field){
    baked_->normal_field->validate();maps->normal=*baked_->normal_field->pbr();maps->mtoon_normal=*baked_->normal_field->mtoon();
  }else if(normal_projection_) {
    normal_projection_->validate(r);
    auto field=normal_projection_->version==2?SourceNormalPreparationAccess::resolved(*this):normal_projection_;
    maps->normal=*field->pbr;maps->mtoon_normal=*field->mtoon;
  } else if (baked_ && !baked_->maps.normal.rgba.empty()) {
    maps->normal=blank_image(r,true);maps->baked_normal=blank_image(r,true);
    maps->baked_coverage=blank_image(r);for(size_t at=3;at<maps->baked_coverage.rgba.size();at+=4)maps->baked_coverage.rgba[at]=255;
  }

  // These values are decoded directly from byte images and sparse layer
  // tiles. Original samplers still interpolate in their existing domain.
  static const auto linear_byte = [] {
    std::array<float, 256> table{};
    for (size_t value = 0; value < table.size(); ++value)
      table[value] = srgb_to_linear(unit(uint8_t(value)));
    return table;
  }();
  const detail::SourceTextureGrid occlusion(source.occlusion,maps->metal_rough.width,maps->metal_rough.height,false);
  const detail::SourceTextureGrid projected(projected_vertex_color,maps->color.width,maps->color.height,false);
  const auto tile_layers=composite_tile_layers(layers_,surface_->tiles.size());
  for(int channel=0;channel<3;++channel){
    auto &image=channel==0?maps->color:channel==1?maps->metal_rough:maps->emission;
    const auto &source_view=channel==0?source.color:channel==1?source.metal_rough:source.emission;
    const detail::SourceTextureGrid source_grid(source_view,image.width,image.height,channel!=1);
    const Image *baked_image = nullptr;
    if (baked_)
      baked_image = channel == 0 ? &baked_->maps.color :
                    channel == 1 ? &baked_->maps.metal_rough :
                    baked_->emission_field ? nullptr : &baked_->maps.emission;
    for(uint32_t y=0;y<image.height;++y)for(uint32_t x=0;x<image.width;++x){
      if((x&255)==0)paint_compute::checkpoint(cancelled);
      float u=(float(x)+.5f)/image.width,v=(float(y)+.5f)/image.height;
      const auto sampled=source_grid.sample(x,y);
      std::array<float,4> value{};
      if(channel==0)for(int k=0;k<4;++k)value[k]=material.color[k]*sampled[k];
      else if(channel==1){
        const float ao=occlusion.sample(x,y)[0];
        value={1-source.occlusion_strength*(1-ao),material.roughness*sampled[1],material.metallic*sampled[2],1};
      }else{for(int k=0;k<3;++k)value[k]=material.emissive[k]*sampled[k];value[3]=1;}
      auto pbr_value=value;
      if(channel==0 && color_projection_) {
        const auto vertex=projected.sample(x,y);
        for(int k=0;k<4;++k)pbr_value[k]*=vertex[k];
      }
      uint32_t tx=std::min(r-1,uint32_t(u*r)),ty=std::min(r-1,uint32_t(v*r));
      int32_t owner=surface_->padding[size_t(ty)*r+tx];
      uint32_t px=owner>=0?uint32_t(owner)%r:tx,py=owner>=0?uint32_t(owner)/r:ty;
      float replacement_coverage=0;
      if (baked_ && owner >= 0) {
        const auto texel = size_t(owner) * 4;
        const float a = unit(baked_->coverage[size_t(owner)]);
        replacement_coverage = a;
        for (int k = 0; k < (channel == 0 ? 4 : 3); ++k) {
          const uint8_t sample = baked_image ? baked_image->rgba[texel + k] : 0;
          const float decoded = channel == 1 || k == 3 ? unit(sample) : linear_byte[sample];
          value[k] = value[k] * (1 - a) + decoded * a;
        }
        if(channel==0 && color_projection_) {
          if(baked_->color_interpretation!=BakedColorInterpretation::ResolvedPbr)throw std::logic_error("Prepared surface contains unresolved baked colors");
          for(int k=0;k<4;++k) {
            const auto sample = baked_->maps.pbr_color.rgba[texel + k];
            const float decoded = k < 3 ? linear_byte[sample] : unit(sample);
            pbr_value[k] = pbr_value[k] * (1 - a) + decoded * a;
          }
        }
      }

      const auto tile=composite_tile(px,py,surface_->columns);
      const auto within=size_t(py%PaintTile::size)*PaintTile::size+px%PaintTile::size;
      for(auto remaining=owner>=0?tile_layers[tile]:uint16_t{};remaining;remaining&=remaining-1){
        const auto &l=layers_[std::countr_zero(remaining)];
        const auto &p=l.tiles[tile]->pixels[within];
        float mask=unit(p.data[13])*l.opacity;
        float weight=mask*unit(p.data[channel==2?12:9]);replacement_coverage=replacement_coverage*(1-weight)+weight;
        auto blend=[&](float base,int data,int coverage,bool srgb){
          const float a = mask * unit(p.data[coverage]);
          const auto sample = p.data[data];
          const float paint = srgb ? linear_byte[sample] : unit(sample);
          return base*(1-a)+paint*a;
        };
        if(channel==0)for(int k=0;k<4;++k)value[k]=blend(value[k],k,9,k<3);
        else if(channel==1){value[1]=blend(value[1],5,11,false);value[2]=blend(value[2],4,10,false);}
        else for(int k=0;k<3;++k)value[k]=blend(value[k],6+k,12,true);
        if(channel==0 && color_projection_) {
          std::array<uint8_t,4> painted{p.data[0],p.data[1],p.data[2],p.data[3]};
          if(!l.pbr_tiles.empty()&&l.pbr_tiles[tile])painted=l.pbr_tiles[tile]->pixels[within];
          const float a=mask*unit(p.data[9]);
          for(int k=0;k<4;++k) {
            const float sample = k < 3 ? linear_byte[painted[k]] : unit(painted[k]);
            pbr_value[k]=pbr_value[k]*(1-a)+sample*a;
          }
        }
      }
      size_t at=(size_t(y)*image.width+x)*4;
      for(int k=0;k<4;++k)image.rgba[at+k]=byte(channel!=1&&k<3?linear_to_srgb(value[k]):value[k]);
      if(channel==0){
        maps->transparent|=value[3]<.999f;
        if(color_projection_) {
          for(int k=0;k<4;++k)maps->pbr_color.rgba[at+k]=byte(k<3?linear_to_srgb(pbr_value[k]):pbr_value[k]);
          maps->pbr_transparent|=pbr_value[3]<.999f;
        }
        if(maps->color_coverage.width==image.width&&maps->color_coverage.height==image.height)maps->color_coverage.rgba[at]=byte(replacement_coverage);
      } else if(channel==2 && maps->emission_coverage.width==image.width && maps->emission_coverage.height==image.height)
        maps->emission_coverage.rgba[at]=byte(replacement_coverage);
    }
  }
  if(normal_projection_ && normal_projection_->version==1 && baked_ && !baked_->maps.normal.rgba.empty()) {
    if(baked_->normal_interpretation!=BakedNormalInterpretation::PreparedAttributes)
      throw std::logic_error("Prepared normal model cannot reinterpret historical canonical pixels");
    for(size_t at=0;at<size_t(r)*r;++at){
      const auto owner=surface_->padding[at];if(owner<0)continue;
      const float coverage=unit(baked_->coverage[size_t(owner)]);
      for(auto pair:{std::pair{&maps->normal,&baked_->maps.normal},std::pair{&maps->mtoon_normal,&baked_->maps.mtoon_normal}}){
        const size_t a=at*4,b=size_t(owner)*4;
        Vec3 original{unit(pair.first->rgba[a])*2-1,unit(pair.first->rgba[a+1])*2-1,unit(pair.first->rgba[a+2])*2-1};
        Vec3 detail{unit(pair.second->rgba[b])*2-1,unit(pair.second->rgba[b+1])*2-1,unit(pair.second->rgba[b+2])*2-1};
        auto value=original*(1-coverage)+detail*coverage;
        value=length(value)>1e-8f?normalized(value):Vec3{0,0,1};
        pair.first->rgba[a]=byte(value.x*.5f+.5f);pair.first->rgba[a+1]=byte(value.y*.5f+.5f);pair.first->rgba[a+2]=byte(value.z*.5f+.5f);pair.first->rgba[a+3]=255;
      }
    }
  } else if (!normal_projection_ && baked_ && !baked_->normal_field && !maps->normal.rgba.empty()) {
    // Protected/fractional coverage keeps original source normal detail. Convert
    // that affine-UV derivative frame into the canonical baked tangent basis.
    std::vector<Tangent> corners;
    if(source.normal.image && source.normal_scale>0){
      bool needs_original=false;
      for(size_t at=0;at<surface_->texels.size();++at)
        if(surface_->texels[at].covered && baked_->coverage[at]<255){needs_original=true;break;}
      if(needs_original)corners=tangent_corners(mesh());
    }
    for (size_t at=0;at<size_t(r)*r;++at) {
      if((at&4095)==0)paint_compute::checkpoint(cancelled);
      int32_t owner=surface_->padding[at];
      if (owner<0) continue;
      float a=unit(baked_->coverage[size_t(owner)]);
      maps->baked_coverage.rgba[at*4]=byte(a);
      for(int c=0;c<4;++c)maps->baked_normal.rgba[at*4+c]=baked_->maps.normal.rgba[size_t(owner)*4+c];
      const auto normal_detail=unpack_baked_normal(baked_->maps.normal,size_t(owner));
      if(baked_->normal_interpretation==BakedNormalInterpretation::NativeAttributes){
        const auto rgb=detail::composite_native_bake_normal(normal_detail,a);
        std::copy_n(rgb.begin(),3,maps->normal.rgba.begin()+at*4);
        continue;
      }
      Vec3 original{0,0,1};
      if(a<1 && !corners.empty()){
        const auto &texel=surface_->texels[size_t(owner)];
        std::array<float,3> bary{texel.bary[0],texel.bary[1],1-texel.bary[0]-texel.bary[1]};
        original=tangent_frame(mesh(),corners,texel.face,bary).to_tangent(
          source_texture_normal(mesh(),source,texel.face,bary));
      }
      Vec3 n=normal_detail*a+original*(1-a);
      if(length(n)<1e-8f)n={0,0,1};else n=normalized(n);
      maps->normal.rgba[at*4]=byte(n.x*.5f+.5f);
      maps->normal.rgba[at*4+1]=byte(n.y*.5f+.5f);
      maps->normal.rgba[at*4+2]=byte(n.z*.5f+.5f);
    }
  }
  paint_compute::checkpoint(cancelled);return maps;
}
std::optional<Vec3> PaintedMesh::raycast(Vec3 origin, Vec3 direction,
                                         float maximum) const {
  if (!finite(origin) || !finite(direction) || length(direction) < 1e-10f ||
      !std::isfinite(maximum) || maximum <= 0)
    throw std::invalid_argument("Invalid mesh ray");
  direction = normalized(direction);
  float nearest = maximum;
  bool found = false;
  auto &m = mesh();
  for (size_t k = 0; k < m.indices.size(); k += 3) {
    if(texture_set_domain_&&!texture_set_domain_->contains(uint32_t(k/3)))continue;
    auto a = m.positions[m.indices[k]], e1 = m.positions[m.indices[k + 1]] - a,
         e2 = m.positions[m.indices[k + 2]] - a;
    auto p = cross(direction, e2);
    float det = dot(e1, p);
    if (std::abs(det) < 1e-8f * length(e1) * length(e2))
      continue;
    float f = 1 / det;
    auto s = origin - a;
    float u = f * dot(s, p);
    if (u < 0 || u > 1)
      continue;
    auto q = cross(s, e1);
    float v = f * dot(direction, q);
    if (v < 0 || u + v > 1)
      continue;
    float t = f * dot(e2, q);
    if (t >= 0 && t <= nearest) {
      found = true;
      nearest = t;
    }
  }
  return found ? std::optional<Vec3>(origin + direction * nearest)
               : std::nullopt;
}
Vec3 PaintedMesh::normal(Vec3 point) const {
  auto &m = mesh();
  float distance = INFINITY;
  Vec3 result{0, 1, 0};
  for (size_t i = 0; i < m.positions.size(); ++i) {
    auto d = dot(m.positions[i] - point, m.positions[i] - point);
    if (d < distance) {
      distance = d;
      result = m.normals[i];
    }
  }
  return result;
}
std::shared_ptr<PaintedMesh>
PaintedMesh::reparameterize(Mesh target,
                            const std::function<bool()> &cancelled) const {
  if(texture_set_domain_)throw std::logic_error("Texture-set UV editing requires the owning face/corner remap worker");
  if(has_physical_emission())throw std::logic_error("Physical emission requires explicit linear UV transfer");
  if(paint_sampling_domain())throw std::logic_error("Retained writable paint requires explicit source-domain UV transfer");
  if (editing_)
    throw std::logic_error("Finish the paint stroke before editing UVs");
  target.validate();
  if (target.indices.size() != mesh().indices.size() ||
      target.uv.size() != target.positions.size())
    throw std::invalid_argument(
        "UV editing must preserve the triangle surface");
  bool same = true;
  for (size_t c = 0; c < target.indices.size(); ++c) {
    auto a = target.indices[c], b = mesh().indices[c];
    auto p = target.positions[a], q = mesh().positions[b],
         n = target.normals[a], o = mesh().normals[b];
    if (p.x != q.x || p.y != q.y || p.z != q.z || n.x != o.x || n.y != o.y ||
        n.z != o.z)
      throw std::invalid_argument(
          "UV editing cannot change triangle geometry or order");
    same &= target.uv[a] == mesh().uv[b];
  }
  auto result = snapshot();
  if (same) {
    if(source_maps_&&source_maps_->uses_alternate_uv()&&(target.indices!=mesh().indices||target.positions.size()!=mesh().positions.size()))throw std::invalid_argument("Source UV1 vertex remapping requires explicit source-coordinate preparation");
    auto surface = std::make_shared<SurfaceMap>(*surface_,std::make_shared<const Mesh>(std::move(target)));
    result->surface_ = std::move(surface);
    ++result->revision_;
    return result;
  }
  if(color_projection_)throw std::invalid_argument("Prepared PBR color planes require explicit source-aware UV transfer; original data was retained");
  if(normal_projection_)throw std::invalid_argument("Prepared normal planes require explicit frame-aware UV transfer; original data retained");
  if(source_maps_)
    throw std::invalid_argument("UV transfer of original source maps requires an explicit bake; original maps were retained");
  auto surface = std::make_shared<SurfaceMap>(std::move(target), resolution());
  result->surface_ = surface;
  std::shared_ptr<BakedTextures> transferred;
  std::vector<Tangent> old_frames, new_frames;
  if (baked_) {
    transferred = std::make_shared<BakedTextures>();
    transferred->maps.color = blank_image(resolution());
    transferred->maps.metal_rough = blank_image(resolution());
    transferred->maps.emission = blank_image(resolution());
    transferred->coverage.resize(size_t(resolution()) * resolution());
    if (!baked_->maps.normal.rgba.empty()) {
      transferred->maps.normal = blank_image(resolution(), true);
      old_frames = tangent_corners(mesh(), cancelled);
      new_frames = tangent_corners(surface->mesh, cancelled);
    }
    result->baked_ = transferred;
  }
  for (auto &l : result->layers_)
    l.tiles.assign(surface->tiles.size(), {});
  std::vector<std::vector<std::shared_ptr<PaintTile>>> writable(layers_.size());
  for (auto &tiles : writable)
    tiles.resize(surface->tiles.size());
  size_t allocated = transferred ? transferred->storage_bytes() : 0;
  for (uint32_t y = 0; y < resolution(); ++y) {
    if (cancelled && cancelled())
      throw std::runtime_error("UV paint transfer cancelled");
    for (uint32_t x = 0; x < resolution(); ++x) {
      auto &t = surface->texels[size_t(y) * resolution() + x];
      if (!t.covered)
        continue;
      auto f = t.face * 3;
      float w[] = {t.bary[0], t.bary[1], 1 - t.bary[0] - t.bary[1]};
      std::array<float, 2> uv{};
      for (int k = 0; k < 3; ++k)
        for (int d = 0; d < 2; ++d)
          uv[d] += mesh().uv[mesh().indices[f + k]][d] * w[k];
      float px = uv[0] * resolution() - .5f, py = uv[1] * resolution() - .5f;
      int bx = int(std::floor(px)), by = int(std::floor(py));
      float fx = px - bx, fy = py - by;
      size_t at[4];
      float weights[4];
      for (int k = 0; k < 4; ++k) {
        auto sx = std::clamp(bx + (k & 1), 0, int(resolution()) - 1),
             sy = std::clamp(by + (k >> 1), 0, int(resolution()) - 1);
        int32_t owner = surface_->padding[size_t(sy) * resolution() + sx];
        at[k] = owner >= 0 ? size_t(owner) : size_t(sy) * resolution() + sx;
        weights[k] = ((k & 1) ? fx : 1 - fx) * ((k >> 1) ? fy : 1 - fy);
      }
      if (transferred) {
        BakedPixel values{};
        float coverage = 0;
        for (int k = 0; k < 4; ++k) {
          float a = unit(baked_->coverage[at[k]]) * weights[k];
          if (!a) continue;
          auto sample = baked_pixel(*baked_, at[k]);
          coverage += a;
          for (int c = 0; c < 13; ++c) values[c] += sample[c] * a;
        }
        if (coverage > 1e-8f) {
          for (auto &value : values) value /= coverage;
          if (!old_frames.empty()) {
            std::array<float,3> bary{w[0], w[1], w[2]};
            auto old_frame = tangent_frame(mesh(), old_frames, t.face, bary);
            auto new_frame = tangent_frame(surface->mesh, new_frames, t.face, bary);
            auto normal = new_frame.to_tangent(old_frame.to_world({values[10], values[11], values[12]}));
            values[10] = normal.x; values[11] = normal.y; values[12] = normal.z;
          }
          size_t at = size_t(y)*resolution()+x;
          transferred->coverage[at] = byte(coverage);
          baked_pixel(*transferred, at, values);
        }
      }
      for (size_t li = 0; li < layers_.size(); ++li) {
        PaintPixel samples[4];
        for (int k = 0; k < 4; ++k)
          samples[k] =
              pixel(layers_[li].id, at[k] % resolution(), at[k] / resolution());
        PaintPixel p;
        for (int group = 0; group < 4; ++group) {
          int first = group == 0   ? 0
                      : group == 1 ? 4
                      : group == 2 ? 5
                                   : 6,
              count = group == 0   ? 4
                      : group == 3 ? 3
                                   : 1;
          float coverage = 0;
          for (int k = 0; k < 4; ++k)
            coverage += weights[k] * unit(samples[k].data[9 + group]);
          p.data[9 + group] = byte(coverage);
          if (coverage > 1e-8f)
            for (int c = 0; c < count; ++c) {
              bool srgb = (group == 0 && c < 3) || group == 3;
              float value = 0;
              for (int k = 0; k < 4; ++k) {
                float v = unit(samples[k].data[first + c]);
                if (srgb)
                  v = srgb_to_linear(v);
                value += weights[k] * unit(samples[k].data[9 + group]) * v;
              }
              value /= coverage;
              p.data[first + c] = byte(srgb ? linear_to_srgb(value) : value);
            }
        }
        float mask = 0;
        for (int k = 0; k < 4; ++k)
          mask += weights[k] * unit(samples[k].data[13]);
        p.data[13] = byte(mask);
        if (p == PaintPixel{})
          continue;
        size_t tile = size_t(y / 32) * surface->columns + x / 32;
        auto &dest = writable[li][tile];
        if (!dest) {
          allocated += sizeof(PaintTile);
          if (allocated > 128 * 1024 * 1024)
            throw std::length_error("UV transfer exceeds paint storage budget");
          dest = std::make_shared<PaintTile>();
          result->layers_[li].tiles[tile] = dest;
        }
        dest->pixels[size_t(y % 32) * 32 + x % 32] = p;
      }
    }
  }
  result->allocated_tiles_ = 0;
  for(const auto &layer:result->layers_)result->allocated_tiles_+=tile_count(layer);
  if(result->surface_->source_capture_only &&
     (!result->source_maps_||result->baked_||result->color_projection_||result->normal_projection_||
      result->allocated_tiles_||result->allocated_color_tiles_))
    throw std::invalid_argument("Initial original-coordinate capture cannot contain writable paint data");
  result->composite_token_=next_composite_token();
  ++result->revision_;
  return result;
}
namespace {
void word(std::vector<uint8_t> &out, uint32_t v) {
  for (int k = 0; k < 4; ++k)
    out.push_back(uint8_t(v >> (8 * k)));
}
uint32_t word(const std::vector<uint8_t> &bytes, size_t offset) {
  uint32_t v = 0;
  for (int k = 0; k < 4; ++k)
    v |= uint32_t(bytes[offset + k]) << (8 * k);
  return v;
}
} // namespace
nlohmann::json PaintedMesh::encode(PaintImageArchive *archive) const {
  return encode(archive,nullptr);
}
nlohmann::json PaintedMesh::encode(PaintImageArchive *archive,NormalSamplingArchive *domains) const {
  return encode(archive,domains,nullptr);
}
nlohmann::json PaintedMesh::encode(PaintImageArchive *archive,NormalSamplingArchive *domains,PaintSamplingArchive *paint_domains) const {
  return encode_native_payload(archive,domains,paint_domains,nullptr);
}
nlohmann::json PaintedMesh::encode_native_payload(PaintImageArchive *archive,NormalSamplingArchive *domains,PaintSamplingArchive *paint_domains,const detail::PolygonSurfaceOwner *polygon) const {
  return encode_impl(archive,domains,paint_domains,{},polygon);
}
nlohmann::json detail::TextureSetPaintAccess::encode(const PaintedMesh &paint,PaintImageArchive *images,
    NormalSamplingArchive *normals,PaintSamplingArchive *sampling){
  const auto &domain=paint.texture_set_domain_;
  if(!domain||!domain->matches(paint.mesh_owner())||!images)
    throw std::invalid_argument("Paint7 needs its exact owning membership and shared image archive");
  auto payload=paint.encode_impl(images,normals,sampling,domain);
  return {{"version",7},{"set",domain->set()},{"payload",std::move(payload)}};
}
nlohmann::json detail::TextureSetPaintAccess::encode_source_grid(const PaintedMesh&paint,
    const std::shared_ptr<const TextureSetFieldSource>&source,PaintImageArchive*images,NormalSamplingArchive*normal,PaintSamplingArchive*sampling){
  if(!source||!images)throw std::invalid_argument("Source Paint7 needs its actual field/image archive");source->authenticate_owner(source);source->validate_field(source->field());if(!source->owns_read_paint(paint)||!source->polygon_owner())throw std::invalid_argument("Source Paint7 cannot encode a foreign original/grid owner");return paint.encode_impl(images,normal,sampling,source->field().domain,source->polygon_owner().get(),source.get());
}
nlohmann::json PaintedMesh::encode_impl(PaintImageArchive *archive,NormalSamplingArchive *domains,
    PaintSamplingArchive *paint_domains,const std::shared_ptr<const TextureSetFaceDomain> &membership,const detail::PolygonSurfaceOwner *polygon,const detail::TextureSetFieldSource*source_field) const {
  if(!source_sampling_prepared()&&source_maps_&&source_maps_->uses_alternate_uv()&&baked_)
    throw std::logic_error("Unowned original-coordinate Bake cannot be saved; prepare genuine source atlas ownership");

  if(polygon&&membership&&!source_field)throw std::invalid_argument("Polygon field Paint6 and texture-set Paint7 require their complete shared-field adapter");
  if(source_field&&(!source_field->owns_read_paint(*this)||source_field->polygon_owner().get()!=polygon||!membership||source_field->field().domain!=membership||source_field->field().domain.owner_before(membership)||membership.owner_before(source_field->field().domain)))throw std::invalid_argument("Source Paint7 omitted its actual original/read field and membership proof");
  if(polygon){if(!archive)throw std::invalid_argument("Polygon Paint6 requires its actual owning image archive");polygon->validate_current(*this);}
  if(!detail::TextureSetPaintAccess::valid_raster(*this)||(!source_field&&texture_set_domain_!=membership)||(membership&&!membership->matches(mesh_owner())))
    throw std::invalid_argument("Texture-set paint requires its Scene40/Paint7 membership archive context");
  if(membership&&!source_field&&(source_sampling_prepared()||paint_sampling_domain()))throw std::invalid_argument("Independent source or polygon field needs its geometry-deduplicated Paint7 adapter");
  if(!valid_source_sampling_attachment())throw std::invalid_argument("Independent source paint lost its own sampling attachment");
  const auto sampling=paint_sampling_domain();
  if(!valid_paint_sampling_attachment())throw std::logic_error("Paint sampling geometry attachment is invalid");
  if(sampling&&sampling->version()==3&&!polygon){if(!archive)throw std::invalid_argument("Polygon Paint6 requires its actual owning image archive");return detail::PolygonSurfaceArchive::writer().encode_paint(*this,*archive,domains);}
  if(sampling&&!polygon&&(!paint_domains||!archive))throw std::logic_error("Retained writable paint requires its complete paired Scene24 archive context");
  if((!domains||!archive)&&((baked_&&baked_->normal_authority)||
     (normal_projection_&&normal_projection_->version==2)))
    throw std::invalid_argument("Retained normal bake requires an owning sampling archive context");
  if (editing_)
    throw std::logic_error("Finish the paint stroke before saving");
  nlohmann::json j = {{"resolution", resolution()},
                      {"active", active_},
                      {"next_id", next_},
                      {"layers", nlohmann::json::array()}};
  if(surface_->options.policy==PaintUvPolicy::SourceAuthored)j["uv_policy"]="source-authored";
  else if(surface_->options.policy==PaintUvPolicy::PolygonAuthored)j["uv_policy"]="polygon-authored";
  PaintImageArchive local;
  if(source_maps_)j["source_maps"]=encode_source_maps(*source_maps_,archive?*archive:local);
  if(color_projection_)j["color_projection"]=encode_color_projection(*color_projection_,archive?*archive:local);
  if(normal_projection_)j["normal_projection"]=encode_normal_projection(*normal_projection_,archive?*archive:local,domains);
  if(!archive && !source_sampling_prepared() && (source_maps_ || color_projection_ || normal_projection_))j["source_images"]=local.encode();
  if(!membership){
  std::vector<uint8_t> positions, normals, uv, indices;
  for (auto p : mesh().positions)
    for (float f : {p.x, p.y, p.z})
      word(positions, std::bit_cast<uint32_t>(f));
  for (auto p : mesh().normals)
    for (float f : {p.x, p.y, p.z})
      word(normals, std::bit_cast<uint32_t>(f));
  for (auto p : mesh().uv)
    for (float f : p)
      word(uv, std::bit_cast<uint32_t>(f));
  for (auto i : mesh().indices)
    word(indices, i);
  j["mesh"] = {{"positions", nlohmann::json::binary(positions)},
               {"normals", nlohmann::json::binary(normals)},
               {"uv", nlohmann::json::binary(uv)},
               {"indices", nlohmann::json::binary(indices)}};
  j["mesh"]["uv_pins"] = nlohmann::json::binary(mesh().uv_pins);
  j["mesh"]["uv_seams"] = nlohmann::json::binary(mesh().uv_seams);
  }
  for (auto &l : layers_) {
    nlohmann::json tiles = nlohmann::json::array();
    for (size_t i = 0; i < l.tiles.size(); ++i)
      if (l.tiles[i]) {
        std::vector<uint8_t> bytes;
        bytes.reserve(32 * 32 * 14);
        for (auto &p : l.tiles[i]->pixels)
          bytes.insert(bytes.end(), p.data.begin(), p.data.end());
        tiles.push_back(
            {{"index", i}, {"data", nlohmann::json::binary(bytes)}});
      }
    j["layers"].push_back({{"id", l.id},
                           {"name", l.name},
                           {"visible", l.visible},
                           {"opacity", l.opacity},
                           {"tiles", std::move(tiles)}});
    if(color_projection_) {
      if(l.pbr_tiles.size()!=l.tiles.size())throw std::logic_error("Prepared PBR layer grid is incomplete");
      auto colors=nlohmann::json::array();
      for(size_t i=0;i<l.pbr_tiles.size();++i)if(l.pbr_tiles[i]) {
        if(!l.tiles[i])throw std::logic_error("Prepared PBR color tile has no common coverage owner");
        std::vector<uint8_t> bytes;bytes.reserve(sizeof(PaintColorTile));
        for(const auto &pixel:l.pbr_tiles[i]->pixels)bytes.insert(bytes.end(),pixel.begin(),pixel.end());
        colors.push_back({{"index",i},{"data",nlohmann::json::binary(bytes)}});
      }
      j["layers"].back()["pbr_color_tiles"]=std::move(colors);
    } else if(!l.pbr_tiles.empty())throw std::logic_error("PBR color planes have no preparation");
    if(!l.emission_tiles.empty()) {
      if(l.emission_tiles.size()!=l.tiles.size())throw std::logic_error("Incomplete physical emission tile grid");
      auto emission=nlohmann::json::array();
      for(size_t i=0;i<l.emission_tiles.size();++i)if(l.emission_tiles[i]) {
        if(!l.tiles[i])throw std::logic_error("Physical emission tile has no common coverage owner");
        std::vector<uint8_t> bytes;bytes.reserve(sizeof(PaintEmissionTile));
        size_t pixel_index=0;
        for(const auto &pixel:l.emission_tiles[i]->pixels){pixel.validate();
          if(!emission_fallback_matches(pixel,l.tiles[i]->pixels[pixel_index++]))throw std::logic_error("Physical emission fallback bytes differ from their owning linear plane");
          for(float v:pixel.legacy)word(bytes,std::bit_cast<uint32_t>(v));
          for(float v:pixel.physical)word(bytes,std::bit_cast<uint32_t>(v));
          word(bytes,std::bit_cast<uint32_t>(pixel.coverage));}
        emission.push_back({{"index",i},{"data",nlohmann::json::binary(bytes)}});
      }
      if(!emission.empty())j["layers"].back()["physical_emission_tiles"]=std::move(emission);
    }
  }
  if (baked_) {
    const auto encode_bake=[&]{
      nlohmann::json field= {{"version", 1},
                      {"color", nlohmann::json::binary(baked_->maps.color.rgba)},
                      {"orm", nlohmann::json::binary(baked_->maps.metal_rough.rgba)},
                      {"emission", nlohmann::json::binary(baked_->maps.emission.rgba)},
                      {"normal", nlohmann::json::binary(baked_->maps.normal.rgba)},
                      {"coverage", nlohmann::json::binary(baked_->coverage)}};
      if(baked_->color_interpretation==BakedColorInterpretation::ResolvedPbr) {
        field["version"]=2;field["color_interpretation"]="resolved-pbr";
        field["pbr_color"]=nlohmann::json::binary(baked_->maps.pbr_color.rgba);
      }
      if(baked_->normal_interpretation==BakedNormalInterpretation::NativeAttributes){
        field["version"]=3;field["normal_interpretation"]="native-attributes-v1";
        field["mtoon_normal"]=nlohmann::json::binary({});
      }else if(baked_->normal_interpretation==BakedNormalInterpretation::PreparedAttributes){
        field["version"]=3;field["normal_interpretation"]="prepared-attributes-v1";
        field["mtoon_normal"]=nlohmann::json::binary(baked_->maps.mtoon_normal.rgba);
      }
      if(baked_->normal_authority){
        if(!domains||!normal_projection_||normal_projection_->version!=2)
          throw std::invalid_argument("Retained normal bake requires its own sampling model");
        field["version"]=4;
        field["normal_interpretation"]="retained-field-metadata-v1";
        field["normal_authority"]=domains->add_detail(baked_->normal_authority);
      }
      if(baked_->normal_field){
        if(!archive)throw std::invalid_argument("Independent baked normal persistence needs the actual Scene33 image archive");
        field["version"]=5;field["normal_interpretation"]="independent-baked-field-v1";
        field["normal_field"]=detail::BakedNormalArchive::writer().add_field(baked_->normal_field);
      }
      if(baked_->emission_field){
        field["base_version"]=field["version"];
        field["version"]=6;
        field["float_emission"]=nlohmann::json::binary(baked_->emission_field->encode());
        detail::BakedEmissionWriteScope::field_emitted(archive);
      }
      return field;
    };
    j["baked"]=detail::SharedBakeWriter::encode(*this,archive,[&]{return paint_domains->add(sampling);},encode_bake);
  }
  if(source_sampling_prepared()) {
    if(sampling||color_projection_||normal_projection_||surface_->diagnostics.policy!=PaintUvPolicy::Strict||!valid_source_sampling_attachment())
      throw std::invalid_argument("Independent source paint has an unsupported current interpretation");
    if(baked_){
      if(baked_->normal_interpretation!=BakedNormalInterpretation::CanonicalMikk||baked_->normal_authority||baked_->normal_field)
        throw std::invalid_argument("Source Bake writer needs its explicit current normal interpretation");
      detail::BakedEmissionWriteScope::source_bake_emitted(archive);
    }
    auto before=source_sampling_original();SourcePaintAccess::pristine(*before);
    if(storage_bytes()>128*1024*1024||SourcePaintAccess::working_bytes(*this)>SourcePaintOptions::maximum_working_bytes)
      throw std::length_error("Independent source paint exceeds persistable resource budget");
    std::vector<uint8_t> vertices;vertices.reserve(source_sampling_vertices().size()*4);
    for(auto v:source_sampling_vertices())word(vertices,v);
    auto encoded=before->encode(archive?archive:&local,domains,paint_domains);
    nlohmann::json envelope{{"version",baked_?8:4},{"payload",std::move(j)},{"before",std::move(encoded)},
      {"vertices",nlohmann::json::binary(std::move(vertices))}};
    if(const auto &uv1=surface_->original_sampling->authored_uv1){std::vector<uint8_t> bytes;bytes.reserve(uv1->size()*8);
      for(const auto &q:*uv1)for(float value:q)word(bytes,std::bit_cast<uint32_t>(value));envelope["authored_uv1"]=nlohmann::json::binary(std::move(bytes));}
    if(!archive)envelope["source_images"]=local.encode();
    return envelope;
  }
  if(polygon)return j; // Only the typed private Paint6 writer can call this branch.
  if(has_physical_emission())return {{"version",3},{"payload",std::move(j)},{"domain",sampling?nlohmann::json(paint_domains->add(sampling)):nlohmann::json(nullptr)}};
  if(sampling)return {{"version",2},{"payload",std::move(j)},{"domain",paint_domains->add(sampling)}};
  return j;
}
std::shared_ptr<PaintedMesh> PaintedMesh::decode(const nlohmann::json &j,const PaintImageArchive *archive) {
  return decode(j,archive,false);
}
std::shared_ptr<PaintedMesh> PaintedMesh::decode(const nlohmann::json &j,const PaintImageArchive *archive,bool allow_polygon) {
  return decode(j,archive,allow_polygon,nullptr);
}
std::shared_ptr<PaintedMesh> PaintedMesh::decode(const nlohmann::json &input,const PaintImageArchive *archive,
    bool allow_polygon,const NormalSamplingArchive *domains) {
  return decode(input,archive,allow_polygon,domains,nullptr);
}
std::shared_ptr<PaintedMesh> PaintedMesh::decode(const nlohmann::json &input,const PaintImageArchive *archive,
    bool allow_polygon,const NormalSamplingArchive *domains,const PaintSamplingArchive *paint_domains) {
  return decode(input,archive,allow_polygon,domains,paint_domains,SourcePaintDecodeContext{});
}
std::shared_ptr<PaintedMesh> PaintedMesh::decode(const nlohmann::json &input,const PaintImageArchive *archive,
    bool allow_polygon,const NormalSamplingArchive *domains,const PaintSamplingArchive *paint_domains,bool physical) {
  return decode(input,archive,allow_polygon,domains,paint_domains,SourcePaintDecodeContext{false,physical});
}
std::shared_ptr<PaintedMesh> PaintedMesh::decode(const nlohmann::json &input,const PaintImageArchive *archive,
    bool allow_polygon,const NormalSamplingArchive *domains,const PaintSamplingArchive *paint_domains,
    const SourcePaintDecodeContext &context) {
  if(input.is_object()&&input.contains("version")&&input.at("version")==6){if(!archive)throw std::invalid_argument("Polygon Paint6 requires its actual owning image archive");return detail::PolygonSurfaceArchive::reader().decode_paint(input,*archive,domains);}
  return decode_native_payload(input,archive,allow_polygon,domains,paint_domains,context,nullptr);
}
std::shared_ptr<PaintedMesh> PaintedMesh::decode_native_payload(const nlohmann::json &input,const PaintImageArchive *archive,
    bool allow_polygon,const NormalSamplingArchive *domains,const PaintSamplingArchive *paint_domains,
    const SourcePaintDecodeContext &context,const detail::PolygonSurfaceStage *polygon) {
  return decode_impl(input,archive,allow_polygon,domains,paint_domains,context,{},polygon);
}
std::shared_ptr<PaintedMesh> detail::TextureSetPaintAccess::decode(const nlohmann::json &input,
    const PaintImageArchive *images,bool polygon,const NormalSamplingArchive *normals,
    const PaintSamplingArchive *sampling,const SourcePaintDecodeContext &context,
    const std::shared_ptr<const TextureSetFaceDomain> &membership,const PaintedMesh*shared_raster){
  if(!images||!membership||!membership->topology()||!membership->topology()->geometry()||
     !input.is_object()||input.size()!=3||!input.at("version").is_number_integer()||input.at("version")!=7||
     !input.at("set").is_number_integer()||input.at("set").get<int64_t>()<0||
     input.at("set").get<uint64_t>()!=membership->set()||!input.at("payload").is_object())
    throw std::invalid_argument("Paint7 requires its authenticated owning geometry and membership");
  const auto &payload=input.at("payload");
  if(payload.contains("version")&&(payload.at("version")==4||payload.at("version")==8||payload.at("version")==6))
    throw std::invalid_argument("Independent source or polygon field needs its geometry-deduplicated Paint7 adapter");
  return PaintedMesh::decode_impl(payload,images,polygon,normals,sampling,context,membership,nullptr,shared_raster);
}
std::shared_ptr<PaintedMesh>detail::TextureSetPaintAccess::decode_source_grid(const nlohmann::json&payload,
    const PaintImageArchive*images,bool polygon,const NormalSamplingArchive*normal,const PaintSamplingArchive*sampling,
    const SourcePaintDecodeContext&context,const PolygonSurfaceStage&stage,const PaintedMesh&prototype,
    const std::shared_ptr<const TextureSetFaceDomain>&membership,const std::shared_ptr<const TextureSetFieldSource>&source){
  if(!images)throw std::invalid_argument("Source grid requires its actual image archive");if(source)source->authenticate_owner(source);return PaintedMesh::decode_impl(payload,images,polygon,normal,sampling,context,membership,&stage,&prototype,source.get());
}
std::shared_ptr<PaintedMesh> PaintedMesh::decode_impl(const nlohmann::json &input,const PaintImageArchive *archive,
    bool allow_polygon,const NormalSamplingArchive *domains,const PaintSamplingArchive *paint_domains,
    const SourcePaintDecodeContext &context,const std::shared_ptr<const TextureSetFaceDomain> &membership,const detail::PolygonSurfaceStage *polygon,const PaintedMesh*shared_raster,const detail::TextureSetFieldSource*source_field) {
  if(polygon&&membership&&!source_field)throw std::invalid_argument("Polygon field Paint6 and texture-set Paint7 require their complete shared-field adapter");
  const bool independent=input.is_object()&&input.contains("version")&&(input.at("version")==4||input.at("version")==8);
  std::optional<PaintImageArchive> independent_images;
  std::shared_ptr<PaintedMesh> original;
  std::vector<uint32_t> original_vertices;
  std::shared_ptr<const std::vector<std::array<float,2>>> authored_uv1;
  if(independent) {
    if(!context.independent_source||input.size()!=(archive?4u:5u)+size_t(input.contains("authored_uv1"))||
       !input.at("version").is_number_integer()||!input.contains("payload")||!input.at("payload").is_object()||
       input.at("payload").contains("version")||input.at("payload").contains("source_images")||
       !input.contains("before")||!input.at("before").is_object()||input.at("before").contains("version")||
       input.at("before").contains("source_images")||!input.contains("vertices")||!input.at("vertices").is_binary()||
       input.at("vertices").get_binary().size()%4||input.at("vertices").get_binary().size()>8'000'000)
      throw std::invalid_argument("Paint4 requires its complete independent-source owning context");
    if(input.at("version")==8&&(!context.baked_emission||!context.baked_emission->source_bake_allowed()||!input.at("payload").contains("baked")))
      throw std::invalid_argument("Paint8 requires genuine Scene42 current Bake ownership");
    SourcePaintAccess::encoded_budget(input,context.physical_emission,context.baked_emission);
    if(!archive){independent_images=PaintImageArchive::decode(input.at("source_images"));archive=&*independent_images;}
    else if(input.contains("source_images"))throw std::invalid_argument("Ambiguous independent source image archive");
    const auto &bytes=input.at("vertices").get_binary();
    original_vertices.reserve(bytes.size()/4);
    for(size_t at=0;at<bytes.size();at+=4)original_vertices.push_back(uint32_t(bytes[at])|
      uint32_t(bytes[at+1])<<8|uint32_t(bytes[at+2])<<16|uint32_t(bytes[at+3])<<24);
    original=decode(input.at("before"),archive,false,nullptr,nullptr);
    SourcePaintAccess::pristine(*original);
    if(input.contains("authored_uv1")){
      const auto &data=input.at("authored_uv1").get_binary();auto values=std::make_shared<std::vector<std::array<float,2>>>();values->reserve(data.size()/8);
      for(size_t at=0;at<data.size();at+=8)values->push_back({std::bit_cast<float>(word(data,at)),std::bit_cast<float>(word(data,at+4))});authored_uv1=std::move(values);
    }
  }
  const auto &record=independent?input.at("payload"):input;
  std::shared_ptr<const PaintSamplingDomain> sampling;
  const bool physical=(record.is_object()&&record.contains("version")&&record.at("version")==3)||(polygon&&context.physical_emission);
  const auto &j=detail::paint_archive_payload(record,archive&&paint_domains,context.physical_emission);
  if(record.contains("version")&&!record.at("domain").is_null())
    sampling=paint_domains->at(record.at("domain").get<uint32_t>());
  if(j.contains("source_maps"))detail::source_uv_archive_measure(j.at("source_maps"));
  const bool retained_projection=j.contains("normal_projection")&&
    j.at("normal_projection").is_object()&&j.at("normal_projection").contains("domain");
  const bool retained_bake=j.contains("baked")&&j.at("baked").is_object()&&
    j.at("baked").contains("normal_authority");
  if((retained_projection||retained_bake)&&(!domains||!archive))
    throw std::invalid_argument("Retained normal Paint requires paired image and sampling archive context");
  std::shared_ptr<const Mesh> geometry;
  if(polygon&&shared_raster&&!membership&&!j.contains("mesh")){
    if(shared_raster!=polygon->paint.get())throw std::invalid_argument("Source grid needs its actual node-stage raster");geometry=shared_raster->mesh_owner();
  }else if(membership){
    if(j.contains("mesh"))throw std::invalid_argument("Paint7 current geometry must use its single owning topology record");
    geometry=membership->topology()->geometry();
  }else{
  auto &m = j.at("mesh");
  auto blob = [&](const char *name, size_t stride,
                  size_t limit) -> const std::vector<uint8_t> & {
    auto &v = m.at(name);
    if (!v.is_binary() || v.get_binary().empty() ||
        v.get_binary().size() % stride ||
        v.get_binary().size() / stride > limit)
      throw std::runtime_error("Invalid persisted mesh buffer");
    return v.get_binary();
  };
  Mesh mesh;
  auto &p = blob("positions", 12, 2'000'000),
       &n = blob("normals", 12, 2'000'000), &uv = blob("uv", 8, 2'000'000),
       &indices = blob("indices", 12, 2'000'000);
  for (size_t i = 0; i < p.size(); i += 12)
    mesh.positions.push_back({std::bit_cast<float>(word(p, i)),
                              std::bit_cast<float>(word(p, i + 4)),
                              std::bit_cast<float>(word(p, i + 8))});
  for (size_t i = 0; i < n.size(); i += 12)
    mesh.normals.push_back({std::bit_cast<float>(word(n, i)),
                            std::bit_cast<float>(word(n, i + 4)),
                            std::bit_cast<float>(word(n, i + 8))});
  for (size_t i = 0; i < uv.size(); i += 8)
    mesh.uv.push_back({std::bit_cast<float>(word(uv, i)),
                       std::bit_cast<float>(word(uv, i + 4))});
  for (size_t i = 0; i < indices.size(); i += 4)
    mesh.indices.push_back(word(indices, i));
  auto flags = [&](const char *key, size_t count, std::vector<uint8_t> &out) {
    if (!m.contains(key))
      return;
    auto &v = m.at(key);
    if (!v.is_binary() ||
        (!v.get_binary().empty() && v.get_binary().size() != count))
      throw std::runtime_error("Invalid persisted UV edit flags");
    out.assign(v.get_binary().begin(), v.get_binary().end());
  };
  flags("uv_pins", mesh.positions.size(), mesh.uv_pins);
  flags("uv_seams", mesh.indices.size(), mesh.uv_seams);
  geometry=std::make_shared<const Mesh>(std::move(mesh));
  }
  auto &resolution = j.at("resolution");
  if ((!resolution.is_number_unsigned() &&
       (!resolution.is_number_integer() || resolution.get<int64_t>() < 64)) ||
      resolution.get<uint64_t>() > 2048)
    throw std::runtime_error("Invalid paint resolution");
  PaintSurfaceOptions surface_options;
  if(j.contains("uv_policy")){
    const auto &policy=j.at("uv_policy");
    if(policy.is_string()&&policy.get_ref<const std::string&>()=="polygon-authored") {
      if(!allow_polygon)throw std::runtime_error("Polygon-authored UV policy requires authenticated Scene12 authority");
      surface_options.policy=PaintUvPolicy::PolygonAuthored;
    } else {
      if(!policy.is_string()||policy.get_ref<const std::string&>()!="source-authored"||(!membership&&!j.contains("source_maps")&&!j.contains("color_projection")&&!j.contains("normal_projection")))throw std::runtime_error("Invalid persisted source UV policy");
      surface_options.policy=PaintUvPolicy::SourceAuthored;
    }
  }
  std::shared_ptr<PaintedMesh>result;
  if(source_field){
    using detail::native_weight_state::owner;
    if(!polygon||!membership||!shared_raster||source_field->field().painted.get()!=shared_raster||!owner(source_field->field().domain,membership)||!owner(source_field->polygon_owner(),polygon->owner)||!owner(geometry,shared_raster->mesh_owner())||resolution.get<uint64_t>()!=shared_raster->resolution()||j.contains("source_maps")||j.contains("normal_projection")||j.contains("color_projection"))throw std::invalid_argument("Source Paint7 current grid differs from its actual field/node prototype");
    result=shared_raster->snapshot();result->allocated_tiles_=result->allocated_color_tiles_=0;
    if(j.contains("baked"))result->baked_.reset();
    result->composite_cache_=std::make_shared<CompositeCache>();result->composite_token_=next_composite_token();
  }else if(shared_raster&&!polygon){
    using detail::native_weight_state::owner;
    if(polygon||!membership||!shared_raster->valid_source_sampling_attachment()||shared_raster->in_stroke()||!detail::TextureSetPaintAccess::valid_raster(*shared_raster)||!owner(shared_raster->texture_set_domain_,membership)||!owner(shared_raster->mesh_owner(),geometry)||shared_raster->resolution()!=resolution.get<uint64_t>()||shared_raster->surface_->options.policy!=surface_options.policy||shared_raster->surface_->paint_domain||shared_raster->source_sampling_prepared())throw std::invalid_argument("Paint7 shared raster differs from its actual membership/geometry/resolution/policy");
    result=std::shared_ptr<PaintedMesh>(new PaintedMesh);result->surface_=shared_raster->surface_;result->texture_set_domain_=membership;result->sampling_attachment_=shared_raster->sampling_attachment_;result->composite_cache_=std::make_shared<CompositeCache>();result->composite_token_=next_composite_token();
  }else if(polygon){
    if(!archive||membership||!polygon->owner||!polygon->paint||independent||sampling||surface_options.policy!=PaintUvPolicy::Strict||j.contains("source_maps")||j.contains("normal_projection")||j.contains("color_projection")||j.contains("source_images"))throw std::invalid_argument("Polygon Paint6 payload contradicts its actual independent field/grid model");
    polygon->owner->validate_current(*polygon->paint);polygon->owner->validate_current_geometry(*geometry);
    if(resolution.get<uint64_t>()!=polygon->paint->resolution())throw std::invalid_argument("Polygon Paint6 changed its actual source grid resolution");
    // The node factory has already replayed and sealed this exact map. Parse
    // current layer values onto it; never build a second guessed UV raster.
    result=std::shared_ptr<PaintedMesh>(new PaintedMesh);
    result->surface_=polygon->paint->surface_;result->sampling_attachment_=polygon->paint->sampling_attachment_;
    result->composite_cache_=std::make_shared<CompositeCache>();result->composite_token_=next_composite_token();
  }else {
    const auto size=resolution.get<uint32_t>();
    const bool share=sampling&&sampling->version()==1&&surface_options.policy==PaintUvPolicy::Strict&&
        !membership&&detail::ScenePaintRasterReader::active();
    const auto cached=share?detail::ScenePaintRasterReader::find(*sampling,size):nullptr;
    if(cached){
      PaintSamplingLimits limits;
      limits.cancelled=[]{detail::NativeReadScope::check();return false;};
      sampling->validate_current(*geometry,limits);
      const auto surface=std::static_pointer_cast<const SurfaceMap>(cached);
      result=std::shared_ptr<PaintedMesh>(new PaintedMesh);
      result->surface_=std::make_shared<SurfaceMap>(*surface,std::move(geometry),std::move(sampling));
      result->seal_paint_sampling_attachment();
      result->composite_cache_=std::make_shared<CompositeCache>();
      result->composite_token_=next_composite_token();
    }else{
      result=std::shared_ptr<PaintedMesh>(new PaintedMesh(std::move(geometry),size,surface_options,std::move(sampling),membership));
      if(share)detail::ScenePaintRasterReader::retain(*result->paint_sampling_domain(),size,result->surface_);
    }
  }
  std::optional<PaintImageArchive> local_images;
  const PaintImageArchive *images=archive;
  if(j.contains("source_maps") || j.contains("color_projection") || j.contains("normal_projection")) {
    if(archive && j.contains("source_images"))throw std::runtime_error("Ambiguous original texture archive");
    if(!archive){local_images=PaintImageArchive::decode(j.at("source_images"));images=&*local_images;}
    if(j.contains("source_maps")){result->source_maps_=decode_source_maps(j.at("source_maps"),*images);result->source_maps_->validate_surface(result->mesh());}
    if(j.contains("normal_projection")){
      result->normal_projection_=decode_normal_projection(j.at("normal_projection"),*images,domains);
      result->normal_projection_->validate(result->resolution());
      if(result->surface_->diagnostics.policy!=PaintUvPolicy::Strict||result->surface_->diagnostics.additional_candidates||!result->surface_->diagnostics.unpaintable_uv_faces.empty())throw std::runtime_error("Prepared normal archive has no strict writable atlas");
    }
    if(j.contains("color_projection")) {
      result->color_projection_=decode_color_projection(j.at("color_projection"),*images);
      result->color_projection_->validate(result->resolution());
      if(result->surface_->diagnostics.additional_candidates || !result->surface_->diagnostics.unpaintable_uv_faces.empty())
        throw std::runtime_error("Prepared color archive does not have a writable atlas");
    }
  }
  auto positive = [](const nlohmann::json &v) {
    if ((!v.is_number_unsigned() &&
         (!v.is_number_integer() || v.get<int64_t>() <= 0)) ||
        v.get<uint64_t>() == 0)
      throw std::runtime_error("Invalid paint identity");
    return v.get<uint64_t>();
  };
  result->active_ = positive(j.at("active"));
  result->next_ = positive(j.at("next_id"));
  auto &layers = j.at("layers");
  if (!layers.is_array() || layers.empty() || layers.size() > 16)
    throw std::length_error("Invalid paint layer count");
  result->layers_.clear();
  std::set<uint64_t> ids;
  size_t allocated = 0;
  for (auto &v : layers) {
    if(v.contains("physical_emission_tiles")&&!(physical||(independent&&context.physical_emission)))
      throw std::invalid_argument("Physical emission requires its independent owning capability");
    PaintLayer l;
    l.id = positive(v.at("id"));
    if (l.id >= result->next_ || !ids.insert(l.id).second)
      throw std::runtime_error("Invalid paint layer identity");
    l.name = v.at("name").get<std::string>();
    name_valid(l.name);
    l.visible = v.at("visible").get<bool>();
    l.opacity = v.at("opacity").get<float>();
    if (!std::isfinite(l.opacity) || l.opacity < 0 || l.opacity > 1)
      throw std::runtime_error("Invalid layer opacity");
    l.tiles.resize(result->surface_->tiles.size());
    auto &tiles = v.at("tiles");
    if (!tiles.is_array() || tiles.size() > l.tiles.size())
      throw std::length_error("Invalid paint tile count");
    for (auto &t : tiles) {
      auto &index = t.at("index"), &data = t.at("data");
      if (!index.is_number_unsigned() &&
          (!index.is_number_integer() || index.get<int64_t>() < 0))
        throw std::runtime_error("Invalid paint tile index");
      auto i = index.get<uint64_t>();
      if (i >= l.tiles.size() || l.tiles[i] || !data.is_binary() ||
          data.get_binary().size() != 32 * 32 * 14)
        throw std::runtime_error("Invalid paint tile data");
      allocated += sizeof(PaintTile);
      if (allocated > 128 * 1024 * 1024)
        throw std::length_error("Paint storage budget exceeded");
      auto tile = std::make_shared<PaintTile>();
      auto &bytes = data.get_binary();
      size_t offset = 0;
      for (auto &p : tile->pixels)
        for (auto &b : p.data)
          b = bytes[offset++];
      l.tiles[i] = std::move(tile);
    }
    if(result->color_projection_) {
      const auto &colors=v.at("pbr_color_tiles");
      if(!colors.is_array() || colors.size()>l.tiles.size())throw std::runtime_error("Invalid prepared PBR tile count");
      l.pbr_tiles.resize(l.tiles.size());
      for(const auto &color:colors) {
        const auto &index=color.at("index"),&data=color.at("data");
        if((!index.is_number_unsigned() && (!index.is_number_integer() || index.get<int64_t>()<0)) || index.get<uint64_t>()>=l.tiles.size())throw std::runtime_error("Invalid PBR color tile identity");
        const auto i=size_t(index.get<uint64_t>());
        if(!l.tiles[i] || l.pbr_tiles[i] || !data.is_binary() || data.get_binary().size()!=sizeof(PaintColorTile))throw std::runtime_error("Invalid PBR color tile data");
        allocated+=sizeof(PaintColorTile);
        if(allocated+result->color_projection_->vertex_rgba->rgba.size()>128*1024*1024)throw std::length_error("Prepared paint storage budget exceeded");
        auto tile=std::make_shared<PaintColorTile>();size_t offset=0;
        for(auto &pixel:tile->pixels)for(auto &component:pixel)component=data.get_binary()[offset++];
        l.pbr_tiles[i]=std::move(tile);++result->allocated_color_tiles_;
      }
    } else if(v.contains("pbr_color_tiles"))throw std::runtime_error("PBR color tiles require explicit current color preparation");
    if(v.contains("physical_emission_tiles")) {
      if(!(physical||(independent&&context.physical_emission)))throw std::invalid_argument("Physical emission tiles require their actual owning capability");
      const auto &emission=v.at("physical_emission_tiles");
      if(!emission.is_array()||emission.empty()||emission.size()>l.tiles.size())throw std::invalid_argument("Invalid physical emission tile count");
      l.emission_tiles.resize(l.tiles.size());
      for(const auto &record:emission) {
        if(!record.is_object()||record.size()!=2)throw std::invalid_argument("Invalid physical emission tile record");
        const auto &index=record.at("index"),&data=record.at("data");
        if((!index.is_number_unsigned()&&(!index.is_number_integer()||index.get<int64_t>()<0))||index.get<uint64_t>()>=l.tiles.size())throw std::invalid_argument("Invalid physical emission tile identity");
        const auto i=size_t(index.get<uint64_t>());
        if(!l.tiles[i]||l.emission_tiles[i]||!data.is_binary()||data.get_binary().size()!=32*32*7*4)throw std::invalid_argument("Invalid physical emission tile data");
        allocated+=sizeof(PaintEmissionTile);
        if(allocated>128*1024*1024)throw std::length_error("Physical paint storage budget exceeded");
        auto tile=std::make_shared<PaintEmissionTile>();size_t offset=0;
        size_t pixel_index=0;
        for(auto &pixel:tile->pixels){
          for(auto &value:pixel.legacy){value=std::bit_cast<float>(word(data.get_binary(),offset));offset+=4;}
          for(auto &value:pixel.physical){value=std::bit_cast<float>(word(data.get_binary(),offset));offset+=4;}
          pixel.coverage=std::bit_cast<float>(word(data.get_binary(),offset));offset+=4;
          pixel.validate();
          if(!emission_fallback_matches(pixel,l.tiles[i]->pixels[pixel_index++]))throw std::invalid_argument("Physical emission fallback bytes differ from their linear plane");
        }
        l.emission_tiles[i]=std::move(tile);
      }
    }
    result->layers_.push_back(std::move(l));
  }
  result->layer(result->active_);
  result->allocated_tiles_=0;
  for(const auto &layer:result->layers_)result->allocated_tiles_+=tile_count(layer);
  if(independent){
    SourcePaintOptions resource_limits;resource_limits.max_working_bytes=SourcePaintOptions::maximum_working_bytes;
    SourcePaintAccess::bind(*result,std::move(original),original_vertices,resource_limits,std::move(authored_uv1));
  }
  // A stand-alone paint payload cannot acquire writable alternate-UV
  // ownership through the bake installer. Reject it before decoding the maps.
  if(!independent&&result->source_maps_&&result->source_maps_->uses_alternate_uv()&&
     (result->allocated_tiles_||result->allocated_color_tiles_||result->baked_||j.contains("baked")))
    throw std::invalid_argument("Paint on independent original coordinates requires its actual Paint4 owner");
  if (j.contains("baked")) {
    const auto shared=detail::shared_bake_for(record,j,result->resolution(),context.baked_emission);
    const auto &v=shared?*shared->record:j.at("baked");
    if(shared&&(!archive||independent||membership||polygon||result->baked_||
        !result->paint_sampling_domain()||result->paint_sampling_domain()->version()!=1||
        !result->valid_paint_sampling_attachment()))
      throw std::invalid_argument("Shared baked field needs its authenticated native raster");
    if(shared&&shared->owner){
      result->baked_=shared->owner;
      result->composite_token_=next_composite_token();
      ++result->revision_;
    }else{
      if(v.contains("normal_authority")&&!domains)
        throw std::invalid_argument("Retained normal bake requires a complete owning sampling archive context");
      const auto baked_version=detail::baked_legacy_version(v,context.baked_emission);
      const bool floating=v.at("version")==6;
      const bool retained=baked_version==4;
      const bool layered=baked_version==5;
      if(layered){
        (void)detail::BakedNormalArchive::reader();
        if(!v.contains("normal_field")||v.at("normal_interpretation")!="independent-baked-field-v1"||result->normal_projection_)
          throw std::invalid_argument("Independent baked field has contradictory intrinsic/normal ownership");
      }else if(v.contains("normal_field"))throw std::invalid_argument("Independent baked reference requires payload5/Scene33");
      if(retained&&(!domains||!v.contains("normal_authority")||!result->normal_projection_||result->normal_projection_->version!=2))
        throw std::invalid_argument("Retained normal metadata requires complete paired field context");
      if(!retained&&v.contains("normal_authority"))
        throw std::invalid_argument("Normal event identity requires baked metadata4");
      if(baked_version==1 && (v.contains("color_interpretation") || v.contains("pbr_color")))throw std::runtime_error("Resolved baked colors require baked payload2");
      uint32_t r = result->resolution();
      auto bytes = [&](const char *key, size_t count, bool optional = false) {
        const auto &value = v.at(key);
        if (!value.is_binary() ||
            (value.get_binary().size() != count && !(optional && value.get_binary().empty())))
          throw std::runtime_error("Invalid persisted baked image or coverage buffer");
        return std::vector<uint8_t>(value.get_binary().begin(), value.get_binary().end());
      };
      BakedTextures baked;
      if(floating){
        detail::baked_float_preflight(v,r,context.baked_emission);
        baked.emission_field=BakedEmissionField::decode(v.at("float_emission").get_binary(),
          {128*1024*1024,[]{detail::NativeReadScope::check();return false;}});
      }
      baked.maps.color = {r, r, bytes("color", size_t(r)*r*4)};
      baked.maps.metal_rough = {r, r, bytes("orm", size_t(r)*r*4)};
      if(!floating)baked.maps.emission = {r, r, bytes("emission", size_t(r)*r*4)};
      auto normal = bytes("normal", size_t(r)*r*4, true);
      if (!normal.empty()) baked.maps.normal = {r, r, std::move(normal)};
      baked.coverage = bytes("coverage", size_t(r)*r);
      if(baked_version!=3&&!retained&&!layered&&(v.contains("normal_interpretation")||v.contains("mtoon_normal")))throw std::runtime_error("Prepared normals require baked payload3");
      if(baked_version==2||((baked_version==3||retained||layered)&&(v.contains("color_interpretation")||v.contains("pbr_color")))) {
        if(v.at("color_interpretation")!="resolved-pbr")throw std::runtime_error("Unknown baked color interpretation");
        baked.color_interpretation=BakedColorInterpretation::ResolvedPbr;baked.maps.pbr_color={r,r,bytes("pbr_color",size_t(r)*r*4)};
      }
      if(baked_version==3||retained){
        const auto &kind=v.at("normal_interpretation");
        if(!retained&&kind=="native-attributes-v1")baked.normal_interpretation=BakedNormalInterpretation::NativeAttributes;
        else {
          if(!kind.is_string()||kind.get_ref<const std::string&>()!=(retained?"retained-field-metadata-v1":"prepared-attributes-v1"))
            throw std::runtime_error("Unknown baked normal interpretation");
          baked.normal_interpretation=BakedNormalInterpretation::PreparedAttributes;
        }
        auto normal=bytes("mtoon_normal",size_t(r)*r*4,true);if(!normal.empty())baked.maps.mtoon_normal={r,r,std::move(normal)};
      }
      if(layered){
        const auto &reference=v.at("normal_field");
        if((!reference.is_number_unsigned()&&(!reference.is_number_integer()||reference.get<int64_t>()<0))||reference.get<uint64_t>()>UINT32_MAX)throw std::invalid_argument("Invalid independent baked normal reference");
        baked.normal_interpretation=BakedNormalInterpretation::PreparedAttributes;
        baked.normal_field=detail::BakedNormalArchive::reader().field_at(reference.get<uint32_t>());
      }
      if(retained){
        const auto &reference=v.at("normal_authority");
        if((!reference.is_number_unsigned()&&(!reference.is_number_integer()||reference.get<int64_t>()<0))||
           reference.get<uint64_t>()>UINT32_MAX)
          throw std::invalid_argument("Invalid retained normal event reference");
        baked.normal_authority=domains->detail_at(reference.get<uint32_t>());
        baked.validate(r);
        if(bool(result->color_projection_)!=(baked.color_interpretation==BakedColorInterpretation::ResolvedPbr))
          throw std::invalid_argument("Retained baked color interpretation differs");
        // Only structural reconstruction here. Source replay/pairing happens in
        // the owning model before publication; ordinary install_bake cannot
        // manufacture or smuggle a retained event identity.
        result->baked_=std::make_shared<const BakedTextures>(std::move(baked));
        result->composite_token_=next_composite_token();++result->revision_;
      }else if(independent&&input.at("version")==8){
        SourcePaintOptions resource_limits;resource_limits.max_working_bytes=SourcePaintOptions::maximum_working_bytes;
        result=SourcePaintAccess::install(*result,std::move(baked),resource_limits);
        context.baked_emission->note_source_bake();
      }else result->install_bake(std::move(baked));
      if(floating)context.baked_emission->note_validated_field();
      if(shared)shared->owner=result->baked_;
    }
    if(shared)shared->users.insert(&j);
  }
  if(physical&&!result->has_physical_emission())throw std::invalid_argument("Paint3 requires genuine physical emission ownership");
  result->allocated_tiles_ = 0;
  for(const auto &layer:result->layers_)result->allocated_tiles_+=tile_count(layer);
  if(result->surface_->source_capture_only &&
     (!result->source_maps_||result->baked_||result->color_projection_||result->normal_projection_||
      result->allocated_tiles_||result->allocated_color_tiles_))
    throw std::invalid_argument("Initial original-coordinate capture cannot contain writable paint data");
  if(result->storage_bytes()>128*1024*1024)throw std::length_error("Persisted paint exceeds total storage budget");
  if(independent) {
    if(result->storage_bytes()>128*1024*1024)throw std::length_error("Independent source paint exceeds total retained storage budget");
  }
  if(polygon&&!source_field)polygon->owner->validate_current(*result);
  if(source_field&&!detail::TextureSetPaintAccess::valid_source_member(*result,*source_field->original_paint(),membership,j.contains("baked")?result.get():nullptr))throw std::invalid_argument("Decoded source member changed its genuine source/raster ownership");
  return result;
}
void PaintedMesh::swap_state(PaintedMesh &other) noexcept {
  using std::swap;
  static_assert(noexcept(swap(composite_cache_,other.composite_cache_)));swap(composite_cache_,other.composite_cache_);
  static_assert(noexcept(swap(composite_token_,other.composite_token_)));swap(composite_token_,other.composite_token_);
  static_assert(noexcept(swap(before_composite_token_,other.before_composite_token_)));swap(before_composite_token_,other.before_composite_token_);
  static_assert(noexcept(swap(surface_,other.surface_)));swap(surface_,other.surface_);
  static_assert(noexcept(swap(texture_set_domain_,other.texture_set_domain_)));swap(texture_set_domain_,other.texture_set_domain_);
  static_assert(noexcept(swap(sampling_attachment_,other.sampling_attachment_)));swap(sampling_attachment_,other.sampling_attachment_);
  static_assert(noexcept(swap(source_maps_,other.source_maps_)));swap(source_maps_,other.source_maps_);
  static_assert(noexcept(swap(color_projection_,other.color_projection_)));swap(color_projection_,other.color_projection_);
  static_assert(noexcept(swap(normal_projection_,other.normal_projection_)));swap(normal_projection_,other.normal_projection_);
  static_assert(noexcept(swap(layers_,other.layers_)));swap(layers_,other.layers_);
  static_assert(noexcept(swap(baked_,other.baked_)));swap(baked_,other.baked_);
  static_assert(noexcept(swap(active_,other.active_)));swap(active_,other.active_);
  static_assert(noexcept(swap(next_,other.next_)));swap(next_,other.next_);
  static_assert(noexcept(swap(revision_,other.revision_)));swap(revision_,other.revision_);
  static_assert(noexcept(swap(editing_,other.editing_)));swap(editing_,other.editing_);
  static_assert(noexcept(swap(before_,other.before_)));swap(before_,other.before_);
  static_assert(noexcept(swap(before_colors_,other.before_colors_)));swap(before_colors_,other.before_colors_);
  static_assert(noexcept(swap(before_emission_,other.before_emission_)));swap(before_emission_,other.before_emission_);
  static_assert(noexcept(swap(undo_,other.undo_)));swap(undo_,other.undo_);
  static_assert(noexcept(swap(redo_,other.redo_)));swap(redo_,other.redo_);
  static_assert(noexcept(swap(history_bytes_,other.history_bytes_)));swap(history_bytes_,other.history_bytes_);
  static_assert(noexcept(swap(allocated_tiles_,other.allocated_tiles_)));swap(allocated_tiles_,other.allocated_tiles_);
  static_assert(noexcept(swap(allocated_color_tiles_,other.allocated_color_tiles_)));swap(allocated_color_tiles_,other.allocated_color_tiles_);
  static_assert(noexcept(swap(before_allocated_tiles_,other.before_allocated_tiles_)));swap(before_allocated_tiles_,other.before_allocated_tiles_);
  static_assert(noexcept(swap(before_allocated_color_tiles_,other.before_allocated_color_tiles_)));swap(before_allocated_color_tiles_,other.before_allocated_color_tiles_);
}
} // namespace dome
