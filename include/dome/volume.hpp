// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "brush.hpp"
#include <deque>
#include <filesystem>
#include <optional>
#include <unordered_map>
#include <vector>
namespace dome {
class Volume {
public:
  using Voxels = std::unordered_map<Int3, float, IntHash>;
  explicit Volume(float voxel = .03f, size_t history_limit = 64 * 1024 * 1024);
  float voxel_size() const { return voxel_; }
  float band() const { return voxel_ * 4; }
  const Voxels &voxels() const { return cells_; }
  float at(Int3 p) const;
  float sample(Vec3 p) const;
  Vec3 gradient(Vec3 p) const;
  std::optional<Vec3> raycast(Vec3 origin, Vec3 direction,
                              float maximum = 20) const;
  void begin_stroke();
  void stamp(Brush brush);
  bool end_stroke();
  void cancel_stroke();
  bool undo();
  bool redo();
  bool in_stroke() const { return editing_; }
  size_t undo_count() const { return undo_.size(); }
  void save(const std::filesystem::path &) const;
  static Volume load(const std::filesystem::path &);

private:
  struct Edit {
    Int3 key;
    float before, after;
  };
  using Command = std::vector<Edit>;
  float voxel_;
  Voxels cells_, before_;
  std::deque<Command> undo_, redo_;
  size_t history_limit_, history_bytes_ = 0;
  bool editing_ = false;
  void set(Int3, float);
};
} // namespace dome
