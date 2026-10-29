// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/input.hpp"
#include "dome/uv.hpp"
namespace dome {
const std::array<const char *, 12> &spatial_labels(int page) {
  static const std::array<const char *, 12> root = {
      "Clay", "Carve",       "Smooth",  "Flatten",   "Undo",       "Redo",
      "Save", "Export glTF", "Objects", "Materials", "Brush size", "Flow"};
  static const std::array<const char *, 12> objects = {
      "New sphere", "New empty",  "Duplicate",  "Next",
      "Previous",   "Visibility", "Lock",       "Delete",
      "Back",       "Move left",  "Move right", "Sculpt layers"};
  static const std::array<const char *, 12> materials = {
      "Base color", "Blue",      "White",   "Paint tools",
      "Metallic",   "Roughness", "Opacity", "UV editor",
      "Back",       "Toon",      "PBR",     "Flow"};
  static const std::array<const char *, 12> paint = {
      "Make mesh",   "Edit source", "Paint color", "Paint metal",
      "Paint rough", "Paint glow",  "Mask",        "Add layer",
      "Back",        "Next layer",  "Erase",       "Controls"};
  static const std::array<const char *, 12> colors = {
      "Red",    "Green",  "Blue", "White",      "Black",        "Warm",
      "Flow +", "Flow -", "Back", "Show layer", "Delete layer", "Layer alpha"};
  static const std::array<const char *, 12> controls = {
      "Color",      "Metal value", "Rough value", "Emission",
      "Mask value", "Flow",        "Layer alpha", "Brush size",
      "Back",       "Presets",     "Erase",       "Layer order"};
  static const std::array<const char *, 12> sculpt = {
      "Add layer",  "Next layer", "Previous",     "Show layer",
      "Lock layer", "Strength",   "Delete layer", "Undo",
      "Back",       "Redo",       "Recovery",     "Save"};
  static const std::array<const char *, 12> recovery = {
      "Refresh", "Next project", "Previous", "Recover", "Save", "Center",
      "Undo", "Redo", "Back", "Export glTF", "Objects", "Materials"};
  return page == 8   ? recovery
         : page == 7 ? sculpt
         : page == 1 ? objects
         : page == 2 ? materials
         : page == 3 ? paint
         : page == 4 ? colors
         : page == 6 ? controls
                     : root;
}
std::optional<VrParameter> spatial_parameter(int page, int sector) {
  if (page == 7 && sector == 5)
    return VrParameter::SculptOpacity;
  if (page == 0) {
    if (sector == 10)
      return VrParameter::BrushRadius;
    if (sector == 11)
      return VrParameter::Flow;
  }
  if (page == 2) {
    switch (sector) {
    case 0:
      return VrParameter::BaseColor;
    case 4:
      return VrParameter::Metallic;
    case 5:
      return VrParameter::Roughness;
    case 6:
      return VrParameter::Opacity;
    case 7:
      return VrParameter::UvEdit;
    case 11:
      return VrParameter::Flow;
    default:
      break;
    }
  }
  if (page == 6) {
    static const std::array<VrParameter, 12> controls = {
        VrParameter::InkColor,     VrParameter::InkMetallic,
        VrParameter::InkRoughness, VrParameter::InkEmission,
        VrParameter::Mask,         VrParameter::Flow,
        VrParameter::LayerOpacity, VrParameter::BrushRadius,
        VrParameter::None,         VrParameter::None,
        VrParameter::None,         VrParameter::LayerOrder};
    if (sector >= 0 && sector < 12 && controls[sector] != VrParameter::None)
      return controls[sector];
  }
  return {};
}
std::optional<SpatialCommand> spatial_action(int page, int index) {
  if (index < 0 || index >= 12 || page < 0 || page > 8 || page == 5 ||
      spatial_parameter(page, index))
    return {};
  if (page == 8) {
    static const std::array<SpatialCommand, 12> recovery = {
      SpatialCommand::RecoveryRefresh, SpatialCommand::RecoveryNext,
      SpatialCommand::RecoveryPrevious, SpatialCommand::RecoveryRestore,
      SpatialCommand::Save, SpatialCommand::Center,
      SpatialCommand::Undo, SpatialCommand::Redo, SpatialCommand::Undo,
      SpatialCommand::Export, SpatialCommand::Undo, SpatialCommand::Undo};
    return index == 8 || index > 9 ? std::nullopt
                                   : std::optional<SpatialCommand>(recovery[index]);
  }
  if (page == 7) {
    static const std::array<SpatialCommand, 12> sculpt = {
        SpatialCommand::SculptAdd,      SpatialCommand::SculptNext,
        SpatialCommand::SculptPrevious, SpatialCommand::SculptVisible,
        SpatialCommand::SculptLock,     SpatialCommand::SculptOpacity,
        SpatialCommand::SculptDelete,   SpatialCommand::Undo,
        SpatialCommand::Undo,           SpatialCommand::Redo,
        SpatialCommand::Center,         SpatialCommand::Save};
    return index == 8 || index == 10 ? std::nullopt
                      : std::optional<SpatialCommand>(sculpt[index]);
  }
  if ((page == 0 && (index < 4 || index == 8 || index == 9)) ||
      (page != 0 && index == 8) || (page == 2 && index == 3) ||
      (page == 3 && index == 11) || (page == 1 && index == 11))
    return {};
  static const std::array<SpatialCommand, 12> root = {
      SpatialCommand::Undo, SpatialCommand::Undo,   SpatialCommand::Undo,
      SpatialCommand::Undo, SpatialCommand::Undo,   SpatialCommand::Redo,
      SpatialCommand::Save, SpatialCommand::Export, SpatialCommand::Undo,
      SpatialCommand::Undo, SpatialCommand::Larger, SpatialCommand::Smaller};
  static const std::array<SpatialCommand, 12> objects = {
      SpatialCommand::NewSphere, SpatialCommand::NewEmpty,
      SpatialCommand::Duplicate, SpatialCommand::Next,
      SpatialCommand::Previous,  SpatialCommand::Visibility,
      SpatialCommand::Lock,      SpatialCommand::Delete,
      SpatialCommand::Undo,      SpatialCommand::Left,
      SpatialCommand::Right,     SpatialCommand::Center};
  static const std::array<SpatialCommand, 12> materials = {
      SpatialCommand::Warm,    SpatialCommand::Blue,
      SpatialCommand::White,   SpatialCommand::Dark,
      SpatialCommand::MetalUp, SpatialCommand::MetalDown,
      SpatialCommand::RoughUp, SpatialCommand::RoughDown,
      SpatialCommand::Undo,    SpatialCommand::Toon,
      SpatialCommand::Pbr,     SpatialCommand::Opacity};
  static const std::array<SpatialCommand, 12> paint = {
      SpatialCommand::MakeMesh,   SpatialCommand::EditSource,
      SpatialCommand::PaintColor, SpatialCommand::PaintMetal,
      SpatialCommand::PaintRough, SpatialCommand::PaintGlow,
      SpatialCommand::PaintMask,  SpatialCommand::AddLayer,
      SpatialCommand::Undo,       SpatialCommand::NextLayer,
      SpatialCommand::ErasePaint, SpatialCommand::Undo};
  static const std::array<SpatialCommand, 12> colors = {
      SpatialCommand::PaintRed,    SpatialCommand::PaintGreen,
      SpatialCommand::PaintBlue,   SpatialCommand::PaintWhite,
      SpatialCommand::PaintBlack,  SpatialCommand::PaintWarm,
      SpatialCommand::FlowUp,      SpatialCommand::FlowDown,
      SpatialCommand::Undo,        SpatialCommand::LayerVisible,
      SpatialCommand::DeleteLayer, SpatialCommand::LayerOpacity};
  if (page == 6)
    return index == 10
               ? std::optional<SpatialCommand>(SpatialCommand::ErasePaint)
               : std::nullopt;
  return (page == 1   ? objects
          : page == 2 ? materials
          : page == 3 ? paint
          : page == 4 ? colors
                      : root)[index];
}
void SpatialInput::cancel() {
  if (stroke_active()) {
    if (backend.cancel)
      backend.cancel();
    else
      volume_->cancel_stroke();
  }
  device_ = 0;
  menu_ = false;
  page_ = 0;
  parameter_ = VrParameter::None;
  controls_.clear();
  uv_dragging_ = false;
  uv_stick_active_ = uv_gesture_open_ = false;
  uv_pointer_ = 0;
  command_.reset();
  devices_.clear();
}
void SpatialInput::uv_gesture_begin() {
  controls_.push_back({VrControlEventType::UvCommand, parameter_,
                       int(UvAction::BeginGesture)});
  uv_gesture_open_ = true;
}
void SpatialInput::uv_gesture_end(bool force) {
  if (uv_gesture_open_ && (force || (!uv_dragging_ && !uv_stick_active_))) {
    controls_.push_back({VrControlEventType::UvCommand, parameter_,
                         int(UvAction::EndGesture)});
    uv_gesture_open_ = false;
  }
}
void SpatialInput::ray_controls(const SpatialSample &s, bool pressed) {
  if (pressed && s.panel_hit && s.panel_v >= 588.f / 640 &&
      s.panel_v <= 630.f / 640) {
    bool cancel = s.panel_u >= 60.f / 640 && s.panel_u <= 280.f / 640;
    bool accept = s.panel_u >= 360.f / 640 && s.panel_u <= 580.f / 640;
    if (accept && control_accept && !control_accept())
      return;
    if (cancel || accept) {
      uv_dragging_ = uv_stick_active_ = uv_gesture_open_ = false;
      uv_pointer_ = 0;
      controls_.push_back(
          {cancel ? VrControlEventType::Cancel : VrControlEventType::Confirm,
           parameter_});
      parameter_ = VrParameter::None;
      page_ = parent_page_;
      selected_ = parent_sector_;
      return;
    }
  }
  if (parameter_ == VrParameter::UvEdit) {
    if (!s.trigger) {
      if (uv_pointer_ == s.id) {
        uv_dragging_ = false;
        uv_pointer_ = 0;
        uv_gesture_end();
      }
      return;
    }
    float x = s.panel_u * 640, y = s.panel_v * 640;
    if (pressed && ((y >= 68 && y <= 102) || (y >= 538 && y <= 580)) &&
        x >= 40 && x < 600) {
      int column = int((x - 40) / 94);
      if (column < 6 && (x - 40 - column * 94) < 88) {
        UvAction action;
        float value = 0;
        if (y < 110) {
          if (column < 3) {
            uv_mode_ = column;
            action = UvAction::Tool;
            value = float(column);
          } else {
            action = column == 3   ? UvAction::Rotate
                     : column == 4 ? UvAction::Undo
                                   : UvAction::Redo;
            value = column == 3 ? 1.570796327f : 0;
          }
        } else {
          static const UvAction actions[] = {
              UvAction::VertexMode, UvAction::Pin,  UvAction::Relax,
              UvAction::Pack,       UvAction::Seam, UvAction::Next};
          action = actions[column];
        }
        uv_gesture_end(true);
        controls_.push_back(
            {VrControlEventType::UvCommand, parameter_, int(action), value});
      }
      return;
    }
    if (x >= 110 && x <= 530 && y >= 110 && y <= 530) {
      if (uv_pointer_ && uv_pointer_ != s.id)
        return;
      float u = (x - 110) / 420, v = (y - 110) / 420;
      if (pressed) {
        controls_.push_back({VrControlEventType::UvPick, parameter_, 0, u, v});
        uv_dragging_ = true;
        uv_pointer_ = s.id;
      } else if (uv_dragging_) {
        uv_gesture_begin();
        if (!uv_mode_)
          controls_.push_back({VrControlEventType::UvMove, parameter_, 0,
                               u - uv_x_, v - uv_y_});
        else {
          float delta = (u - uv_x_) - (v - uv_y_) * .1f;
          auto action = uv_mode_ == 1 ? UvAction::Scale : UvAction::Rotate;
          float value =
              uv_mode_ == 1 ? std::exp(delta * 2) : delta * 3.14159265f;
          controls_.push_back(
              {VrControlEventType::UvCommand, parameter_, int(action), value});
        }
      }
      uv_x_ = u;
      uv_y_ = v;
    } else if (uv_pointer_ == s.id) {
      uv_dragging_ = false;
      uv_pointer_ = 0;
      uv_gesture_end();
    }
    return;
  }
  bool color = parameter_ == VrParameter::InkColor ||
               parameter_ == VrParameter::InkEmission ||
               parameter_ == VrParameter::BaseColor;
  int components = parameter_ == VrParameter::InkEmission ? 3 : color ? 4 : 1;
  if (s.trigger && s.panel_hit) {
    int row = 0;
    float y = .46875f;
    if (color) {
      int base = components == 4 ? 210 : 240, step = components == 4 ? 70 : 80;
      row = int(std::round((s.panel_v * 640 - base) / step));
      row = std::clamp(row, 0, components - 1);
      y = (base + row * step) / 640.f;
    }
    if (std::abs(s.panel_v - y) < .06f && s.panel_u > .12f &&
        s.panel_u < .86f) {
      component_ = row;
      controls_.push_back(
          {VrControlEventType::Absolute, parameter_, component_,
           std::clamp((s.panel_u * 640 - 100) / 420.f, 0.f, 1.f)});
    }
  }
}
void SpatialInput::update(const SpatialSample &s) {
  if (controls_.size() > 64) {
    cancel();
    return;
  }
  bool valid =
      s.id && s.tracked && s.focused && finite(s.tip) && finite(s.normal) &&
      std::isfinite(s.time) && s.time >= 0 && std::isfinite(s.pressure) &&
      s.pressure >= 0 && s.pressure <= 1 && std::isfinite(s.stick_x) &&
      std::isfinite(s.stick_y) && std::abs(s.stick_x) <= 1 &&
      std::abs(s.stick_y) <= 1 &&
      (!s.panel_hit || (std::isfinite(s.panel_u) && std::isfinite(s.panel_v)));
  if (!valid) {
    if (uv_pointer_ == s.id) {
      uv_dragging_ = false;
      uv_pointer_ = 0;
      uv_gesture_end();
    }
    if (s.id == device_)
      cancel();
    else
      devices_.erase(s.id);
    return;
  }
  if (!devices_.contains(s.id) && devices_.size() >= 16)
    throw std::length_error("Too many spatial input sources");
  auto old = devices_.find(s.id);
  if (old != devices_.end()) {
    if (s.sequence <= old->second.sequence || s.time < old->second.time)
      return;
    if (old->second.time >= 0 && s.time - old->second.time > .25) {
      if (device_ == s.id)
        cancel();
      else
        devices_.erase(old);
    }
  }
  auto &d = devices_[s.id];
  double elapsed = d.time >= 0 ? std::clamp(s.time - d.time, 0.0, .1) : 0;
  d.sequence = s.sequence;
  d.time = s.time;
  if (!d.armed) {
    d.pressed = s.trigger;
    d.joystick = s.joystick_press;
    if (!s.trigger && !s.joystick_press)
      d.armed = true;
    return;
  }
  bool menu_press = s.joystick_press && !d.joystick;
  d.joystick = s.joystick_press;
  // Idle hands cannot acquire an edit lease. A hand held while another edits
  // must release before it can take ownership of a later stroke.
  if (device_ && device_ != s.id) {

    if (menu_ && parameter_ != VrParameter::None && control_armed_ &&
        (s.panel_hit || uv_dragging_)) {
      ray_controls(s, s.trigger && !d.pressed);
      d.pressed = s.trigger;
      return;
    }
    if (s.trigger || s.joystick_press)
      d.armed = false;
    d.pressed = s.trigger;
    return;
  }
  if (menu_ && parameter_ != VrParameter::None) {
    if (menu_press) {
      if (control_accept && !control_accept())
        return;
      uv_dragging_ = uv_stick_active_ = uv_gesture_open_ = false;
      uv_pointer_ = 0;
      controls_.push_back({VrControlEventType::Confirm, parameter_});
      parameter_ = VrParameter::None;
      page_ = parent_page_;
      selected_ = parent_sector_;
      d.pressed = s.trigger;
      return;
    }
    if (!control_armed_) {
      if (std::abs(s.stick_x) < .25f && std::abs(s.stick_y) < .25f &&
          !s.trigger)
        control_armed_ = true;
      d.pressed = s.trigger;
      return;
    }
    if (parameter_ == VrParameter::UvEdit) {
      auto axis = [](float v) {
        return std::abs(v) > .25f
                   ? std::copysign((std::abs(v) - .25f) / .75f, v)
                   : 0.f;
      };
      float dx = float(elapsed) * axis(s.stick_x) * .25f,
            dy = -float(elapsed) * axis(s.stick_y) * .25f;
      uv_stick_active_ = axis(s.stick_x) != 0 || axis(s.stick_y) != 0;
      if (dx || dy) {
        uv_gesture_begin();
        if (!uv_mode_)
          controls_.push_back(
              {VrControlEventType::UvMove, parameter_, 0, dx, dy});
        else {
          float delta =
              float(elapsed) * (axis(s.stick_x) + axis(s.stick_y) * .1f);
          auto action = uv_mode_ == 1 ? UvAction::Scale : UvAction::Rotate;
          float value = uv_mode_ == 1 ? std::exp(delta * .69314718f)
                                      : delta * .523598776f;
          controls_.push_back(
              {VrControlEventType::UvCommand, parameter_, int(action), value});
        }
      }
      ray_controls(s, s.trigger && !d.pressed);
      uv_gesture_end();
      d.pressed = s.trigger;
      return;
    }
    bool color = parameter_ == VrParameter::InkColor ||
                 parameter_ == VrParameter::InkEmission ||
                 parameter_ == VrParameter::BaseColor;
    int components = parameter_ == VrParameter::InkEmission ? 3 : color ? 4 : 1;
    if (color) {
      if (std::abs(s.stick_y) < .3f)
        y_latch_ = 0;
      if (std::abs(s.stick_y) > .6f && !y_latch_) {
        y_latch_ = 1;
        component_ =
            (component_ + (s.stick_y < 0 ? 1 : components - 1)) % components;
        controls_.push_back(
            {VrControlEventType::Select, parameter_, component_});
      }
    }
    auto axis = [](float v) {
      return std::abs(v) > .25f ? std::copysign((std::abs(v) - .25f) / .75f, v)
                                : 0.f;
    };
    float delta = float(elapsed) *
                  (axis(s.stick_x) + (color ? 0 : axis(s.stick_y) * .1f));
    if (delta)
      controls_.push_back(
          {VrControlEventType::Delta, parameter_, component_, delta});
    ray_controls(s, s.trigger && !d.pressed);
    d.pressed = s.trigger;
    return;
  }
  if (menu_press) {
    if (stroke_active())
      end_stroke();
    if (!menu_) {
      menu_ = true;
      page_ = 0;
      menu_anchor_ = s.tip;
      device_ = s.id;
      selected_ = int(tool_);
    } else if (page_ == 0 && (selected_ == 8 || selected_ == 9)) {
      page_ = selected_ == 8 ? 1 : 2;
      selected_ = 0;
    } else if (page_ == 7 && selected_ == 10) {
      page_ = 8;
      selected_ = 0;
    } else if (page_ == 8 && (selected_ == 10 || selected_ == 11)) {
      page_ = selected_ == 10 ? 1 : 2;
      selected_ = 0;
    } else if (page_ == 2 && selected_ == 3) {
      page_ = 3;
      selected_ = 0;
    } else if (page_ == 1 && selected_ == 11) {
      page_ = 7;
      selected_ = 0;
    } else if (page_ == 3 && selected_ == 11) {
      page_ = 6;
      selected_ = 0;
    } else if (page_ == 6 && selected_ == 9) {
      page_ = 4;
      selected_ = 0;
    } else if (auto parameter = spatial_parameter(page_, selected_)) {
      if (parameter_allowed && !parameter_allowed(*parameter)) {
        d.pressed = s.trigger;
        return;
      }
      parent_page_ = page_;
      parent_sector_ = selected_;
      parameter_ = *parameter;
      parameter_owner_ = device_;
      uv_mode_ = 0;
      uv_dragging_ = false;
      uv_stick_active_ = uv_gesture_open_ = false;
      uv_pointer_ = 0;
      control_armed_ = false;
      component_ = y_latch_ = 0;
      page_ = 5;
      controls_.push_back({VrControlEventType::Begin, parameter_});
    } else if (page_ != 0 && selected_ == 8) {
      page_ = page_ == 8   ? 7
              : page_ == 7 ? 1
              : page_ == 4 ? 6
              : page_ == 6 ? 3
              : page_ == 3 ? 2
                           : 0;
      selected_ = int(tool_);
    } else {
      if (page_ == 0 && selected_ < 4) {
        if (painting && allowed && !allowed(SpatialCommand::EditSource)) {
          d.pressed = s.trigger;
          return;
        }
        tool_ = Tool(selected_);
        if (painting)
          command_ = SpatialCommand::EditSource;
      } else {
        auto action = spatial_action(page_, selected_);
        if (!action || (allowed && !allowed(*action))) {
          d.pressed = s.trigger;
          return;
        }
        command_ = action;
      }
      menu_ = false;
      device_ = 0;
      d.armed = false;
    }
    d.pressed = s.trigger;
    return;
  }
  if (menu_) {
    if (s.stick_x * s.stick_x + s.stick_y * s.stick_y > .25f) {
      constexpr float pi = 3.14159265358979323846f;
      float angle = std::atan2(s.stick_y, s.stick_x);
      int index = int(std::floor((angle + pi / 12 + 2 * pi) / (pi / 6))) % 12;
      selected_ = index;
    }
    d.pressed = s.trigger;
    return;
  }
  if (!edit_allowed) {
    d.pressed = s.trigger;
    return;
  }
  if (s.trigger) {
    if (!d.pressed) {
      device_ = s.id;
      if (backend.begin)
        backend.begin();
      else
        volume_->begin_stroke();
      previous_ = s.tip;
    }
    if (device_ == s.id && stroke_active()) {
      Brush brush;
      brush.from = previous_;
      brush.to = s.tip;
      brush.normal = s.normal;
      brush.radius = radius;
      brush.strength = strength;
      brush.pressure = s.pressure;
      brush.blend = std::min(.025f, radius * .25f);
      brush.tool = tool_;
      try {
        if (backend.stamp)
          backend.stamp(brush);
        else
          volume_->stamp(brush);
      } catch (...) {
        cancel();
        throw;
      }
      previous_ = s.tip;
    }
  } else if (d.pressed && device_ == s.id && stroke_active()) {
    end_stroke();
    device_ = 0;
  }
  d.pressed = s.trigger;
}
} // namespace dome
