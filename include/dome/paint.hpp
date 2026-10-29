// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "mesh.hpp"
#include <deque>
#include <map>
#include <memory>
namespace dome {
struct Material;
struct Image {
  uint32_t width = 0, height = 0;
  std::vector<uint8_t> rgba;
  void validate() const;
  std::vector<uint8_t> png() const;
};
struct TextureMaps {
  Image color, metal_rough, emission;
  bool transparent = false;
  Image normal; // Optional linear +Y tangent-space map. Empty means no detail.
};
struct BakedTextures {
  TextureMaps maps;
  // Linear coverage below editable paint layers: 0 intrinsic material,
  // 255 baked surface. Fractional values retain filtered UV boundary coverage.
  std::vector<uint8_t> coverage;
  void validate(uint32_t resolution) const;
  size_t storage_bytes() const;
};
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
class PaintedMesh {
public:
  PaintedMesh(Mesh, uint32_t resolution = 512);
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
  bool end_stroke();
  void cancel_stroke();
  bool in_stroke() const { return editing_; }
  bool pending_changes() const;
  size_t undo_count() const { return undo_.size(); }
  bool undo();
  bool redo();
  std::shared_ptr<PaintedMesh> snapshot() const;
  std::shared_ptr<PaintedMesh>
  reparameterize(Mesh, const std::function<bool()> &cancelled = {}) const;
  std::shared_ptr<const TextureMaps> composite(const Material &) const;
  std::shared_ptr<const BakedTextures> baked() const { return baked_; }
  void install_bake(BakedTextures);
  void clear_bake();
  size_t storage_bytes() const;
  const PaintPixel &pixel(uint64_t layer, uint32_t x, uint32_t y) const;
  std::optional<Vec3> raycast(Vec3, Vec3, float maximum = 20) const;
  Vec3 normal(Vec3) const;
  nlohmann::json encode() const;
  static std::shared_ptr<PaintedMesh> decode(const nlohmann::json &);

private:
  PaintedMesh() = default;
  struct SurfaceMap;
  std::shared_ptr<const SurfaceMap> surface_;
  std::vector<PaintLayer> layers_;
  std::shared_ptr<const BakedTextures> baked_;
  uint64_t active_ = 1, next_ = 2, revision_ = 1;
  bool editing_ = false;
  using Tiles = std::vector<std::shared_ptr<const PaintTile>>;
  struct Stroke {
    uint64_t layer;
    Tiles before, after;
    size_t bytes;
  };
  Tiles before_;
  std::deque<Stroke> undo_, redo_;
  size_t history_bytes_ = 0;
  PaintLayer &active();
  const PaintLayer &layer(uint64_t) const;
};
float srgb_to_linear(float);
float linear_to_srgb(float);
} // namespace dome
