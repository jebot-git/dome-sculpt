// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "brush.hpp"
#include "cow-map.hpp"
#include <array>
#include <deque>
#include <filesystem>
#include <functional>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>
namespace dome {
struct SculptResolutionOptions;
struct SculptResolutionResult;
class Volume {
public:
  using Voxels = std::unordered_map<Int3, float, IntHash>;
  using GeometryOwner = std::shared_ptr<const Voxels>;
  struct GeometryRegion {
    Int3 minimum, maximum;
  };
  struct GeometryChange {
    enum class Kind { Unchanged, Local, Rebuild };
    static constexpr size_t maximum_regions = 128;
    Kind kind = Kind::Rebuild;
    std::array<GeometryRegion, maximum_regions> samples{};
    size_t count = 0, events = 0;
    std::span<const GeometryRegion> regions() const {
      return {samples.data(), count};
    }
  };
  // Ephemeral authenticated history, deliberately absent from native archives.
  GeometryOwner geometry_owner() const { return cells_.owner(); }
  GeometryChange geometry_change(const GeometryOwner &) const;
  enum class MaskTool : int { Add, Subtract, Smooth };
  struct SculptSettings {
    unsigned symmetry_axes = 0; // X=1, Y=2, Z=4, object-local planes.
    Vec3 symmetry_origin{};
    bool mask_edit = false, mask_visible = true;
    MaskTool mask_tool = MaskTool::Add;
    float mask_strength = 1;
  };
  const SculptSettings &sculpt_settings() const { return settings_; }
  bool sculpt_settings(SculptSettings);
  // Sparse exceptions to a global default. Fill/Invert never enumerate empty
  // space: a full mask also protects geometry created later in this object.
  float mask_at(Int3) const;
  float mask_sample(Vec3) const;
  float mask_default() const { return mask_.background; }
  const Voxels &mask_values() const { return mask_.values.read(); }
  bool fill_mask(float, const std::function<bool()> &cancelled = {});
  bool invert_mask(const std::function<bool()> &cancelled = {});
  struct Layer {
    uint64_t id = 1;
    std::string name = "Base";
    bool visible = true, locked = false;
    float opacity = 1;
    CowMap<Int3, double, IntHash> delta;
  };
  const std::vector<Layer> &layers() const { return layers_; }
  uint64_t active_layer() const { return active_; }
  bool editable_layer() const;
  uint64_t add_layer(std::string name);
  void select_layer(uint64_t);
  void layer_properties(uint64_t, std::string, bool visible, bool locked,
                        float opacity);
  void erase_layer(uint64_t);
  size_t stored_samples() const;
  explicit Volume(float voxel = .03f, size_t history_limit = 64 * 1024 * 1024);
  float voxel_size() const { return voxel_; }
  float band() const { return band_; }
  const Voxels &voxels() const { return cells_.read(); }
  float at(Int3 p) const;
  float sample(Vec3 p) const;
  Vec3 gradient(Vec3 p) const;
  std::optional<Vec3> raycast(Vec3 origin, Vec3 direction,
                              float maximum = 20) const;
  void begin_stroke();
  void stamp(Brush brush, size_t sample_budget = 4'000'000);
  void stamp(Brush, size_t sample_budget,
             const std::function<bool()> &cancelled);
  bool end_stroke();
  void cancel_stroke();
  bool undo();
  bool redo();
  bool in_stroke() const { return editing_; }
  bool pending_changes() const { return !before_.empty() || mask_changed_; }
  size_t undo_count() const { return undo_.size(); }
  uint64_t revision() const { return revision_; }
  Volume geometry_snapshot() const;
  nlohmann::json encode() const;
  static Volume decode(const nlohmann::json &);
  static Volume from_samples(float voxel, Voxels);
  void save(const std::filesystem::path &) const;
  static Volume load(const std::filesystem::path &);

private:
  friend class Document;
  void swap_state(Volume &) noexcept;
  struct GeometryHistory;
  using GeometryHistoryPtr = std::shared_ptr<const GeometryHistory>;
  GeometryHistoryPtr geometry_history_, geometry_start_;
  static void geometry_include(GeometryChange &, Int3);
  static void geometry_include(GeometryChange &, GeometryRegion);
  GeometryHistoryPtr
  prepare_geometry_history(const CowMap<Int3, float, IntHash> &,
                           const GeometryChange &) const;
  friend SculptResolutionResult resample_sculpt(const Volume &, const SculptResolutionOptions &);
  struct MaskState {
    CowMap<Int3, float, IntHash> values;
    float background = 0;
  };
  MaskState mask_;
  SculptSettings settings_;
  std::optional<MaskState> mask_start_;
  std::optional<CowMap<Int3, float, IntHash>> cells_start_;
  std::optional<CowMap<Int3, double, IntHash>> delta_start_;
  bool mask_stroke_ = false, mask_changed_ = false;
  static bool same_settings(const SculptSettings &, const SculptSettings &);
  static bool same_mask(const MaskState &, const MaskState &);
  struct Edit {
    Int3 key;
    float before, after;
    double delta_before, delta_after;
  };
  struct Command : std::vector<Edit> {
    uint64_t layer = 1;
    std::optional<MaskState> mask_before, mask_after;
    std::optional<SculptSettings> settings_before, settings_after;
    size_t bytes() const;
  };
  void push_command(Command);
  std::vector<Layer> layers_{Layer{}};
  uint64_t active_ = 1, next_layer_ = 2;
  std::unordered_map<Int3, double, IntHash> before_delta_;
  Layer &layer(uint64_t);
  double sum(Int3, uint64_t exclude = 0) const;
  void recompose();
  void clear_history();
  float voxel_, band_;
  CowMap<Int3, float, IntHash> cells_;
  Voxels before_;
  std::deque<Command> undo_, redo_;
  size_t history_limit_, history_bytes_ = 0;
  bool editing_ = false;
  uint64_t revision_ = 0;
};
} // namespace dome
