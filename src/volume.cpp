// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/volume.hpp"
#include "io.hpp"
#include <fcntl.h>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <set>
#include <unistd.h>
namespace dome {
Volume::Volume(float voxel, size_t limit)
    : voxel_(voxel), history_limit_(limit) {
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
void Volume::raw_set(Int3 p, float v) {
  if (at(p) == v)
    return;
  ++revision_;
  if (v >= band())
    cells_.erase(p);
  else
    cells_[p] = v;
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
  return n;
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
void Volume::set(Int3 p, float v) {
  auto &l = layer(active_);
  l.delta[p] = (double(v) - sum(p, active_)) / l.opacity;
  raw_set(p, v);
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
  cells_ = std::move(result);
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
  l.id = next_layer_++;
  l.name = std::move(name);
  layers_.push_back(std::move(l));
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
  auto &l = layer(id);
  l.name = std::move(name);
  l.visible = visible;
  l.locked = locked;
  l.opacity = opacity;
  recompose();
}
void Volume::erase_layer(uint64_t id) {
  if (editing_)
    throw std::logic_error("Finish sculpt stroke first");
  layer(id);
  if (layers_.size() == 1)
    throw std::logic_error("Keep at least one sculpt layer");
  std::erase_if(layers_, [&](auto &l) { return l.id == id; });
  if (active_ == id)
    active_ = layers_.front().id;
  recompose();
}

Volume Volume::geometry_snapshot() const {
  Volume result(voxel_, history_limit_);
  result.cells_ = cells_;
  result.layers_ = layers_;
  result.active_ = active_;
  result.next_layer_ = next_layer_;
  result.revision_ = revision_;
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
std::optional<Vec3> Volume::raycast(Vec3 origin, Vec3 direction,
                                    float maximum) const {
  if (!finite(origin) || !finite(direction) || length(direction) < 1e-8f ||
      std::max({std::abs(origin.x), std::abs(origin.y), std::abs(origin.z)}) +
              maximum >
          1000 ||
      !std::isfinite(maximum) || maximum <= 0 || maximum > 1000)
    throw std::invalid_argument("Invalid sculpt ray");
  direction = normalized(direction);
  float t = 0;
  for (int step = 0; step < 20000 && t <= maximum; ++step) {
    Vec3 p = origin + direction * t;
    float d = sample(p);
    if (d < voxel_ * .12f)
      return p;
    t += std::max(voxel_ * .1f, d * .8f);
  }
  return {};
}
void Volume::begin_stroke() {
  if (editing_)
    throw std::logic_error("A stroke is already active");
  if (!editable_layer())
    throw std::logic_error(
        "Active sculpt layer is hidden, locked or has zero opacity");
  editing_ = true;
  before_delta_.clear();
  before_.clear();
}
void Volume::stamp(Brush b, size_t sample_budget) {
  if (!editing_)
    throw std::logic_error("Begin a stroke before sculpting");
  if (!finite(b.from) || !finite(b.to) || !finite(b.normal) ||
      !std::isfinite(b.radius) || b.radius <= 0 || b.radius > 5 ||
      !std::isfinite(b.strength) || b.strength < 0 || b.strength > 1 ||
      !std::isfinite(b.pressure) || b.pressure < 0 || b.pressure > 1 ||
      !std::isfinite(b.blend) || b.blend < 0 || b.blend > b.radius ||
      int(b.tool) < 0 || int(b.tool) > 3)
    throw std::invalid_argument("Invalid sculpt brush");
  if (std::max({std::abs(b.from.x), std::abs(b.from.y), std::abs(b.from.z),
                std::abs(b.to.x), std::abs(b.to.y), std::abs(b.to.z)}) > 100)
    throw std::invalid_argument("Sculpt position exceeds the workspace bounds");
  if (!b.pressure || !b.strength)
    return;
  b.band = band();
  b.normal = normalized(b.normal);
  float margin = b.radius + b.blend + voxel_;
  Int3 lo{int(std::floor((std::min(b.from.x, b.to.x) - margin) / voxel_)),
          int(std::floor((std::min(b.from.y, b.to.y) - margin) / voxel_)),
          int(std::floor((std::min(b.from.z, b.to.z) - margin) / voxel_))};
  Int3 hi{int(std::ceil((std::max(b.from.x, b.to.x) + margin) / voxel_)),
          int(std::ceil((std::max(b.from.y, b.to.y) + margin) / voxel_)),
          int(std::ceil((std::max(b.from.z, b.to.z) + margin) / voxel_))};
  uint64_t nx = hi.x - lo.x + 1, ny = hi.y - lo.y + 1, nz = hi.z - lo.z + 1;
  if (nx > 2'000'000 || ny > 2'000'000 || nz > 2'000'000)
    throw std::length_error("Brush bounds exceed the interactive voxel budget");
  uint64_t count = nx * ny * nz;
  if (count > 2'000'000)
    throw std::length_error("Brush sweep exceeds the interactive voxel budget; "
                            "shorten it or increase voxel size");
  std::vector<Sample> samples;
  std::vector<Int3> keys;
  samples.reserve(count);
  keys.reserve(count);
  for (int z = lo.z; z <= hi.z; ++z)
    for (int y = lo.y; y <= hi.y; ++y)
      for (int x = lo.x; x <= hi.x; ++x) {
        Int3 q{x, y, z};
        float average = 0;
        for (Int3 n : {Int3{1, 0, 0},
                       {-1, 0, 0},
                       {0, 1, 0},
                       {0, -1, 0},
                       {0, 0, 1},
                       {0, 0, -1}})
          average += at(q + n) / 6;
        keys.push_back(q);
        samples.push_back(
            {{x * voxel_, y * voxel_, z * voxel_}, at(q), average});
      }
  // Compute into detached storage. A failed GPU operation cannot partially edit
  // the volume.
  apply_brush(samples, b);
  for (auto s : samples)
    if (!std::isfinite(s.distance))
      throw std::runtime_error(
          "Compute backend returned a non-finite distance");
  size_t extra = 0;
  auto &target = layer(active_).delta;
  for (size_t i = 0; i < keys.size(); ++i)
    if (samples[i].distance != at(keys[i]) && !target.contains(keys[i]))
      extra++;
  if (stored_samples() + extra > std::min(size_t(4'000'000), sample_budget))
    throw std::length_error("Sculpt layer storage budget exceeded");
  for (size_t i = 0; i < keys.size(); ++i) {
    float old = at(keys[i]), value = samples[i].distance;
    if (value == old)
      continue;
    before_.try_emplace(keys[i], old);
    auto &d = layer(active_).delta;
    auto it = d.find(keys[i]);
    before_delta_.try_emplace(keys[i], it == d.end() ? 0 : it->second);
    set(keys[i], value);
  }
}
bool Volume::end_stroke() {
  if (!editing_)
    return false;
  Command command;
  command.layer = active_;
  command.reserve(before_.size());
  for (auto [p, old] : before_)
    if (old == at(p)) {
      auto d = before_delta_.at(p);
      if (d == 0)
        layer(active_).delta.erase(p);
      else
        layer(active_).delta[p] = d;
    } else
      command.push_back(
          {p, old, at(p), before_delta_.at(p), layer(active_).delta.at(p)});
  editing_ = false;
  before_.clear();
  before_delta_.clear();
  if (command.empty())
    return false;
  for (auto &c : redo_)
    history_bytes_ -= c.size() * sizeof(Edit);
  redo_.clear();
  history_bytes_ += command.size() * sizeof(Edit);
  undo_.push_back(std::move(command));
  while (history_bytes_ > history_limit_ && undo_.size() > 1) {
    history_bytes_ -= undo_.front().size() * sizeof(Edit);
    undo_.pop_front();
  }
  return true;
}
void Volume::cancel_stroke() {
  for (auto [p, old] : before_) {
    auto d = before_delta_.at(p);
    auto &delta = layer(active_).delta;
    if (d == 0)
      delta.erase(p);
    else
      delta[p] = d;
    raw_set(p, old);
  }
  before_delta_.clear();
  before_.clear();
  editing_ = false;
}
bool Volume::undo() {
  if (editing_)
    throw std::logic_error("End the stroke before undo");
  if (undo_.empty())
    return false;
  auto c = std::move(undo_.back());
  undo_.pop_back();
  for (auto e : c) {
    auto &d = layer(c.layer).delta;
    if (e.delta_before == 0)
      d.erase(e.key);
    else
      d[e.key] = e.delta_before;
    raw_set(e.key, e.before);
  }
  redo_.push_back(std::move(c));
  return true;
}
bool Volume::redo() {
  if (editing_)
    throw std::logic_error("End the stroke before redo");
  if (redo_.empty())
    return false;
  auto c = std::move(redo_.back());
  redo_.pop_back();
  for (auto e : c) {
    auto &d = layer(c.layer).delta;
    if (e.delta_after == 0)
      d.erase(e.key);
    else
      d[e.key] = e.delta_after;
    raw_set(e.key, e.after);
  }
  undo_.push_back(std::move(c));
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
  if (j.at("format") != "dome-sculpt" ||
      (j.at("version") != 1 && j.at("version") != 2))
    throw std::runtime_error("Unsupported project format");
  Volume v(j.at("voxel_size").get<float>());
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
  if (j.at("version") == 2 && !j.contains("sculpt_layers"))
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
  return v;
}
} // namespace dome
