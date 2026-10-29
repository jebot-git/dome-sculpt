// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "brush.hpp"
#include "cow-map.hpp"
#include <deque>
#include <filesystem>
#include <nlohmann/json_fwd.hpp>
#include <optional>
#include <unordered_map>
#include <vector>
namespace dome {
class Volume {
public:
  using Voxels = std::unordered_map<Int3, float, IntHash>;
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
  float band() const { return voxel_ * 4; }
  const Voxels &voxels() const { return cells_.read(); }
  float at(Int3 p) const;
  float sample(Vec3 p) const;
  Vec3 gradient(Vec3 p) const;
  std::optional<Vec3> raycast(Vec3 origin, Vec3 direction,
                              float maximum = 20) const;
  void begin_stroke();
  void stamp(Brush brush, size_t sample_budget = 4'000'000);
  bool end_stroke();
  void cancel_stroke();
  bool undo();
  bool redo();
  bool in_stroke() const { return editing_; }
  bool pending_changes() const { return !before_.empty(); }
  size_t undo_count() const { return undo_.size(); }
  uint64_t revision() const { return revision_; }
  Volume geometry_snapshot() const;
  nlohmann::json encode() const;
  static Volume decode(const nlohmann::json &);
  static Volume from_samples(float voxel, Voxels);
  void save(const std::filesystem::path &) const;
  static Volume load(const std::filesystem::path &);

private:
  struct Edit {
    Int3 key;
    float before, after;
    double delta_before, delta_after;
  };
  struct Command : std::vector<Edit> {
    uint64_t layer = 1;
  };
  std::vector<Layer> layers_{Layer{}};
  uint64_t active_ = 1, next_layer_ = 2;
  std::unordered_map<Int3, double, IntHash> before_delta_;
  Layer &layer(uint64_t);
  double sum(Int3, uint64_t exclude = 0) const;
  void recompose();
  void raw_set(Int3, float);
  void clear_history();
  float voxel_;
  CowMap<Int3, float, IntHash> cells_;
  Voxels before_;
  std::deque<Command> undo_, redo_;
  size_t history_limit_, history_bytes_ = 0;
  bool editing_ = false;
  uint64_t revision_ = 0;
  void set(Int3, float);
};
} // namespace dome
