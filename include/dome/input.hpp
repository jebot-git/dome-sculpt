// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "volume.hpp"
#include "vr-controls.hpp"
#include <functional>
#include <optional>
namespace dome {
enum class DeviceKind { Controller, SpatialStylus };
enum class SpatialCommand {
  Undo,
  Redo,
  Save,
  Export,
  Larger,
  Smaller,
  NewSphere,
  NewEmpty,
  Duplicate,
  Next,
  Previous,
  Visibility,
  Lock,
  Delete,
  Left,
  Right,
  Center,
  Warm,
  Blue,
  White,
  Dark,
  MetalUp,
  MetalDown,
  RoughUp,
  RoughDown,
  Toon,
  Pbr,
  Opacity,
  MakeMesh,
  EditSource,
  PaintColor,
  PaintMetal,
  PaintRough,
  PaintGlow,
  PaintMask,
  AddLayer,
  NextLayer,
  ErasePaint,
  PaintRed,
  PaintGreen,
  PaintBlue,
  PaintWhite,
  PaintBlack,
  PaintWarm,
  FlowUp,
  FlowDown,
  LayerVisible,
  DeleteLayer,
  LayerOpacity,
  SculptAdd,
  SculptNext,
  SculptPrevious,
  SculptVisible,
  SculptLock,
  SculptDelete,
  SculptOpacity,
  RecoveryRefresh, RecoveryNext, RecoveryPrevious, RecoveryRestore,
  CreateFemale, CreateMale, CreateMakeHuman, DeleteGroup, Ungroup,
  RigNextJoint, RigPreviousJoint, RigNextMorph, RigPreviousMorph, RigResetPose,
  BakeSurface, ClearBake, CancelWork
};
const std::array<const char *, 12> &spatial_labels(int page);
std::optional<SpatialCommand> spatial_action(int page, int index);
struct SpatialSample {
  uint32_t id = 0;
  uint64_t sequence = 0;
  double time = 0;
  Vec3 tip{}, normal{0, 1, 0};
  float pressure = 0, stick_x = 0, stick_y = 0;
  bool panel_hit = false;
  float panel_u = 0, panel_v = 0;
  bool trigger = false, joystick_press = false, tracked = false,
       focused = false;
  DeviceKind kind = DeviceKind::Controller;
};
// Device-independent edit state. Platform adapters must pass original pose/time
// for each button transition, including transitions between rendering frames.
class SpatialInput {
public:
  explicit SpatialInput(Volume &v) : volume_(&v) {}
  void bind(Volume &v) {
    cancel();
    volume_ = &v;
  }
  void update(const SpatialSample &);
  void refresh_volume(Volume &v) { volume_ = &v; }
  void cancel();
  bool menu_open() const { return menu_; }
  Vec3 menu_anchor() const { return menu_anchor_; }
  Tool tool() const { return tool_; }
  Tool menu_selection() const { return Tool(std::min(selected_, 3)); }
  int menu_index() const { return selected_; }
  int menu_page() const { return page_; }
  void select_tool(Tool tool) {
    tool_ = tool;
    selected_ = int(tool);
  }
  std::optional<SpatialCommand> take_command() {
    auto c = command_;
    command_.reset();
    return c;
  }
  void resume_control(VrParameter parameter) {
    parameter_ = parameter;
    page_ = 5;
    menu_ = true;
    device_ = parameter_owner_;
    control_armed_ = false;
    uv_dragging_ = false;
    uv_stick_active_ = uv_gesture_open_ = false;
    uv_pointer_ = 0;
  }
  void complete_control() {
    parameter_ = VrParameter::None;
    page_ = parent_page_;
    selected_ = parent_sector_;
    control_armed_ = false;
    uv_dragging_ = uv_stick_active_ = uv_gesture_open_ = false;
    uv_pointer_ = 0;
  }
  float radius = .1f, strength = 1;
  bool edit_allowed = true;
  std::function<void()> committed;
  std::function<bool(SpatialCommand)> allowed;
  struct StrokeBackend {
    std::function<bool()> active;
    std::function<void()> begin, cancel;
    std::function<void(const Brush &)> stamp;
    std::function<bool()> end;
  } backend;
  bool painting = false;
  std::function<bool(VrParameter)> parameter_allowed;
  std::function<bool()> control_accept;
  std::vector<VrControlEvent> take_controls() {
    auto result = std::move(controls_);
    controls_.clear();
    return result;
  }

private:
  Volume *volume_;
  void end_stroke() {
    if ((backend.end ? backend.end() : volume_->end_stroke()) && committed)
      committed();
  }
  void ray_controls(const SpatialSample &, bool pressed);
  void uv_gesture_begin();
  void uv_gesture_end(bool force = false);
  bool stroke_active() const {
    return backend.active ? backend.active() : volume_->in_stroke();
  }
  uint32_t device_ = 0, parameter_owner_ = 0;
  struct DeviceState {
    uint64_t sequence = 0;
    double time = -1;
    bool armed = false, pressed = false, joystick = false;
  };
  std::unordered_map<uint32_t, DeviceState> devices_;
  bool menu_ = false;
  Tool tool_ = Tool::Clay;
  int selected_ = 0, page_ = 0, parent_page_ = 0, parent_sector_ = 0,
      component_ = 0, y_latch_ = 0;
  VrParameter parameter_ = VrParameter::None;
  bool control_armed_ = false;
  std::vector<VrControlEvent> controls_;
  std::optional<SpatialCommand> command_;
  Vec3 previous_{}, menu_anchor_{};
  bool uv_dragging_ = false, uv_stick_active_ = false, uv_gesture_open_ = false;
  uint32_t uv_pointer_ = 0;
  int uv_mode_ = 0;
  float uv_x_ = 0, uv_y_ = 0;
};
} // namespace dome
