// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/volume.hpp"
#include "io.hpp"
#include "sculpt-mask.hpp"
#include "sculpt-symmetry.hpp"
#include "volume-raycast.hpp"
#include "volume-archive.hpp"
#include <fcntl.h>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <set>
#include <unistd.h>
namespace dome {
Volume::Volume(float voxel, size_t limit)
    : voxel_(voxel), band_(voxel * 4), history_limit_(limit) {
  if (!std::isfinite(voxel) || voxel < .00001f || voxel > 1)
    throw std::invalid_argument(
        "Voxel size must be between 0.00001 and 1 metres");
}
Volume Volume::from_samples(float voxel, Voxels samples) {
  Volume v(voxel);
  if (samples.size() > 4'000'000)
    throw std::length_error("Field sample budget exceeded");
  auto limit = int64_t(101 / voxel);
  for (auto &[p, d] : samples) {
    if (!std::isfinite(d) || std::abs(d) > v.band() ||
        std::max({std::abs(int64_t(p.x)), std::abs(int64_t(p.y)),
                  std::abs(int64_t(p.z))}) > limit)
      throw std::invalid_argument("Invalid imported field sample");
    if (d < v.band())
      v.layers_.front().delta.emplace(p, double(d) - v.band());
  }
  std::erase_if(samples, [&](const auto &e) { return e.second >= v.band(); });
  v.cells_ = std::move(samples);
  return v;
}
float Volume::at(Int3 p) const {
  auto it = cells_.find(p);
  return it == cells_.end() ? band() : it->second;
}

namespace {
void layer_name(const std::string &n) {
  if (n.empty() || n.size() > 256 || n.find('\0') != std::string::npos)
    throw std::invalid_argument("Invalid sculpt layer name");
}
} // namespace
Volume::Layer &Volume::layer(uint64_t id) {
  for (auto &l : layers_)
    if (l.id == id)
      return l;
  throw std::out_of_range("Unknown sculpt layer");
}
bool Volume::editable_layer() const {
  for (auto &l : layers_)
    if (l.id == active_)
      return l.visible && !l.locked && l.opacity >= .0001f;
  return false;
}
size_t Volume::stored_samples() const {
  size_t n = 0;
  for (auto &l : layers_)
    n += l.delta.size();
  return n + mask_.values.size();
}
double Volume::sum(Int3 p, uint64_t exclude) const {
  double v = band();
  for (auto &l : layers_)
    if (l.id != exclude && l.visible) {
      auto i = l.delta.find(p);
      if (i != l.delta.end())
        v += i->second * l.opacity;
    }
  return v;
}
void Volume::clear_history() {
  undo_.clear();
  redo_.clear();
  history_bytes_ = 0;
}
void Volume::recompose() {
  Voxels result;
  std::unordered_map<Int3, bool, IntHash> keys;
  for (auto &l : layers_)
    for (auto &[p, d] : l.delta)
      keys[p] = true;
  for (auto &[p, unused] : keys) {
    float v = std::clamp(float(sum(p)), -band(), band());
    if (v < band())
      result.emplace(p, v);
  }
  if (result != cells_.read()) {
    CowMap<Int3, float, IntHash> next;
    next = std::move(result);
    auto history = prepare_geometry_history(next, GeometryChange{});
    cells_ = std::move(next);
    geometry_history_ = std::move(history);
  }
  ++revision_;
  clear_history();
}
uint64_t Volume::add_layer(std::string name) {
  if (editing_)
    throw std::logic_error("Finish sculpt stroke first");
  layer_name(name);
  if (layers_.size() >= 16 || next_layer_ == UINT64_MAX)
    throw std::length_error("Sculpt layer limit reached");
  Layer l;
  l.id = next_layer_;
  l.name = std::move(name);
  layers_.push_back(std::move(l));
  ++next_layer_;
  active_ = layers_.back().id;
  ++revision_;
  return active_;
}
void Volume::select_layer(uint64_t id) {
  if (editing_)
    throw std::logic_error("Finish sculpt stroke first");
  layer(id);
  active_ = id;
}
void Volume::layer_properties(uint64_t id, std::string name, bool visible,
                              bool locked, float opacity) {
  if (editing_)
    throw std::logic_error("Finish sculpt stroke first");
  layer_name(name);
  if (!std::isfinite(opacity) || opacity < 0 || opacity > 1)
    throw std::invalid_argument("Invalid sculpt layer opacity");
  auto &current = layer(id);
  if (current.visible == visible && current.opacity == opacity) {
    current.name = std::move(name);
    current.locked = locked;
    ++revision_;
    clear_history();
    return;
  }
  auto staged = geometry_snapshot();
  auto &l = staged.layer(id);
  l.name = std::move(name);
  l.visible = visible;
  l.locked = locked;
  l.opacity = opacity;
  staged.recompose();
  layers_ = std::move(staged.layers_);
  cells_ = std::move(staged.cells_);
  geometry_history_ = std::move(staged.geometry_history_);
  revision_ = staged.revision_;
  clear_history();
}
void Volume::erase_layer(uint64_t id) {
  if (editing_)
    throw std::logic_error("Finish sculpt stroke first");
  layer(id);
  if (layers_.size() == 1)
    throw std::logic_error("Keep at least one sculpt layer");
  auto staged = geometry_snapshot();
  std::erase_if(staged.layers_, [&](const auto &l) { return l.id == id; });
  if (staged.active_ == id)
    staged.active_ = staged.layers_.front().id;
  staged.recompose();
  layers_ = std::move(staged.layers_);
  cells_ = std::move(staged.cells_);
  active_ = staged.active_;
  geometry_history_ = std::move(staged.geometry_history_);
  revision_ = staged.revision_;
  clear_history();
}

Volume Volume::geometry_snapshot() const {
  Volume result(voxel_, history_limit_);
  result.band_ = band_;
  result.cells_ = cells_;
  result.layers_ = layers_;
  result.active_ = active_;
  result.next_layer_ = next_layer_;
  result.revision_ = revision_;
  result.mask_ = mask_;
  result.settings_ = settings_;
  result.geometry_history_ = geometry_history_;
  return result;
}
float Volume::sample(Vec3 p) const {
  if (!finite(p) ||
      std::max({std::abs(p.x), std::abs(p.y), std::abs(p.z)}) > 1001)
    throw std::invalid_argument("Invalid field sample position");
  p = p / voxel_;
  Int3 q{int(std::floor(p.x)), int(std::floor(p.y)), int(std::floor(p.z))};
  Vec3 f = p - Vec3{float(q.x), float(q.y), float(q.z)};
  float result = 0;
  for (int z = 0; z < 2; ++z)
    for (int y = 0; y < 2; ++y)
      for (int x = 0; x < 2; ++x)
        result += at(q + Int3{x, y, z}) * (x ? f.x : 1 - f.x) *
                  (y ? f.y : 1 - f.y) * (z ? f.z : 1 - f.z);
  return result;
}
Vec3 Volume::gradient(Vec3 p) const {
  return normalized(
      {sample(p + Vec3{voxel_, 0, 0}) - sample(p - Vec3{voxel_, 0, 0}),
       sample(p + Vec3{0, voxel_, 0}) - sample(p - Vec3{0, voxel_, 0}),
       sample(p + Vec3{0, 0, voxel_}) - sample(p - Vec3{0, 0, voxel_})});
}
std::optional<Vec3> Volume::raycast(Vec3 origin,Vec3 direction,float maximum) const {
  return sculpt_detail::raycast_field(*this,origin,direction,maximum);
}

void Volume::begin_stroke() {
  if (editing_)
    throw std::logic_error("A stroke is already active");
  if (!settings_.mask_edit && !editable_layer())
    throw std::logic_error(
        "Active sculpt layer is hidden, locked or has zero opacity");
  editing_ = true;
  mask_stroke_ = settings_.mask_edit;
  mask_changed_ = false;
  if (mask_stroke_)
    mask_start_ = mask_;
  else {
    cells_start_ = cells_;
    delta_start_ = layer(active_).delta;
    geometry_start_ = geometry_history_;
  }
  before_delta_.clear();
  before_.clear();
}
void Volume::stamp(Brush b, size_t sample_budget) {
  stamp(b, sample_budget, {});
}
void Volume::stamp(Brush b, size_t sample_budget,
                   const std::function<bool()> &cancelled) {
  sculpt_detail::checkpoint(cancelled);
  if (!editing_)
    throw std::logic_error("Begin a stroke before sculpting");
  if (!finite(b.from) || !finite(b.to) || !finite(b.normal) ||
      !std::isfinite(b.radius) || b.radius <= 0 || b.radius > 5 ||
      !std::isfinite(b.strength) || b.strength < 0 || b.strength > 1 ||
      !std::isfinite(b.pressure) || b.pressure < 0 || b.pressure > 1 ||
      !std::isfinite(b.blend) || b.blend < 0 || b.blend > b.radius ||
      int(b.tool) < 0 || int(b.tool) > 3 ||
      b.operation != BrushOperation::Sculpt || b.symmetry_axes != 0)
    throw std::invalid_argument("Invalid sculpt brush");
  if (std::max({std::abs(b.from.x), std::abs(b.from.y), std::abs(b.from.z),
                std::abs(b.to.x), std::abs(b.to.y), std::abs(b.to.z)}) > 100)
    throw std::invalid_argument("Sculpt position exceeds the workspace bounds");
  if (!b.pressure || !b.strength)
    return;
  b.band = band();
  b.normal = normalized(b.normal);
  b.symmetry_axes = settings_.symmetry_axes;
  b.symmetry_origin = settings_.symmetry_origin;
  if (mask_stroke_) {
    b.operation = settings_.mask_tool == MaskTool::Add ? BrushOperation::MaskAdd
                  : settings_.mask_tool == MaskTool::Subtract
                      ? BrushOperation::MaskSubtract
                      : BrushOperation::MaskSmooth;
    b.strength *= settings_.mask_strength;
    if (!b.strength)
      return;
  }
  float margin = b.radius + b.blend + voxel_;
  std::vector<Int3> keys;
  std::unordered_map<Int3, bool, IntHash> visited;
  uint64_t work = 0;
  const int domain = int(101 / voxel_);
  for (auto &image : sculpt_detail::images(b)) {
    if (std::max({std::abs(image.from.x), std::abs(image.from.y),
                  std::abs(image.from.z), std::abs(image.to.x),
                  std::abs(image.to.y), std::abs(image.to.z)}) > 100)
      throw std::invalid_argument(
          "Mirrored sculpt position exceeds the workspace bounds");
    Int3 lo{
        std::max(-domain,
                 int(std::floor((std::min(image.from.x, image.to.x) - margin) /
                                voxel_))),
        std::max(-domain,
                 int(std::floor((std::min(image.from.y, image.to.y) - margin) /
                                voxel_))),
        std::max(-domain,
                 int(std::floor((std::min(image.from.z, image.to.z) - margin) /
                                voxel_)))};
    Int3 hi{
        std::min(domain,
                 int(std::ceil((std::max(image.from.x, image.to.x) + margin) /
                               voxel_))),
        std::min(domain,
                 int(std::ceil((std::max(image.from.y, image.to.y) + margin) /
                               voxel_))),
        std::min(domain,
                 int(std::ceil((std::max(image.from.z, image.to.z) + margin) /
                               voxel_)))};
    uint64_t nx = hi.x - lo.x + 1, ny = hi.y - lo.y + 1, nz = hi.z - lo.z + 1;
    constexpr uint64_t maximum = 2'000'000;
    if (nx > maximum || ny > maximum / nx || nz > maximum / (nx * ny) ||
        nx * ny * nz > maximum - work)
      throw std::length_error(
          "Brush/symmetry support exceeds the interactive voxel budget");
    work += nx * ny * nz;
    // Enumerate each mirror's own box, never the possibly enormous empty gap.
    for (int z = lo.z; z <= hi.z; ++z)
      for (int y = lo.y; y <= hi.y; ++y)
        for (int x = lo.x; x <= hi.x; ++x) {
          if ((keys.size() & 1023) == 0)
            sculpt_detail::checkpoint(cancelled);
          Int3 q{x, y, z};
          if (visited.emplace(q, true).second)
            keys.push_back(q);
        }
  }
  std::vector<Sample> samples;
  samples.reserve(keys.size());
  for (size_t i = 0; i < keys.size(); ++i) {
    if ((i & 1023) == 0)
      sculpt_detail::checkpoint(cancelled);
    auto q = keys[i];
    float average = 0;
    bool uniform_mask = true;
    float current_mask = mask_stroke_ ? mask_at(q) : 0;
    for (Int3 n : {Int3{1, 0, 0},
                   {-1, 0, 0},
                   {0, 1, 0},
                   {0, -1, 0},
                   {0, 0, 1},
                   {0, 0, -1}}) {
      float neighbour = mask_stroke_ ? mask_at(q + n) : at(q + n);
      average += neighbour / 6;
      if (mask_stroke_ && neighbour != current_mask)
        uniform_mask = false;
    }
    // A uniform protection default must not acquire sparse float-roundoff
    // exceptions merely because six equal values were summed for smoothing.
    if (mask_stroke_ && uniform_mask)
      average = current_mask;
    samples.push_back({{q.x * voxel_, q.y * voxel_, q.z * voxel_},
                       mask_stroke_ ? mask_at(q) : at(q),
                       average,
                       mask_stroke_ ? 1.f : 1.f - mask_at(q)});
  }
  sculpt_detail::checkpoint(cancelled);
  // One selected-backend dispatch for all mirrors and mask modes. Protection
  // scales the kernel delta without altering pressure-dependent radius.
  apply_brush(samples, b);
  sculpt_detail::checkpoint(cancelled);
  for (auto &sample : samples)
    if (!std::isfinite(sample.distance) ||
        (mask_stroke_ && (sample.distance < 0 || sample.distance > 1)))
      throw std::runtime_error(
          "Compute backend returned an invalid sculpt sample");
  size_t extra = 0, removed = 0;
  auto &target = layer(active_).delta;
  for (size_t i = 0; i < keys.size(); ++i) {
    auto q = keys[i];
    float old = mask_stroke_ ? mask_at(q) : at(q);
    if (samples[i].distance != old) {
      if (mask_stroke_ && mask_.values.contains(q) &&
          samples[i].distance == mask_.background)
        ++removed;
      else if (mask_stroke_ ? !mask_.values.contains(q) : !target.contains(q))
        ++extra;
    }
  }
  auto limit = std::min(size_t(4'000'000), sample_budget);
  if (stored_samples() - removed > limit ||
      extra > limit - (stored_samples() - removed))
    throw std::length_error("Sculpt layer/mask storage budget exceeded");
  if (mask_stroke_) {
    auto staged = mask_;
    bool changed = false;
    for (size_t i = 0; i < keys.size(); ++i) {
      if ((i & 1023) == 0)
        sculpt_detail::checkpoint(cancelled);
      float value = samples[i].distance;
      auto q = keys[i];
      if (value == mask_at(q))
        continue;
      changed = true;
      if (value == staged.background)
        staged.values.erase(q);
      else
        staged.values[q] = value;
    }
    sculpt_detail::checkpoint(cancelled);
    if (changed) {
      mask_ = std::move(staged);
      mask_changed_ = true;
      ++revision_;
    }
  } else {
    // Prepare all allocations before publishing any field, displacement or
    // stroke-history mutation. Cancellation/allocation failure is
    // transactional.
    auto staged_cells = cells_;
    auto staged_delta = target;
    auto staged_before = before_;
    auto staged_before_delta = before_delta_;
    bool changed = false;
    GeometryChange dirty;
    dirty.kind = GeometryChange::Kind::Local;
    for (size_t i = 0; i < keys.size(); ++i) {
      if ((i & 1023) == 0)
        sculpt_detail::checkpoint(cancelled);
      auto q = keys[i];
      float old = at(q), value = samples[i].distance;
      if (value == old)
        continue;
      changed = true;
      geometry_include(dirty, q);
      staged_before.try_emplace(q, old);
      auto found = target.find(q);
      staged_before_delta.try_emplace(q, found == target.end() ? 0
                                                               : found->second);
      staged_delta[q] =
          (double(value) - sum(q, active_)) / layer(active_).opacity;
      if (value >= band())
        staged_cells.erase(q);
      else
        staged_cells[q] = value;
    }
    sculpt_detail::checkpoint(cancelled);
    if (changed) {
      auto history = prepare_geometry_history(staged_cells, dirty);
      sculpt_detail::checkpoint(cancelled);
      cells_ = std::move(staged_cells);
      geometry_history_ = std::move(history);
      target = std::move(staged_delta);
      before_ = std::move(staged_before);
      before_delta_ = std::move(staged_before_delta);
      ++revision_;
    }
  }
}

bool Volume::end_stroke() {
  if (!editing_)
    return false;
  if (mask_stroke_) {
    bool changed = !same_mask(mask_, *mask_start_);
    if (changed) {
      Command command;
      command.mask_before = *mask_start_;
      command.mask_after = mask_;
      push_command(std::move(command));
    }
    mask_start_.reset();
    mask_changed_ = false;
    mask_stroke_ = false;
    editing_ = false;
    return changed;
  }
  Command command;
  command.layer = active_;
  command.reserve(before_.size());
  for (auto [p, old] : before_)
    if (old != at(p))
      command.push_back(
          {p, old, at(p), before_delta_.at(p), layer(active_).delta.at(p)});
  bool changed = !command.empty();
  if (changed) {
    auto delta = layer(active_).delta;
    for (auto [p, old] : before_)
      if (old == at(p)) {
        auto d = before_delta_.at(p);
        if (d == 0)
          delta.erase(p);
        else
          delta[p] = d;
      }
    push_command(std::move(command));
    layer(active_).delta = std::move(delta);
  }
  if (!changed) {
    cells_ = std::move(*cells_start_);
    layer(active_).delta = std::move(*delta_start_);
    geometry_history_ = std::move(geometry_start_);
  }
  cells_start_.reset();
  delta_start_.reset();
  geometry_start_.reset();
  editing_ = false;
  before_.clear();
  before_delta_.clear();
  return changed;
}

void Volume::cancel_stroke() {
  if (mask_stroke_) {
    mask_ = std::move(*mask_start_);
    mask_start_.reset();
    mask_stroke_ = mask_changed_ = editing_ = false;
    ++revision_;
    return;
  }
  if (!editing_)
    return;
  cells_ = std::move(*cells_start_);
  layer(active_).delta = std::move(*delta_start_);
  geometry_history_ = std::move(geometry_start_);
  cells_start_.reset();
  delta_start_.reset();
  geometry_start_.reset();
  before_delta_.clear();
  before_.clear();
  editing_ = false;
  ++revision_;
}

bool Volume::undo() {
  if (editing_)
    throw std::logic_error("End the stroke before undo");
  if (undo_.empty())
    return false;
  const auto &c = undo_.back();
  auto next_cells = cells_;
  std::optional<CowMap<Int3, double, IntHash>> next_delta;
  if (!c.empty())
    next_delta = layer(c.layer).delta;
  GeometryChange dirty;
  dirty.kind = GeometryChange::Kind::Local;
  for (auto e : c) {
    if (e.before != at(e.key))
      geometry_include(dirty, e.key);
    if (e.delta_before == 0)
      next_delta->erase(e.key);
    else
      (*next_delta)[e.key] = e.delta_before;
    if (e.before >= band())
      next_cells.erase(e.key);
    else
      next_cells[e.key] = e.before;
  }
  auto history = prepare_geometry_history(next_cells, dirty);
  redo_.emplace_back(); // Allocate history destination before publishing state.
  if (c.mask_before)
    mask_ = *c.mask_before;
  if (c.settings_before)
    settings_ = *c.settings_before;
  if (next_delta)
    layer(c.layer).delta = std::move(*next_delta);
  cells_ = std::move(next_cells);
  geometry_history_ = std::move(history);
  ++revision_;
  redo_.back() = std::move(undo_.back());
  undo_.pop_back();
  return true;
}
bool Volume::redo() {
  if (editing_)
    throw std::logic_error("End the stroke before redo");
  if (redo_.empty())
    return false;
  const auto &c = redo_.back();
  auto next_cells = cells_;
  std::optional<CowMap<Int3, double, IntHash>> next_delta;
  if (!c.empty())
    next_delta = layer(c.layer).delta;
  GeometryChange dirty;
  dirty.kind = GeometryChange::Kind::Local;
  for (auto e : c) {
    if (e.after != at(e.key))
      geometry_include(dirty, e.key);
    if (e.delta_after == 0)
      next_delta->erase(e.key);
    else
      (*next_delta)[e.key] = e.delta_after;
    if (e.after >= band())
      next_cells.erase(e.key);
    else
      next_cells[e.key] = e.after;
  }
  auto history = prepare_geometry_history(next_cells, dirty);
  undo_.emplace_back();
  if (c.mask_after)
    mask_ = *c.mask_after;
  if (c.settings_after)
    settings_ = *c.settings_after;
  if (next_delta)
    layer(c.layer).delta = std::move(*next_delta);
  cells_ = std::move(next_cells);
  geometry_history_ = std::move(history);
  ++revision_;
  undo_.back() = std::move(redo_.back());
  redo_.pop_back();
  return true;
}

void Volume::save(const std::filesystem::path &path) const {
  auto bytes = nlohmann::json::to_cbor(encode());
  if (bytes.size() > 128 * 1024 * 1024)
    throw std::length_error(
        "Volume project exceeds reloadable file size limit");
  atomic_write(path, bytes);
}
nlohmann::json Volume::encode() const {
  if (editing_)
    throw std::logic_error("End the stroke before saving");
  nlohmann::json j = {{"format", "dome-sculpt"},
                      {"version", 2},
                      {"voxel_size", voxel_},
                      {"samples", nlohmann::json::array()}};
  for (auto [p, v] : cells_)
    j["samples"].push_back({p.x, p.y, p.z, v});
  j["sculpt_layers"] = nlohmann::json::array();
  j["active_sculpt_layer"] = active_;
  j["next_sculpt_layer"] = next_layer_;
  for (auto &l : layers_) {
    auto ds = nlohmann::json::array();
    for (auto &[p, d] : l.delta)
      if (d != 0)
        ds.push_back({p.x, p.y, p.z, d});
    j["sculpt_layers"].push_back({{"id", l.id},
                                  {"name", l.name},
                                  {"visible", l.visible},
                                  {"locked", l.locked},
                                  {"opacity", l.opacity},
                                  {"delta", std::move(ds)}});
  }
  if (band_ != voxel_ * 4 || mask_.background != 0 || mask_.values.size() || !same_settings(settings_, SculptSettings{})) {
    j["version"] = 3;
    j["sculpt_mask"] = {{"default", mask_.background},
                        {"values", nlohmann::json::array()}};
    for (auto &[p, value] : mask_.values)
      j["sculpt_mask"]["values"].push_back({p.x, p.y, p.z, value});
    auto &c = settings_;
    j["sculpt_controls"] = {
        {"symmetry_axes", c.symmetry_axes},
        {"symmetry_origin",
         {c.symmetry_origin.x, c.symmetry_origin.y, c.symmetry_origin.z}},
        {"mask_edit", c.mask_edit},
        {"mask_visible", c.mask_visible},
        {"mask_tool", int(c.mask_tool)},
        {"mask_strength", c.mask_strength}};
  }
  if (band_ != voxel_ * 4) { j["version"] = 4; j["distance_band"] = band_; }
  return j;
}
Volume Volume::load(const std::filesystem::path &path) {
  if (std::filesystem::file_size(path) > 128 * 1024 * 1024)
    throw std::length_error("Project exceeds file size limit");
  std::ifstream input(path, std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
  auto j = nlohmann::json::from_cbor(bytes);
  return decode(j);
}
Volume Volume::decode(const nlohmann::json &j) {
  const auto header=archive_detail::volume_header(j);
  const int version=header.version;
  Volume v(header.voxel);
  v.band_=header.band;
  auto &samples = j.at("samples");
  if (!samples.is_array() || samples.size() > 4'000'000)
    throw std::length_error("Invalid project sample count");
  for (auto &s : samples) {
    if (!s.is_array() || s.size() != 4)
      throw std::runtime_error("Invalid project sample");
    auto coordinate = [&](int axis) {
      if (!s[axis].is_number_integer())
        throw std::runtime_error(
            "Project coordinates must be lattice integers");
      if (s[axis].is_number_unsigned() &&
          s[axis].get<uint64_t>() > uint64_t(INT32_MAX))
        throw std::runtime_error("Project coordinate overflow");
      auto value = s[axis].get<int64_t>();
      auto limit = int64_t(101 / v.voxel_);
      if (value < -limit || value > limit)
        throw std::runtime_error("Project coordinate exceeds workspace bounds");
      return int(value);
    };
    Int3 p{coordinate(0), coordinate(1), coordinate(2)};
    float d = s[3].get<float>();
    if (!std::isfinite(d) || std::abs(d) > v.band() ||
        std::max({std::abs(int64_t(p.x)), std::abs(int64_t(p.y)),
                  std::abs(int64_t(p.z))}) > int64_t(101 / v.voxel_))
      throw std::runtime_error("Invalid project distance or position");
    if (v.cells_.contains(p))
      throw std::runtime_error("Duplicate project sample");
    v.cells_.emplace(p, d);
  }
  std::vector<Int3> outside;
  for (auto &[p, d] : v.cells_)
    if (d >= v.band())
      outside.push_back(p);
  for (auto p : outside)
    v.cells_.erase(p);
  if (j.at("version") >= 2 && !j.contains("sculpt_layers"))
    throw std::runtime_error("Missing sculpt layer data");
  if (j.contains("sculpt_layers")) {
    auto &a = j.at("sculpt_layers");
    if (!a.is_array() || a.empty() || a.size() > 16)
      throw std::runtime_error("Invalid sculpt layer count");
    auto id = [](const nlohmann::json &n) {
      if (!n.is_number_integer() || n.get<int64_t>() <= 0)
        throw std::runtime_error("Invalid sculpt layer ID");
      return n.get<uint64_t>();
    };
    v.active_ = id(j.at("active_sculpt_layer"));
    v.next_layer_ = id(j.at("next_sculpt_layer"));
    v.layers_.clear();
    std::set<uint64_t> ids;
    size_t count = 0;
    for (auto &item : a) {
      Layer l;
      l.id = id(item.at("id"));
      if (l.id >= v.next_layer_ || !ids.insert(l.id).second)
        throw std::runtime_error("Duplicate sculpt layer ID");
      l.name = item.at("name").get<std::string>();
      layer_name(l.name);
      l.visible = item.at("visible");
      l.locked = item.at("locked");
      l.opacity = item.at("opacity");
      if (!std::isfinite(l.opacity) || l.opacity < 0 || l.opacity > 1)
        throw std::runtime_error("Invalid sculpt layer opacity");
      auto &ds = item.at("delta");
      if (!ds.is_array() || ds.size() > 4'000'000 ||
          (count += ds.size()) > 4'000'000)
        throw std::length_error("Sculpt layer storage budget exceeded");
      for (auto &s : ds) {
        if (!s.is_array() || s.size() != 4)
          throw std::runtime_error("Invalid sculpt displacement");
        Int3 p;
        int *axes[] = {&p.x, &p.y, &p.z};
        for (int k = 0; k < 3; k++) {
          if (!s[k].is_number_integer())
            throw std::runtime_error("Invalid displacement coordinate");
          if (s[k].is_number_unsigned() &&
              s[k].get<uint64_t>() > uint64_t(INT32_MAX))
            throw std::runtime_error("Displacement coordinate overflow");
          auto x = s[k].get<int64_t>();
          if (x < -int64_t(101 / v.voxel_) || x > int64_t(101 / v.voxel_))
            throw std::runtime_error("Displacement outside workspace");
          *axes[k] = int(x);
        }
        double d = s[3].get<double>();
        if (!std::isfinite(d) || std::abs(d) > 1e6 ||
            !l.delta.emplace(p, d).second)
          throw std::runtime_error("Invalid sculpt displacement value");
      }
      v.layers_.push_back(std::move(l));
    }
    v.layer(v.active_);
    auto stored = v.cells_;
    v.recompose();
    if (v.cells_.size() != stored.size())
      throw std::runtime_error("Sculpt layer cache mismatch");
    for (auto &[p, d] : stored)
      if (std::abs(v.at(p) - d) > v.voxel_ * 1e-5f)
        throw std::runtime_error("Sculpt layer cache mismatch");
    v.cells_ = std::move(stored);
  } else {
    for (auto &[p, d] : v.cells_)
      v.layers_.front().delta[p] = double(d) - v.band();
  }
  if (version >= 3) {
    auto &mask = j.at("sculpt_mask");
    v.mask_.background = mask.at("default").get<float>();
    if (!std::isfinite(v.mask_.background) || v.mask_.background < 0 ||
        v.mask_.background > 1)
      throw std::runtime_error("Invalid sculpt mask default");
    auto &values = mask.at("values");
    if (!values.is_array() || values.size() > 4'000'000 ||
        values.size() > 4'000'000 - v.stored_samples())
      throw std::length_error("Sculpt mask/layer storage budget exceeded");
    for (auto &item : values) {
      if (!item.is_array() || item.size() != 4)
        throw std::runtime_error("Invalid sculpt mask entry");
      Int3 p;
      int *axes[] = {&p.x, &p.y, &p.z};
      for (int k = 0; k < 3; ++k) {
        if (!item[k].is_number_integer() ||
            (item[k].is_number_unsigned() &&
             item[k].get<uint64_t>() > uint64_t(INT32_MAX)))
          throw std::runtime_error("Invalid sculpt mask coordinate");
        auto coordinate = item[k].get<int64_t>();
        auto limit = int64_t(101 / v.voxel_);
        if (coordinate < -limit || coordinate > limit)
          throw std::runtime_error("Mask coordinate outside workspace");
        *axes[k] = int(coordinate);
      }
      float value = item[3].get<float>();
      if (!std::isfinite(value) || value < 0 || value > 1 ||
          value == v.mask_.background ||
          !v.mask_.values.emplace(p, value).second)
        throw std::runtime_error("Invalid or duplicate sculpt mask value");
    }
    auto &controls = j.at("sculpt_controls");
    if (!controls.at("symmetry_axes").is_number_unsigned() &&
        !controls.at("symmetry_axes").is_number_integer())
      throw std::runtime_error("Invalid symmetry axes");
    auto bits = controls.at("symmetry_axes").get<int64_t>();
    if (bits < 0 || bits > 7)
      throw std::runtime_error("Invalid symmetry axes");
    auto origin = controls.at("symmetry_origin").get<std::array<float, 3>>();
    SculptSettings settings;
    settings.symmetry_axes = unsigned(bits);
    settings.symmetry_origin = {origin[0], origin[1], origin[2]};
    settings.mask_edit = controls.at("mask_edit").get<bool>();
    settings.mask_visible = controls.at("mask_visible").get<bool>();
    auto &tool_value = controls.at("mask_tool");
    if (!tool_value.is_number_integer() ||
        (tool_value.is_number_unsigned() && tool_value.get<uint64_t>() > 2))
      throw std::runtime_error("Invalid sculpt mask tool");
    auto tool = tool_value.get<int64_t>();
    if (tool < 0 || tool > 2)
      throw std::runtime_error("Invalid sculpt mask tool");
    settings.mask_tool = MaskTool(tool);
    settings.mask_strength = controls.at("mask_strength").get<float>();
    v.sculpt_settings(settings);
    v.clear_history();
  } else if (j.contains("sculpt_mask") || j.contains("sculpt_controls")) {
    throw std::runtime_error("Sculpt mask/controls require volume version 3 or later");
  }
  v.geometry_history_.reset();
  return v;
}
} // namespace dome
