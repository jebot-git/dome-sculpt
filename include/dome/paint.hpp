// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mesh.hpp"
#include <deque>
#include <map>
#include <memory>
namespace dome {
struct Material;
class PaintProjection;
struct PaintSourceMaps;
class PaintImageArchive;
struct Image {
  uint32_t width = 0, height = 0;
  std::vector<uint8_t> rgba;
  void validate() const;
  std::vector<uint8_t> png() const;
};
struct TextureMaps {
  Image color, metal_rough, emission;
  bool transparent = false;
  // Original normal/AO views and exact source metadata survive compositing.
  std::shared_ptr<const PaintSourceMaps> source_maps;
  // Preview-only immutable original bindings; derived Images are intentionally empty.
  bool source_only = false;
  // Linear R: combined visible color coverage; 1x1 zero for untouched sources.
  Image color_coverage;
};
struct PaintCompositeCacheStats {
  size_t retained_bytes = 0, retained_maps = 0, limit_bytes = 0;
  uint64_t hits = 0, builds = 0;
};
PaintCompositeCacheStats paint_composite_cache_stats();
// Global strong retention only; zero disables it without invalidating readers.
void set_paint_composite_cache_budget(size_t bytes);
enum class PaintChannel : int { Color, Metallic, Roughness, Emission, Mask };
struct PaintSettings {
  PaintChannel channel = PaintChannel::Color;
  std::array<float, 4> color{.8f, .03f, .02f, 1};
  std::array<float, 3> emission{.8f, .1f, .02f};
  float metallic = 1, roughness = .25f, mask = 0, flow = 1;
  bool erase = false, front_only = true;
  void validate() const;
};
struct PaintPixel {
  // sRGB color RGBA (alpha linear), metal, roughness, sRGB emission,
  // separate coverage for color/metal/rough/emission, then a linear mask.
  std::array<uint8_t, 14> data{};
  PaintPixel() { data[13] = 255; }
  auto operator<=>(const PaintPixel &) const = default;
};
struct PaintTile {
  static constexpr uint32_t size = 32;
  std::array<PaintPixel, size * size> pixels;
};
struct PaintLayer {
  uint64_t id = 0;
  std::string name = "Paint";
  bool visible = true;
  float opacity = 1;
  std::vector<std::shared_ptr<const PaintTile>> tiles;
};
enum class PaintUvPolicy { Strict, SourceAuthored };
struct PaintSurfaceOptions {
  PaintUvPolicy policy = PaintUvPolicy::Strict;
  size_t max_raster_samples = 128 * 1024 * 1024;
  size_t max_additional_candidates = 4 * 1024 * 1024;
  size_t max_candidate_checks = 128 * 1024 * 1024;
  uint32_t max_candidates_per_texel = 64;
  std::function<bool()> cancelled;
};
struct PaintSurfaceDiagnostics {
  PaintUvPolicy policy = PaintUvPolicy::Strict;
  std::vector<uint32_t> unpaintable_uv_faces;
  size_t linked_texels = 0, additional_candidates = 0, covered_texels = 0;
  size_t raster_samples = 0, candidate_checks = 0;
};
class PaintedMesh {
public:
  PaintedMesh(Mesh, uint32_t resolution = 512);
  static std::shared_ptr<PaintedMesh> from_maps(Mesh, std::shared_ptr<const PaintSourceMaps>, uint32_t resolution = 512);
  static std::shared_ptr<PaintedMesh> from_maps(Mesh, std::shared_ptr<const PaintSourceMaps>, uint32_t resolution, const PaintSurfaceOptions &);
  const PaintSurfaceDiagnostics &surface_diagnostics() const;
  const std::shared_ptr<const PaintSourceMaps> &source_maps() const noexcept { return source_maps_; }
  const Mesh &mesh() const;
  uint32_t resolution() const;
  uint64_t revision() const { return revision_; }
  const std::vector<PaintLayer> &layers() const { return layers_; }
  uint64_t active_layer() const { return active_; }
  void select_layer(uint64_t);
  void add_layer(std::string name = "Paint");
  void delete_layer();
  void layer_properties(std::string, bool visible, float opacity);
  void move_layer(int direction);
  void begin_stroke();
  void stamp(const Brush &, const PaintSettings &);
  void stamp_projection(const Brush &, const PaintSettings &, const PaintProjection &);
  bool end_stroke();
  void cancel_stroke();
  bool in_stroke() const { return editing_; }
  bool pending_changes() const;
  size_t undo_count() const { return undo_.size(); }
  // Conservative retained data, including hidden/zero-opacity/mask-only tiles.
  bool has_paint_data() const noexcept { return allocated_tiles_ != 0 || bool(source_maps_); }
  bool undo();
  bool redo();
  std::shared_ptr<PaintedMesh> snapshot() const;
  std::shared_ptr<PaintedMesh>
  reparameterize(Mesh, const std::function<bool()> &cancelled = {}) const;
  std::shared_ptr<const TextureMaps> composite(const Material &) const;
  std::shared_ptr<const TextureMaps> preview(const Material &) const;
  const PaintPixel &pixel(uint64_t layer, uint32_t x, uint32_t y) const;
  std::optional<Vec3> raycast(Vec3, Vec3, float maximum = 20) const;
  Vec3 normal(Vec3) const;
  nlohmann::json encode(PaintImageArchive * = nullptr) const;
  static std::shared_ptr<PaintedMesh> decode(const nlohmann::json &, const PaintImageArchive * = nullptr);

private:
  PaintedMesh() = default;
  PaintedMesh(Mesh, uint32_t, const PaintSurfaceOptions &);
  struct PaintSample { PaintSettings settings; float opacity = 1; };
  using PaintSampler = std::function<std::optional<PaintSample>(Vec3,Vec3)>;
  void stamp_sampled(const Brush &, const PaintSettings &, const PaintSampler &);
  struct SurfaceMap;
  struct CompositeCache;
  std::shared_ptr<const TextureMaps> build_composite(const Material &) const;
  std::shared_ptr<CompositeCache> composite_cache_;
  uint64_t composite_token_ = 0, before_composite_token_ = 0;
  std::shared_ptr<const SurfaceMap> surface_;
  std::shared_ptr<const PaintSourceMaps> source_maps_;
  std::vector<PaintLayer> layers_;
  uint64_t active_ = 1, next_ = 2, revision_ = 1;
  bool editing_ = false;
  using Tiles = std::vector<std::shared_ptr<const PaintTile>>;
  struct Stroke {
    uint64_t layer;
    Tiles before, after;
    size_t bytes;
    size_t before_tiles = 0, after_tiles = 0;
  };
  Tiles before_;
  std::deque<Stroke> undo_, redo_;
  size_t history_bytes_ = 0;
  PaintLayer &active();
  const PaintLayer &layer(uint64_t) const;
  size_t allocated_tiles_ = 0, before_allocated_tiles_ = 0;
};
float srgb_to_linear(float);
float linear_to_srgb(float);
} // namespace dome
