// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "volume.hpp"
#include <optional>
namespace dome {
enum class DeviceKind { Controller, SpatialStylus };
enum class SpatialCommand { Undo, Redo, Save, Export };
struct SpatialSample {
  uint32_t id = 0;
  uint64_t sequence = 0;
  double time = 0;
  Vec3 tip{}, normal{0, 1, 0};
  float pressure = 0, stick_x = 0, stick_y = 0;
  bool trigger = false, joystick_press = false, tracked = false,
       focused = false;
  DeviceKind kind = DeviceKind::Controller;
};
// Device-independent edit state. Platform adapters must pass original pose/time
// for each button transition, including transitions between rendering frames.
class SpatialInput {
public:
  explicit SpatialInput(Volume &v) : volume_(v) {}
  void update(const SpatialSample &);
  void cancel();
  bool menu_open() const { return menu_; }
  Vec3 menu_anchor() const { return menu_anchor_; }
  Tool tool() const { return tool_; }
  Tool menu_selection() const { return Tool(std::min(selected_, 3)); }
  int menu_index() const { return selected_; }
  void select_tool(Tool tool) {
    tool_ = tool;
    selected_ = int(tool);
  }
  std::optional<SpatialCommand> take_command() {
    auto c = command_;
    command_.reset();
    return c;
  }
  float radius = .1f, strength = 1;

private:
  Volume &volume_;
  uint32_t device_ = 0;
  struct DeviceState {
    uint64_t sequence = 0;
    double time = -1;
    bool armed = false, pressed = false, joystick = false;
  };
  std::unordered_map<uint32_t, DeviceState> devices_;
  bool menu_ = false;
  Tool tool_ = Tool::Clay;
  int selected_ = 0;
  std::optional<SpatialCommand> command_;
  Vec3 previous_{}, menu_anchor_{};
};
} // namespace dome
