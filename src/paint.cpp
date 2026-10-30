// SPDX-License-Identifier: GPL-3.0-or-later
// Ray/triangle picking adapted from Blender 5.4 Alpha isect_ray_tri_v3,
// GPL-2.0-or-later; see third_party/blender/math_geom.cc.reference.
#include "dome/paint.hpp"
#include "polygon-paint.hpp"
#include "dome/source-maps.hpp"
#include "dome/scene.hpp"
#include "dome/tangents.hpp"
#include "paint-color-access.hpp"
#include "paint-normal-access.hpp"
#include "color-projection-quality.hpp"
#include <bit>
#include <cstring>
#include <nlohmann/json.hpp>
#include <png.h>
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
struct CompositeRetention {
  struct Source {size_t references=0,bytes=0;};
  struct Entry {std::shared_ptr<const TextureMaps> maps;uint64_t token=0;CompositeFactors factors{};size_t derived=0,count=0;std::array<const Image*,8> sources{};const BakedTextures *base=nullptr;};
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
  std::shared_ptr<const TextureMaps> find(uint64_t token,const CompositeFactors &factors){
    std::lock_guard lock(mutex);auto found=std::find_if(entries.begin(),entries.end(),[&](auto &e){return e.token==token&&e.factors==factors;});
    if(found==entries.end())return {};
    auto result=found->maps;entries.splice(entries.begin(),entries,found);++hits;return result;
  }
  void retain(const std::shared_ptr<const TextureMaps> &maps,uint64_t token,const CompositeFactors &factors,bool hit){
    std::lock_guard lock(mutex);if(hit)++hits;else ++builds;
    auto found=std::find_if(entries.begin(),entries.end(),[&](auto &e){return e.maps.get()==maps.get();});
    if(found!=entries.end()){entries.splice(entries.begin(),entries,found);return;}
    Entry entry;entry.maps=maps;entry.token=token;entry.factors=factors;entry.derived=maps->color.rgba.size()+maps->metal_rough.rgba.size()+maps->emission.rgba.size()+maps->normal.rgba.size()+maps->mtoon_normal.rgba.size()+maps->color_coverage.rgba.size()+maps->baked_normal.rgba.size()+maps->baked_coverage.rgba.size()+maps->emission_coverage.rgba.size()+maps->pbr_color.rgba.size();
    entry.base=maps->baked_base.get();
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
    while(!entries.empty()&&(entries.size()>=128||increment()>limit||bytes>limit-increment()))evict();
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
    result[7 + c] = srgb_to_linear(unit(m.emission.rgba[i + c]));
    result[10 + c] = m.normal.rgba.empty() ? (c == 2 ? 1 : 0)
                                           : unit(m.normal.rgba[i + c]) * 2 - 1;
  }
  return result;
}
void baked_pixel(BakedTextures &baked, size_t at, BakedPixel values) {
  auto &m = baked.maps;
  size_t i = at * 4;
  for (int c = 0; c < 4; ++c)
    m.color.rgba[i + c] = byte(c < 3 ? linear_to_srgb(values[c]) : values[c]);
  for (int c = 0; c < 3; ++c) {
    m.metal_rough.rgba[i + c] = byte(values[4 + c]);
    m.emission.rgba[i + c] = byte(linear_to_srgb(values[7 + c]));
  }
  m.metal_rough.rgba[i + 3] = m.emission.rgba[i + 3] = 255;
  if (!m.normal.rgba.empty()) {
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
  std::shared_ptr<const TextureMaps> source_preview;
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
      !std::all_of(emission.begin(), emission.end(), valid) ||
      !valid(metallic) || !valid(roughness) || !valid(mask) || !valid(flow))
    throw std::invalid_argument("Invalid paint brush settings");
}
struct PaintedMesh::SurfaceMap {
  Mesh mesh;
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
  PaintSurfaceOptions options;
  PaintSurfaceDiagnostics diagnostics;
  std::unordered_map<uint32_t,Head> linked;
  std::vector<Extra> extra;
  std::vector<Texel> texels;
  std::vector<Bounds> tiles;
  std::vector<int32_t> padding;
  SurfaceMap(Mesh input, uint32_t r, PaintSurfaceOptions opts = {})
      : mesh(std::move(input)), resolution(r), columns(r / 32),options(std::move(opts)),
        texels(size_t(r) * r), tiles(size_t(columns) * columns),
        padding(size_t(r) * r, -1) {
    if((options.policy!=PaintUvPolicy::Strict&&options.policy!=PaintUvPolicy::SourceAuthored&&options.policy!=PaintUvPolicy::PolygonAuthored)||
       !options.max_raster_samples||options.max_raster_samples>512*1024*1024||
       options.max_additional_candidates>8*1024*1024||!options.max_candidate_checks||options.max_candidate_checks>512*1024*1024||
       options.max_candidates_per_texel<1||options.max_candidates_per_texel>128)
      throw std::invalid_argument("Invalid paint surface resource bounds");
    diagnostics.policy=options.policy;
    const bool source=options.policy!=PaintUvPolicy::Strict;
    mesh.validate();
    if (mesh.uv.size() != mesh.positions.size())
      throw std::invalid_argument("Painting requires persisted UV coordinates");
    for (auto uv : mesh.uv)
      if (uv[0] < 0 || uv[0] > 1 || uv[1] < 0 || uv[1] > 1)
        throw std::invalid_argument("Paint UVs must fit a single texture tile");
    for (size_t k = 0; k < mesh.indices.size(); k += 3) {
      uint32_t ids[] = {mesh.indices[k], mesh.indices[k + 1],
                        mesh.indices[k + 2]};
      auto a = mesh.uv[ids[0]], b = mesh.uv[ids[1]], c = mesh.uv[ids[2]];
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
          auto p = mesh.positions[ids[0]] * w0 + mesh.positions[ids[1]] * w1 +
                   mesh.positions[ids[2]] * w2;
          size_t index = size_t(y) * r + x;
          auto &t = texels[index];
          Texel candidate{p,normalized(mesh.normals[ids[0]]*w0+mesh.normals[ids[1]]*w1+mesh.normals[ids[2]]*w2),true,uint32_t(k/3),{w0,w1}};
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
    if (!source && std::none_of(texels.begin(), texels.end(),
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
  }
};
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
PaintedMesh::PaintedMesh(Mesh mesh,uint32_t r,const PaintSurfaceOptions &options) {
  if (r < 64 || r > 2048 || (r & (r - 1)))
    throw std::invalid_argument(
        "Paint resolution must be a power of two between 64 and 2048");
  surface_ = std::make_shared<SurfaceMap>(std::move(mesh), r,options);
  composite_cache_=std::make_shared<CompositeCache>();
  composite_token_=next_composite_token();
  PaintLayer layer;
  layer.id = 1;
  layer.tiles.resize(surface_->tiles.size());
  layers_.push_back(std::move(layer));
}
const Mesh &PaintedMesh::mesh() const { return surface_->mesh; }
std::shared_ptr<const Mesh> PaintedMesh::mesh_owner() const { return {surface_, &surface_->mesh}; }
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
void PaintedMesh::begin_stroke() {
  if (editing_)
    throw std::logic_error("A paint stroke is already active");
  if (!active().visible)
    throw std::logic_error("Show the active paint layer before painting");
  before_ = active().tiles;
  before_colors_ = active().pbr_tiles;
  before_allocated_tiles_ = tile_count(active());
  before_allocated_color_tiles_ = color_tile_count(active());
  before_composite_token_=composite_token_;
  editing_ = true;
}
bool PaintedMesh::pending_changes() const {
  return editing_ && (before_ != layer(active_).tiles || before_colors_ != layer(active_).pbr_tiles);
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
  size_t allocated = allocated_tiles_;
  size_t allocated_colors = allocated_color_tiles_;
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
      bool color_changed=false;
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
            auto evaluated=evaluation?evaluation->sample(candidate):std::pair{candidate.position,candidate.normal};
            auto position=evaluated.first,normal=evaluated.second;
            if(settings.front_only&&dot(normal,n)<=.05f)return;
            float u=axis2>1e-20f?std::clamp(dot(position-b.from,axis)/axis2,0.f,1.f):0;
            float d=length(position-(b.from+axis*u));
            if(d>=radius||d>distance+1e-7f||(std::abs(d-distance)<=1e-7f&&candidate.face>=selected_face))return;
            auto value=settings;float opacity=1;
            if(sampler){auto sample=sampler(candidate.position,candidate.normal);if(!sample)return;value=sample->settings;opacity=sample->opacity;}
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
            float previous = unit(p.data[coverage]);
            if (selected.erase)
              p.data[coverage] = byte(previous * (1 - alpha));
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
              else
                for (int k = 0; k < 3; ++k)
                  mix(6 + k, selected.emission[k], true);
            }
          }
          tile_changed |= p != old;
        }
      if (tile_changed || color_changed) {
        if(!l.tiles[tile])++allocated;
        if(color_changed && !l.pbr_tiles[tile])++allocated_colors;
        if(allocated*sizeof(PaintTile)+allocated_colors*sizeof(PaintColorTile)+
            (baked_?baked_->storage_bytes():0)+(color_projection_?color_projection_->vertex_rgba->rgba.size():0)+(normal_projection_?normal_projection_->pbr->rgba.size()+normal_projection_->mtoon->rgba.size():0)>128*1024*1024)
          throw std::length_error("Paint storage budget exceeded");
        l.tiles[tile] = std::move(copy);
        if(color_changed)l.pbr_tiles[tile]=std::move(color_copy);
        changed = true;
      }
    }
    stop();
  } catch (...) {
    l.tiles = std::move(prior);
    l.pbr_tiles = std::move(prior_colors);
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
  if (before_ == tiles && before_colors_ == colors) {
    before_.clear();
    before_colors_.clear();
    return false;
  }
  size_t bytes = 0;
  for (size_t i = 0; i < tiles.size(); ++i)
    if (before_[i] != tiles[i])
      bytes += sizeof(PaintTile) * 2;
  for(size_t i=0;i<colors.size();++i)if(before_colors_[i]!=colors[i])bytes+=sizeof(PaintColorTile)*2;
  for (auto &s : redo_)
    history_bytes_ -= s.bytes;
  redo_.clear();
  undo_.push_back({active_, std::move(before_), tiles, bytes,before_allocated_tiles_,tile_count(active()),std::move(before_colors_),colors,before_allocated_color_tiles_,color_tile_count(active())});
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
  undo_.push_back(std::move(s));
  composite_token_=token;
  ++revision_;
  return true;
}
std::shared_ptr<PaintedMesh> PaintedMesh::snapshot() const {
  auto p = std::shared_ptr<PaintedMesh>(new PaintedMesh);
  p->composite_cache_=composite_cache_;
  p->composite_token_=composite_token_;
  p->surface_ = surface_;
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
struct PolygonPaintAccess {
  static std::shared_ptr<PaintedMesh> rebuild(const PaintedMesh &source, Mesh target,
                                              const PaintSurfaceOptions &options) {
    if(source.editing_)throw std::logic_error("Finish the paint stroke before rebuilding polygon geometry");
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
  if(version!=1||!pbr||!mtoon)throw std::invalid_argument("Prepared normal projection is incomplete");
  for(const auto &image:{pbr,mtoon}){image->validate();if(image->width!=resolution||image->height!=resolution)throw std::invalid_argument("Prepared normal projection grid differs from paint");}
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
void BakedTextures::validate(uint32_t resolution) const {
  if (maps.source_maps || maps.source_only || maps.baked_base || maps.color_projection || maps.pbr_transparent || !maps.emission_coverage.rgba.empty() || maps.emission_coverage.width || maps.emission_coverage.height || !maps.color_coverage.rgba.empty() || maps.color_coverage.width || maps.color_coverage.height || !maps.baked_normal.rgba.empty() || maps.baked_normal.width || maps.baked_normal.height || !maps.baked_coverage.rgba.empty() || maps.baked_coverage.width || maps.baked_coverage.height) throw std::invalid_argument("Attach original maps through from_maps, not a baked payload");
  if(size_t(color_interpretation)>1 || (color_interpretation==BakedColorInterpretation::ResolvedPbr)!=!maps.pbr_color.rgba.empty())
    throw std::invalid_argument("Baked PBR color interpretation is incomplete");
  if(size_t(normal_interpretation)>1||maps.normal_projection||
     (normal_interpretation==BakedNormalInterpretation::CanonicalMikk&&!maps.mtoon_normal.rgba.empty())||
     (normal_interpretation==BakedNormalInterpretation::PreparedAttributes&&maps.normal.rgba.empty()!=maps.mtoon_normal.rgba.empty()))
    throw std::invalid_argument("Baked normal interpretation is incomplete");
  if (resolution < 64 || resolution > 2048 ||
      coverage.size() != size_t(resolution) * resolution)
    throw std::invalid_argument("Baked coverage differs from the painting surface");
  for (auto image : {&maps.color, &maps.metal_rough, &maps.emission, &maps.normal, &maps.pbr_color,&maps.mtoon_normal}) {
    if ((image == &maps.normal || image == &maps.pbr_color||image==&maps.mtoon_normal) && image->rgba.empty() && !image->width && !image->height)
      continue;
    image->validate();
    if (image->width != resolution || image->height != resolution)
      throw std::invalid_argument("Baked image dimensions differ from the painting surface");
  }
}
size_t BakedTextures::storage_bytes() const {
  return maps.color.rgba.size() + maps.metal_rough.rgba.size() +
         maps.emission.rgba.size() + maps.normal.rgba.size() + maps.mtoon_normal.rgba.size()+maps.pbr_color.rgba.size() + coverage.size();
}
size_t PaintedMesh::storage_bytes() const {
  size_t result = (baked_ ? baked_->storage_bytes() : 0)+(color_projection_?color_projection_->vertex_rgba->rgba.size():0);
  if(normal_projection_)result+=normal_projection_->pbr->rgba.size()+normal_projection_->mtoon->rgba.size();
  for (const auto &layer : layers_)
    for (const auto &tile : layer.tiles)
      if (tile) result += sizeof(PaintTile);
  for(const auto &layer:layers_)result+=color_tile_count(layer)*sizeof(PaintColorTile);
  return result;
}
void PaintedMesh::install_bake(BakedTextures incoming) {
  if (editing_) throw std::logic_error("Finish the paint stroke before baking");
  incoming.validate(resolution());
  if(bool(color_projection_)!=(incoming.color_interpretation==BakedColorInterpretation::ResolvedPbr))
    throw std::invalid_argument("Baked color interpretation must match explicit current color preparation");
  if(bool(normal_projection_)!=(incoming.normal_interpretation==BakedNormalInterpretation::PreparedAttributes))
    throw std::invalid_argument("Baked normal interpretation must match explicit normal preparation");
  auto token = next_composite_token();
  if (baked_ && !baked_->maps.normal.rgba.empty() && incoming.maps.normal.rgba.empty())
    throw std::invalid_argument("A replacement bake must retain the existing normal channel");
  if (storage_bytes() - (baked_ ? baked_->storage_bytes() : 0) +
          incoming.storage_bytes() > 128 * 1024 * 1024)
    throw std::length_error("Baked maps exceed paint storage budget");
  if (baked_) {
    if(baked_->color_interpretation!=incoming.color_interpretation)
      throw std::invalid_argument("Prepare old baked color authority before combining different interpretations");
    if(baked_->normal_interpretation!=incoming.normal_interpretation)
      throw std::invalid_argument("Different normal interpretations require explicit source-retaining conversion");
    auto merged = *baked_;
    if (merged.maps.normal.rgba.empty() && !incoming.maps.normal.rgba.empty())
      merged.maps.normal = blank_image(resolution(), true);
    if(merged.normal_interpretation==BakedNormalInterpretation::PreparedAttributes&&merged.maps.mtoon_normal.rgba.empty()&&!incoming.maps.mtoon_normal.rgba.empty())merged.maps.mtoon_normal=blank_image(resolution(),true);
    for (size_t i = 0; i < incoming.coverage.size(); ++i) {
      float a = unit(incoming.coverage[i]), b = unit(baked_->coverage[i]);
      if (!a) continue;
      float coverage = a + b * (1 - a);
      auto next = baked_pixel(incoming, i), prior = baked_pixel(*baked_, i);
      if(incoming.color_interpretation==BakedColorInterpretation::ResolvedPbr)for(int c=0;c<4;++c) {
        const auto at=i*4+c;
        float x=unit(incoming.maps.pbr_color.rgba[at]),y=unit(baked_->maps.pbr_color.rgba[at]);
        if(c<3){x=srgb_to_linear(x);y=srgb_to_linear(y);}
        const float value=(x*a+y*b*(1-a))/coverage;
        merged.maps.pbr_color.rgba[at]=byte(c<3?linear_to_srgb(value):value);
      }
      for (int c = 0; c < 13; ++c) {
        next[c] = (next[c] * a + prior[c] * b * (1 - a)) / coverage;
      }
      baked_pixel(merged, i, next);
      if(incoming.normal_interpretation==BakedNormalInterpretation::PreparedAttributes&&!incoming.maps.mtoon_normal.rgba.empty())
        for(size_t c=0;c<3;++c){const auto at=i*4+c;const float x=unit(incoming.maps.mtoon_normal.rgba[at])*2-1,y=unit(merged.maps.mtoon_normal.rgba[at])*2-1;
          merged.maps.mtoon_normal.rgba[at]=byte(((x*a+y*b*(1-a))/coverage)*.5f+.5f);
        }
      merged.coverage[i] = byte(coverage);
    }
    incoming = std::move(merged);
  }
  if (storage_bytes() - (baked_ ? baked_->storage_bytes() : 0) +
          incoming.storage_bytes() > 128 * 1024 * 1024)
    throw std::length_error("Baked maps exceed paint storage budget");
  baked_ = std::make_shared<const BakedTextures>(std::move(incoming));
  composite_token_ = token;
  ++revision_;
}
void PaintedMesh::clear_bake() {
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
  maps->validate();
  auto result=std::shared_ptr<PaintedMesh>(new PaintedMesh(std::move(mesh),resolution,options));
  // Freeze the small bundle of references/metadata. The shared image pixels must
  // already be immutable, allowing one original image cache across materials.
  result->source_maps_=std::make_shared<const PaintSourceMaps>(*maps);
  return result;
}
std::shared_ptr<const TextureMaps> PaintedMesh::preview(const Material &material) const {
  material.validate();
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
      composite_cache_->source_eligible=!contributing;composite_cache_->preview_token=composite_token_;
    }
    if(composite_cache_->source_eligible){
      if(!composite_cache_->source_preview){
        auto marker=std::make_shared<TextureMaps>();marker->source_maps=source_maps_;marker->source_only=true;
        if(auto image=source_maps_->color.image)for(size_t i=3;i<image->rgba.size();i+=4)if(image->rgba[i]!=255){marker->transparent=true;break;}
        composite_cache_->source_preview=std::move(marker);
      }
      return composite_cache_->source_preview;
    }
  }
  return composite(material);
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
  const auto before=source.preview(material),after=result->composite(material);
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
PaintedMesh::composite(const Material &material) const {
  material.validate();
  CompositeFactors factors{material.color,material.emissive,material.metallic,material.roughness};
  std::lock_guard lock(composite_cache_->mutex);
  if(composite_cache_->token==composite_token_&&composite_cache_->factors==factors)
    if(auto maps=composite_cache_->maps.lock()){composite_retention().retain(maps,composite_token_,factors,true);return maps;}
  auto maps=composite_retention().find(composite_token_,factors);
  if(maps){composite_cache_->token=composite_token_;composite_cache_->factors=factors;composite_cache_->maps=maps;return maps;}
  maps=build_composite(material);
  composite_cache_->token=composite_token_;composite_cache_->factors=factors;composite_cache_->maps=maps;
  composite_retention().retain(maps,composite_token_,factors,false);return maps;
}
std::shared_ptr<const TextureMaps>
PaintedMesh::build_composite(const Material &material) const {
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
  if(normal_projection_) {
    normal_projection_->validate(r);
    maps->normal=*normal_projection_->pbr;maps->mtoon_normal=*normal_projection_->mtoon;
  } else if (baked_ && !baked_->maps.normal.rgba.empty()) {
    maps->normal=blank_image(r,true);maps->baked_normal=blank_image(r,true);
    maps->baked_coverage=blank_image(r);for(size_t at=3;at<maps->baked_coverage.rgba.size();at+=4)maps->baked_coverage.rgba[at]=255;
  }

  for(int channel=0;channel<3;++channel){
    auto &image=channel==0?maps->color:channel==1?maps->metal_rough:maps->emission;
    for(uint32_t y=0;y<image.height;++y)for(uint32_t x=0;x<image.width;++x){
      float u=(float(x)+.5f)/image.width,v=(float(y)+.5f)/image.height;
      auto sampled=sample_source_texture(channel==0?source.color:channel==1?source.metal_rough:source.emission,u,v,channel!=1);
      std::array<float,4> value{};
      if(channel==0)for(int k=0;k<4;++k)value[k]=material.color[k]*sampled[k];
      else if(channel==1){
        float ao=source.occlusion.image?sample_source_texture(source.occlusion,u,v,false)[0]:1;
        value={1-source.occlusion_strength*(1-ao),material.roughness*sampled[1],material.metallic*sampled[2],1};
      }else{for(int k=0;k<3;++k)value[k]=material.emissive[k]*sampled[k];value[3]=1;}
      auto pbr_value=value;
      if(channel==0 && color_projection_) {
        const auto vertex=sample_source_texture(projected_vertex_color,u,v,false);
        for(int k=0;k<4;++k)pbr_value[k]*=vertex[k];
      }
      uint32_t tx=std::min(r-1,uint32_t(u*r)),ty=std::min(r-1,uint32_t(v*r));
      int32_t owner=surface_->padding[size_t(ty)*r+tx];
      uint32_t px=owner>=0?uint32_t(owner)%r:tx,py=owner>=0?uint32_t(owner)/r:ty;
      float replacement_coverage=0;
      if (baked_ && owner >= 0) {
        auto values = baked_pixel(*baked_, size_t(owner));
        float a = unit(baked_->coverage[size_t(owner)]);
        replacement_coverage=a;
        if (channel == 0) for (int k=0;k<4;++k) value[k] = value[k]*(1-a)+values[k]*a;
        else if (channel == 1) for (int k=0;k<3;++k) value[k] = value[k]*(1-a)+values[4+k]*a;
        else for (int k=0;k<3;++k) value[k] = value[k]*(1-a)+values[7+k]*a;
        if(channel==0 && color_projection_) {
          if(baked_->color_interpretation!=BakedColorInterpretation::ResolvedPbr)throw std::logic_error("Prepared surface contains unresolved baked colors");
          for(int k=0;k<4;++k) {
            float baked=unit(baked_->maps.pbr_color.rgba[size_t(owner)*4+k]);
            if(k<3)baked=srgb_to_linear(baked);
            pbr_value[k]=pbr_value[k]*(1-a)+baked*a;
          }
        }
      }

      for(auto &l:layers_)if(l.visible&&l.opacity>0&&owner>=0){
        auto &p=pixel(l.id,px,py);float mask=unit(p.data[13])*l.opacity;
        float weight=mask*unit(p.data[channel==2?12:9]);replacement_coverage=replacement_coverage*(1-weight)+weight;
        auto blend=[&](float base,int data,int coverage,bool srgb){
          float a=mask*unit(p.data[coverage]),paint=unit(p.data[data]);
          if(srgb)paint=srgb_to_linear(paint);
          return base*(1-a)+paint*a;
        };
        if(channel==0)for(int k=0;k<4;++k)value[k]=blend(value[k],k,9,k<3);
        else if(channel==1){value[1]=blend(value[1],5,11,false);value[2]=blend(value[2],4,10,false);}
        else for(int k=0;k<3;++k)value[k]=blend(value[k],6+k,12,true);
        if(channel==0 && color_projection_) {
          auto painted=pbr_pixel(l.id,px,py);
          const float a=mask*unit(p.data[9]);
          for(int k=0;k<4;++k) {
            float sample=unit(painted[k]);if(k<3)sample=srgb_to_linear(sample);
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
  if(normal_projection_ && baked_ && !baked_->maps.normal.rgba.empty()) {
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
  } else if (baked_ && !maps->normal.rgba.empty()) {
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
      int32_t owner=surface_->padding[at];
      if (owner<0) continue;
      auto values=baked_pixel(*baked_,size_t(owner));
      float a=unit(baked_->coverage[size_t(owner)]);
      maps->baked_coverage.rgba[at*4]=byte(a);
      for(int c=0;c<4;++c)maps->baked_normal.rgba[at*4+c]=baked_->maps.normal.rgba[size_t(owner)*4+c];
      Vec3 original{0,0,1};
      if(a<1 && !corners.empty()){
        const auto &texel=surface_->texels[size_t(owner)];
        std::array<float,3> bary{texel.bary[0],texel.bary[1],1-texel.bary[0]-texel.bary[1]};
        original=tangent_frame(mesh(),corners,texel.face,bary).to_tangent(
          source_texture_normal(mesh(),source,texel.face,bary));
      }
      Vec3 n=Vec3{values[10],values[11],values[12]}*a+original*(1-a);
      if(length(n)<1e-8f)n={0,0,1};else n=normalized(n);
      maps->normal.rgba[at*4]=byte(n.x*.5f+.5f);
      maps->normal.rgba[at*4+1]=byte(n.y*.5f+.5f);
      maps->normal.rgba[at*4+2]=byte(n.z*.5f+.5f);
    }
  }
  return maps;
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
    auto surface = std::make_shared<SurfaceMap>(*surface_);
    surface->mesh = std::move(target);
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
  if(normal_projection_)j["normal_projection"]=encode_normal_projection(*normal_projection_,archive?*archive:local);
  if(!archive && (source_maps_ || color_projection_ || normal_projection_))j["source_images"]=local.encode();
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
  }
  if (baked_) {
    j["baked"] = {{"version", 1},
                    {"color", nlohmann::json::binary(baked_->maps.color.rgba)},
                    {"orm", nlohmann::json::binary(baked_->maps.metal_rough.rgba)},
                    {"emission", nlohmann::json::binary(baked_->maps.emission.rgba)},
                    {"normal", nlohmann::json::binary(baked_->maps.normal.rgba)},
                    {"coverage", nlohmann::json::binary(baked_->coverage)}};
    if(baked_->color_interpretation==BakedColorInterpretation::ResolvedPbr) {
      j["baked"]["version"]=2;j["baked"]["color_interpretation"]="resolved-pbr";
      j["baked"]["pbr_color"]=nlohmann::json::binary(baked_->maps.pbr_color.rgba);
    }
    if(baked_->normal_interpretation==BakedNormalInterpretation::PreparedAttributes){
      j["baked"]["version"]=3;j["baked"]["normal_interpretation"]="prepared-attributes-v1";
      j["baked"]["mtoon_normal"]=nlohmann::json::binary(baked_->maps.mtoon_normal.rgba);
    }
  }
  return j;
}
std::shared_ptr<PaintedMesh> PaintedMesh::decode(const nlohmann::json &j,const PaintImageArchive *archive) {
  return decode(j,archive,false);
}
std::shared_ptr<PaintedMesh> PaintedMesh::decode(const nlohmann::json &j,const PaintImageArchive *archive,bool allow_polygon) {
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
      if(!policy.is_string()||policy.get_ref<const std::string&>()!="source-authored"||(!j.contains("source_maps")&&!j.contains("color_projection")&&!j.contains("normal_projection")))throw std::runtime_error("Invalid persisted source UV policy");
      surface_options.policy=PaintUvPolicy::SourceAuthored;
    }
  }
  auto result=std::shared_ptr<PaintedMesh>(new PaintedMesh(std::move(mesh),uint32_t(resolution.get<uint64_t>()),surface_options));
  std::optional<PaintImageArchive> local_images;
  const PaintImageArchive *images=archive;
  if(j.contains("source_maps") || j.contains("color_projection") || j.contains("normal_projection")) {
    if(archive && j.contains("source_images"))throw std::runtime_error("Ambiguous original texture archive");
    if(!archive){local_images=PaintImageArchive::decode(j.at("source_images"));images=&*local_images;}
    if(j.contains("source_maps"))result->source_maps_=decode_source_maps(j.at("source_maps"),*images);
    if(j.contains("normal_projection")){
      result->normal_projection_=decode_normal_projection(j.at("normal_projection"),*images);
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
    result->layers_.push_back(std::move(l));
  }
  result->layer(result->active_);
  if (j.contains("baked")) {
    const auto &v = j.at("baked");
    if(!v.at("version").is_number_integer() || (v.at("version")!=1 && v.at("version")!=2 && v.at("version")!=3))throw std::runtime_error("Unsupported baked texture version");
    if(v.at("version")==1 && (v.contains("color_interpretation") || v.contains("pbr_color")))throw std::runtime_error("Resolved baked colors require baked payload2");
    uint32_t r = result->resolution();
    auto bytes = [&](const char *key, size_t count, bool optional = false) {
      const auto &value = v.at(key);
      if (!value.is_binary() ||
          (value.get_binary().size() != count && !(optional && value.get_binary().empty())))
        throw std::runtime_error("Invalid persisted baked image or coverage buffer");
      return std::vector<uint8_t>(value.get_binary().begin(), value.get_binary().end());
    };
    BakedTextures baked;
    baked.maps.color = {r, r, bytes("color", size_t(r)*r*4)};
    baked.maps.metal_rough = {r, r, bytes("orm", size_t(r)*r*4)};
    baked.maps.emission = {r, r, bytes("emission", size_t(r)*r*4)};
    auto normal = bytes("normal", size_t(r)*r*4, true);
    if (!normal.empty()) baked.maps.normal = {r, r, std::move(normal)};
    baked.coverage = bytes("coverage", size_t(r)*r);
    if(v.at("version")!=3&&(v.contains("normal_interpretation")||v.contains("mtoon_normal")))throw std::runtime_error("Prepared normals require baked payload3");
    if(v.at("version")==2||(v.at("version")==3&&(v.contains("color_interpretation")||v.contains("pbr_color")))) {
      if(v.at("color_interpretation")!="resolved-pbr")throw std::runtime_error("Unknown baked color interpretation");
      baked.color_interpretation=BakedColorInterpretation::ResolvedPbr;baked.maps.pbr_color={r,r,bytes("pbr_color",size_t(r)*r*4)};
    }
    if(v.at("version")==3){
      const auto &kind=v.at("normal_interpretation");
      if(!kind.is_string()||kind.get_ref<const std::string&>()!="prepared-attributes-v1")throw std::runtime_error("Unknown baked normal interpretation");
      baked.normal_interpretation=BakedNormalInterpretation::PreparedAttributes;
      auto normal=bytes("mtoon_normal",size_t(r)*r*4,true);if(!normal.empty())baked.maps.mtoon_normal={r,r,std::move(normal)};
    }
    result->install_bake(std::move(baked));
  }
  result->allocated_tiles_ = 0;
  for(const auto &layer:result->layers_)result->allocated_tiles_+=tile_count(layer);
  if(result->storage_bytes()>128*1024*1024)throw std::length_error("Persisted paint exceeds total storage budget");
  return result;
}
void PaintedMesh::swap_state(PaintedMesh &other) noexcept {
  using std::swap;
  static_assert(noexcept(swap(composite_cache_,other.composite_cache_)));swap(composite_cache_,other.composite_cache_);
  static_assert(noexcept(swap(composite_token_,other.composite_token_)));swap(composite_token_,other.composite_token_);
  static_assert(noexcept(swap(before_composite_token_,other.before_composite_token_)));swap(before_composite_token_,other.before_composite_token_);
  static_assert(noexcept(swap(surface_,other.surface_)));swap(surface_,other.surface_);
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
  static_assert(noexcept(swap(undo_,other.undo_)));swap(undo_,other.undo_);
  static_assert(noexcept(swap(redo_,other.redo_)));swap(redo_,other.redo_);
  static_assert(noexcept(swap(history_bytes_,other.history_bytes_)));swap(history_bytes_,other.history_bytes_);
  static_assert(noexcept(swap(allocated_tiles_,other.allocated_tiles_)));swap(allocated_tiles_,other.allocated_tiles_);
  static_assert(noexcept(swap(before_allocated_tiles_,other.before_allocated_tiles_)));swap(before_allocated_tiles_,other.before_allocated_tiles_);
}
} // namespace dome
