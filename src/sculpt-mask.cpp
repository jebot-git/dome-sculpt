// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/volume.hpp"
#include "sculpt-mask.hpp"
#include <algorithm>
#include <cmath>
namespace dome {
bool Volume::same_settings(const SculptSettings &a, const SculptSettings &b) {
  return a.symmetry_axes == b.symmetry_axes &&
      a.symmetry_origin.x == b.symmetry_origin.x && a.symmetry_origin.y == b.symmetry_origin.y &&
      a.symmetry_origin.z == b.symmetry_origin.z && a.mask_edit == b.mask_edit &&
      a.mask_visible == b.mask_visible && a.mask_tool == b.mask_tool && a.mask_strength == b.mask_strength;
}
bool Volume::same_mask(const MaskState &a, const MaskState &b) {
  return a.background == b.background && a.values.read() == b.values.read();
}
size_t Volume::Command::bytes() const {
  size_t result = size() * sizeof(Edit);
  if (mask_before) result += mask_before->values.size() * (sizeof(Int3) + sizeof(float));
  if (mask_after) result += mask_after->values.size() * (sizeof(Int3) + sizeof(float));
  return result + sizeof(Command);
}
void Volume::push_command(Command command) {
  auto bytes = command.bytes();
  undo_.push_back(std::move(command)); // Allocate before invalidating redo/history.
  for (auto &c : redo_) history_bytes_ -= c.bytes();
  redo_.clear(); history_bytes_ += bytes;
  while (history_bytes_ > history_limit_ && undo_.size() > 1) {
    history_bytes_ -= undo_.front().bytes(); undo_.pop_front();
  }
}
bool Volume::sculpt_settings(SculptSettings settings) {
  if (editing_) throw std::logic_error("Finish sculpt stroke before changing controls");
  if (settings.symmetry_axes > 7 || !finite(settings.symmetry_origin) ||
      std::max({std::abs(settings.symmetry_origin.x),std::abs(settings.symmetry_origin.y),std::abs(settings.symmetry_origin.z)}) > 100 ||
      int(settings.mask_tool) < 0 || int(settings.mask_tool) > 2 ||
      !std::isfinite(settings.mask_strength) || settings.mask_strength < 0 || settings.mask_strength > 1)
    throw std::invalid_argument("Invalid sculpt mask/symmetry controls");
  if (same_settings(settings_, settings)) return false;
  Command command; command.settings_before = settings_; command.settings_after = settings;
  push_command(std::move(command)); settings_ = settings; ++revision_; return true;
}
float Volume::mask_at(Int3 q) const {
  auto found = mask_.values.find(q);
  return found == mask_.values.end() ? mask_.background : found->second;
}
float Volume::mask_sample(Vec3 p) const {
  if (!finite(p) || std::max({std::abs(p.x),std::abs(p.y),std::abs(p.z)}) > 1001)
    throw std::invalid_argument("Invalid sculpt mask sample position");
  p = p / voxel_;
  Int3 q{int(std::floor(p.x)), int(std::floor(p.y)), int(std::floor(p.z))};
  Vec3 f = p - Vec3{float(q.x),float(q.y),float(q.z)};
  float result = 0;
  for (int z = 0; z < 2; ++z) for (int y = 0; y < 2; ++y) for (int x = 0; x < 2; ++x)
    result += mask_at(q + Int3{x,y,z}) * (x ? f.x : 1-f.x) * (y ? f.y : 1-f.y) * (z ? f.z : 1-f.z);
  return std::clamp(result,0.f,1.f);
}
bool Volume::fill_mask(float value, const std::function<bool()> &cancelled) {
  if (editing_) throw std::logic_error("Finish sculpt stroke before filling mask");
  if (!std::isfinite(value) || value < 0 || value > 1) throw std::invalid_argument("Invalid mask fill");
  sculpt_detail::checkpoint(cancelled);
  if (mask_.background == value && mask_.values.size() == 0) return false;
  MaskState next; next.background = value;
  Command command; command.mask_before = mask_; command.mask_after = next;
  sculpt_detail::checkpoint(cancelled); push_command(std::move(command));
  mask_ = std::move(next); ++revision_; return true;
}
bool Volume::invert_mask(const std::function<bool()> &cancelled) {
  if (editing_) throw std::logic_error("Finish sculpt stroke before inverting mask");
  sculpt_detail::checkpoint(cancelled);
  MaskState next; next.background = 1 - mask_.background;
  size_t count = 0;
  for (auto &[q,value] : mask_.values) {
    if ((count++ & 1023) == 0) sculpt_detail::checkpoint(cancelled);
    auto inverted = 1 - value;
    if (inverted != next.background) next.values.emplace(q,inverted);
  }
  if (same_mask(mask_,next)) return false;
  Command command; command.mask_before = mask_; command.mask_after = next;
  sculpt_detail::checkpoint(cancelled); push_command(std::move(command));
  mask_ = std::move(next); ++revision_; return true;
}
} // namespace dome
