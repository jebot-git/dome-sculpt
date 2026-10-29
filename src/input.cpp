// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/input.hpp"
namespace dome {
const std::array<const char *, 12> &spatial_labels(int page) {
  static const std::array<const char *, 12> root = {
      "Clay", "Carve",       "Smooth",  "Flatten",   "Undo",   "Redo",
      "Save", "Export glTF", "Objects", "Materials", "Larger", "Smaller"};
  static const std::array<const char *, 12> objects = {
      "New sphere", "New empty",  "Duplicate",  "Next",
      "Previous",   "Visibility", "Lock",       "Delete",
      "Back",       "Move left",  "Move right", "Center"};
  static const std::array<const char *, 12> materials = {
      "Warm clay", "Blue",     "White", "Dark", "Metal +", "Metal -",
      "Rougher",   "Smoother", "Back",  "Toon", "PBR",     "Opacity"};
  return page == 1 ? objects : page == 2 ? materials : root;
}
void SpatialInput::cancel() {
  if (volume_->in_stroke())
    volume_->cancel_stroke();
  device_ = 0;
  menu_ = false;
  page_ = 0;
  command_.reset();
  devices_.clear();
}
void SpatialInput::update(const SpatialSample &s) {
  bool valid = s.id && s.tracked && s.focused && finite(s.tip) &&
               finite(s.normal) && std::isfinite(s.time) && s.time >= 0 &&
               std::isfinite(s.pressure) && s.pressure >= 0 &&
               s.pressure <= 1 && std::isfinite(s.stick_x) &&
               std::isfinite(s.stick_y) && std::abs(s.stick_x) <= 1 &&
               std::abs(s.stick_y) <= 1;
  if (!valid) {
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
    if (s.trigger || s.joystick_press)
      d.armed = false;
    d.pressed = s.trigger;
    return;
  }
  if (menu_press) {
    if (volume_->in_stroke())
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
    } else if (page_ != 0 && selected_ == 8) {
      page_ = 0;
      selected_ = int(tool_);
    } else {
      if (page_ == 0 && selected_ < 4)
        tool_ = Tool(selected_);
      else {
        static const std::array<SpatialCommand, 12> root = {
            SpatialCommand::Undo,   SpatialCommand::Undo,
            SpatialCommand::Undo,   SpatialCommand::Undo,
            SpatialCommand::Undo,   SpatialCommand::Redo,
            SpatialCommand::Save,   SpatialCommand::Export,
            SpatialCommand::Undo,   SpatialCommand::Undo,
            SpatialCommand::Larger, SpatialCommand::Smaller};
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
        command_ = (page_ == 1   ? objects
                    : page_ == 2 ? materials
                                 : root)[selected_];
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
      volume_->begin_stroke();
      previous_ = s.tip;
    }
    if (device_ == s.id && volume_->in_stroke()) {
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
        volume_->stamp(brush);
      } catch (...) {
        cancel();
        throw;
      }
      previous_ = s.tip;
    }
  } else if (d.pressed && device_ == s.id && volume_->in_stroke()) {
    end_stroke();
    device_ = 0;
  }
  d.pressed = s.trigger;
}
} // namespace dome
