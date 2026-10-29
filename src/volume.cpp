// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/volume.hpp"
#include "io.hpp"
#include <fcntl.h>
#include <fstream>
#include <limits>
#include <nlohmann/json.hpp>
#include <unistd.h>
namespace dome {
Volume::Volume(float voxel, size_t limit)
    : voxel_(voxel), history_limit_(limit) {
  if (!std::isfinite(voxel) || voxel < .00001f || voxel > 1)
    throw std::invalid_argument(
        "Voxel size must be between 0.00001 and 1 metres");
}
float Volume::at(Int3 p) const {
  auto it = cells_.find(p);
  return it == cells_.end() ? band() : it->second;
}
void Volume::set(Int3 p, float v) {
  if (at(p) == v)
    return;
  ++revision_;
  if (v >= band())
    cells_.erase(p);
  else
    cells_[p] = v;
}
Volume Volume::geometry_snapshot() const {
  Volume result(voxel_, history_limit_);
  result.cells_ = cells_;
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
  editing_ = true;
  before_.clear();
}
void Volume::stamp(Brush b) {
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
  for (size_t i = 0; i < keys.size(); ++i) {
    float old = at(keys[i]), value = samples[i].distance;
    if (value == old)
      continue;
    before_.try_emplace(keys[i], old);
    set(keys[i], value);
  }
}
bool Volume::end_stroke() {
  if (!editing_)
    return false;
  Command command;
  command.reserve(before_.size());
  for (auto [p, old] : before_)
    if (old != at(p))
      command.push_back({p, old, at(p)});
  editing_ = false;
  before_.clear();
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
  for (auto [p, old] : before_)
    set(p, old);
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
  for (auto e : c)
    set(e.key, e.before);
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
  for (auto e : c)
    set(e.key, e.after);
  undo_.push_back(std::move(c));
  return true;
}
void Volume::save(const std::filesystem::path &path) const {
  if (editing_)
    throw std::logic_error("End the stroke before saving");
  nlohmann::json j = {{"format", "dome-sculpt"},
                      {"version", 1},
                      {"voxel_size", voxel_},
                      {"samples", nlohmann::json::array()}};
  for (auto [p, v] : cells_)
    j["samples"].push_back({p.x, p.y, p.z, v});
  auto bytes = nlohmann::json::to_cbor(j);
  atomic_write(path, bytes);
}
Volume Volume::load(const std::filesystem::path &path) {
  if (std::filesystem::file_size(path) > 128 * 1024 * 1024)
    throw std::length_error("Project exceeds file size limit");
  std::ifstream input(path, std::ios::binary);
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
  auto j = nlohmann::json::from_cbor(bytes);
  if (j.at("format") != "dome-sculpt" || j.at("version") != 1)
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
  return v;
}
} // namespace dome
