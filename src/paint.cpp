// SPDX-License-Identifier: GPL-3.0-or-later
// Ray/triangle picking adapted from Blender 5.4 Alpha isect_ray_tri_v3,
// GPL-2.0-or-later; see third_party/blender/math_geom.cc.reference.
#include "dome/paint.hpp"
#include "dome/scene.hpp"
#include <bit>
#include <cstring>
#include <nlohmann/json.hpp>
#include <png.h>
#include <set>
namespace dome {
namespace {
uint8_t byte(float v) {
  return uint8_t(std::lround(std::clamp(v, 0.f, 1.f) * 255));
}
float unit(uint8_t v) { return float(v) / 255; }
void name_valid(const std::string &s) {
  if (s.empty() || s.size() > 256 || s.find('\0') != std::string::npos)
    throw std::invalid_argument("Invalid paint layer name");
}
Vec3 low(Vec3 a, Vec3 b) {
  return {std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
}
Vec3 high(Vec3 a, Vec3 b) {
  return {std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
}
} // namespace
float srgb_to_linear(float v) {
  return v <= .04045f ? v / 12.92f : std::pow((v + .055f) / 1.055f, 2.4f);
}
float linear_to_srgb(float v) {
  v = std::max(0.f, v);
  return v <= .0031308f ? 12.92f * v : 1.055f * std::pow(v, 1 / 2.4f) - .055f;
}
void Image::validate() const {
  if (!width || !height || width > 8192 || height > 8192 ||
      rgba.size() != size_t(width) * height * 4)
    throw std::invalid_argument("Invalid texture image");
}
std::vector<uint8_t> Image::png() const {
  validate();
  png_image image{};
  struct Release {
    png_image *image;
    ~Release() { png_image_free(image); }
  } release{&image};
  image.version = PNG_IMAGE_VERSION;
  image.width = width;
  image.height = height;
  image.format = PNG_FORMAT_RGBA;
  png_alloc_size_t size = 0;
  if (!png_image_write_to_memory(&image, nullptr, &size, 0, rgba.data(), 0,
                                 nullptr))
    throw std::runtime_error(image.message);
  std::vector<uint8_t> bytes(size);
  if (!png_image_write_to_memory(&image, bytes.data(), &size, 0, rgba.data(), 0,
                                 nullptr))
    throw std::runtime_error(image.message);
  bytes.resize(size);
  return bytes;
}
void PaintSettings::validate() const {
  auto valid = [](float f) { return std::isfinite(f) && f >= 0 && f <= 1; };
  if (int(channel) < 0 || int(channel) > 4 ||
      !std::all_of(color.begin(), color.end(), valid) ||
      !std::all_of(emission.begin(), emission.end(), valid) ||
      !valid(metallic) || !valid(roughness) || !valid(mask) || !valid(flow))
    throw std::invalid_argument("Invalid paint brush settings");
}
struct PaintedMesh::SurfaceMap {
  Mesh mesh;
  uint32_t resolution, columns;
  struct Texel {
    Vec3 position, normal;
    bool covered = false;
  };
  struct Bounds {
    Vec3 lo{INFINITY, INFINITY, INFINITY}, hi{-INFINITY, -INFINITY, -INFINITY};
  };
  std::vector<Texel> texels;
  std::vector<Bounds> tiles;
  std::vector<int32_t> padding;
  SurfaceMap(Mesh input, uint32_t r)
      : mesh(std::move(input)), resolution(r), columns(r / 32),
        texels(size_t(r) * r), tiles(size_t(columns) * columns),
        padding(size_t(r) * r, -1) {
    mesh.validate();
    if (mesh.uv.size() != mesh.positions.size())
      throw std::invalid_argument("Painting requires persisted UV coordinates");
    for (auto uv : mesh.uv)
      if (uv[0] < 0 || uv[0] > 1 || uv[1] < 0 || uv[1] > 1)
        throw std::invalid_argument("Paint UVs must fit a single texture tile");
    for (size_t k = 0; k < mesh.indices.size(); k += 3) {
      uint32_t ids[] = {mesh.indices[k], mesh.indices[k + 1],
                        mesh.indices[k + 2]};
      auto a = mesh.uv[ids[0]], b = mesh.uv[ids[1]], c = mesh.uv[ids[2]];
      double den = double(b[1] - c[1]) * (a[0] - c[0]) +
                   double(c[0] - b[0]) * (a[1] - c[1]);
      if (std::abs(den) < 1e-14)
        throw std::invalid_argument(
            "Painting requires nondegenerate UV triangles");
      int x0 = std::max(0, int(std::floor(std::min({a[0], b[0], c[0]}) * r))),
          x1 = std::min(int(r) - 1,
                        int(std::ceil(std::max({a[0], b[0], c[0]}) * r)));
      int y0 = std::max(0, int(std::floor(std::min({a[1], b[1], c[1]}) * r))),
          y1 = std::min(int(r) - 1,
                        int(std::ceil(std::max({a[1], b[1], c[1]}) * r)));
      for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
          double u = (x + .5) / r, v = (y + .5) / r;
          float w0 = float(
              ((b[1] - c[1]) * (u - c[0]) + (c[0] - b[0]) * (v - c[1])) / den);
          float w1 = float(
                    ((c[1] - a[1]) * (u - c[0]) + (a[0] - c[0]) * (v - c[1])) /
                    den),
                w2 = 1 - w0 - w1;
          if (std::min({w0, w1, w2}) < -.000001f)
            continue;
          auto p = mesh.positions[ids[0]] * w0 + mesh.positions[ids[1]] * w1 +
                   mesh.positions[ids[2]] * w2;
          size_t index = size_t(y) * r + x;
          auto &t = texels[index];
          if (t.covered && length(t.position - p) > 1e-4f)
            throw std::invalid_argument(
                "Overlapping UV islands cannot be painted independently");
          t = {p,
               normalized(mesh.normals[ids[0]] * w0 +
                          mesh.normals[ids[1]] * w1 +
                          mesh.normals[ids[2]] * w2),
               true};
          padding[index] = int32_t(index);
          auto &bounds = tiles[size_t(y / 32) * columns + x / 32];
          bounds.lo = low(bounds.lo, p);
          bounds.hi = high(bounds.hi, p);
        }
    }
    if (std::none_of(texels.begin(), texels.end(),
                     [](auto &t) { return t.covered; }))
      throw std::invalid_argument(
          "UV islands have no covered texels at this resolution");
    for (int pass = 0; pass < 4; ++pass) {
      auto next = padding;
      for (int y = 0; y < int(r); ++y)
        for (int x = 0; x < int(r); ++x)
          if (padding[size_t(y) * r + x] < 0) {
            for (auto [dx, dy] : {std::pair{-1, 0}, {1, 0}, {0, -1}, {0, 1}}) {
              int a = x + dx, b = y + dy;
              if (a >= 0 && b >= 0 && a < int(r) && b < int(r) &&
                  padding[size_t(b) * r + a] >= 0) {
                next[size_t(y) * r + x] = padding[size_t(b) * r + a];
                break;
              }
            }
          }
      padding = std::move(next);
    }
  }
};
PaintedMesh::PaintedMesh(Mesh mesh, uint32_t r) {
  if (r < 64 || r > 2048 || (r & (r - 1)))
    throw std::invalid_argument(
        "Paint resolution must be a power of two between 64 and 2048");
  surface_ = std::make_shared<SurfaceMap>(std::move(mesh), r);
  PaintLayer layer;
  layer.id = 1;
  layer.tiles.resize(surface_->tiles.size());
  layers_.push_back(std::move(layer));
}
const Mesh &PaintedMesh::mesh() const { return surface_->mesh; }
uint32_t PaintedMesh::resolution() const { return surface_->resolution; }
const PaintLayer &PaintedMesh::layer(uint64_t id) const {
  auto i = std::find_if(layers_.begin(), layers_.end(),
                        [&](auto &l) { return l.id == id; });
  if (i == layers_.end())
    throw std::out_of_range("Unknown paint layer");
  return *i;
}
PaintLayer &PaintedMesh::active() {
  return const_cast<PaintLayer &>(layer(active_));
}
void PaintedMesh::select_layer(uint64_t id) {
  if (editing_)
    throw std::logic_error("Finish the paint stroke before changing layers");
  layer(id);
  active_ = id;
}
void PaintedMesh::add_layer(std::string name) {
  name_valid(name);
  if (editing_ || layers_.size() >= 16 || next_ == UINT64_MAX)
    throw std::length_error(
        "Finish the stroke or remove a layer before adding another");
  PaintLayer l;
  l.id = next_++;
  l.name = std::move(name);
  l.tiles.resize(surface_->tiles.size());
  active_ = l.id;
  layers_.push_back(std::move(l));
  ++revision_;
}
void PaintedMesh::delete_layer() {
  if (editing_ || layers_.size() == 1)
    throw std::logic_error("Keep at least one paint layer");
  std::erase_if(layers_, [&](auto &l) { return l.id == active_; });
  active_ = layers_.back().id;
  ++revision_;
}
void PaintedMesh::layer_properties(std::string name, bool visible,
                                   float opacity) {
  name_valid(name);
  if (editing_ || !std::isfinite(opacity) || opacity < 0 || opacity > 1)
    throw std::invalid_argument("Invalid paint layer properties");
  auto &l = active();
  l.name = std::move(name);
  l.visible = visible;
  l.opacity = opacity;
  ++revision_;
}
void PaintedMesh::move_layer(int direction) {
  if (editing_ || (direction != 1 && direction != -1))
    throw std::invalid_argument("Invalid layer move");
  auto i = std::find_if(layers_.begin(), layers_.end(),
                        [&](auto &l) { return l.id == active_; });
  auto index = i - layers_.begin();
  auto to = index + direction;
  if (to >= 0 && to < ptrdiff_t(layers_.size())) {
    std::swap(layers_[index], layers_[to]);
    ++revision_;
  }
}
const PaintPixel &PaintedMesh::pixel(uint64_t id, uint32_t x,
                                     uint32_t y) const {
  if (x >= resolution() || y >= resolution())
    throw std::out_of_range("Paint pixel out of bounds");
  auto &l = layer(id);
  auto &tile = l.tiles[size_t(y / 32) * surface_->columns + x / 32];
  static const PaintPixel empty;
  return tile ? tile->pixels[size_t(y % 32) * 32 + x % 32] : empty;
}
void PaintedMesh::begin_stroke() {
  if (editing_)
    throw std::logic_error("A paint stroke is already active");
  if (!active().visible)
    throw std::logic_error("Show the active paint layer before painting");
  before_ = active().tiles;
  editing_ = true;
}
bool PaintedMesh::pending_changes() const {
  return editing_ && before_ != layer(active_).tiles;
}
void PaintedMesh::stamp(const Brush &b, const PaintSettings &settings) {
  settings.validate();
  if (!editing_)
    throw std::logic_error("Begin the paint stroke first");
  if (!finite(b.from) || !finite(b.to) || !finite(b.normal) ||
      !std::isfinite(b.radius) || b.radius <= 0 || b.radius > 5 ||
      !std::isfinite(b.pressure) || b.pressure < 0 || b.pressure > 1 ||
      !std::isfinite(b.strength) || b.strength < 0 || b.strength > 1)
    throw std::invalid_argument("Invalid paint stroke");
  if (!b.pressure || !b.strength || !settings.flow)
    return;
  float radius = b.radius * std::sqrt(b.pressure);
  Vec3 margin{radius, radius, radius},
      lo = low(b.from, b.to) - margin, hi = high(b.from, b.to) + margin,
      axis = b.to - b.from, n = normalized(b.normal);
  float axis2 = dot(axis, axis);
  auto &l = active();
  auto prior = l.tiles;
  size_t allocated = 0;
  for (auto &layer : layers_)
    allocated += std::count_if(layer.tiles.begin(), layer.tiles.end(),
                               [](auto &p) { return bool(p); });
  bool changed = false;
  try {
    for (size_t tile = 0; tile < l.tiles.size(); ++tile) {
      auto bounds = surface_->tiles[tile];
      if (bounds.hi.x < lo.x || bounds.hi.y < lo.y || bounds.hi.z < lo.z ||
          bounds.lo.x > hi.x || bounds.lo.y > hi.y || bounds.lo.z > hi.z)
        continue;
      auto copy = l.tiles[tile] ? std::make_shared<PaintTile>(*l.tiles[tile])
                                : std::make_shared<PaintTile>();
      bool tile_changed = false;
      uint32_t tx = uint32_t(tile % surface_->columns) * 32,
               ty = uint32_t(tile / surface_->columns) * 32;
      for (uint32_t y = 0; y < 32; ++y)
        for (uint32_t x = 0; x < 32; ++x) {
          auto &t = surface_->texels[size_t(ty + y) * resolution() + tx + x];
          if (!t.covered || (settings.front_only && dot(t.normal, n) <= .05f))
            continue;
          float u =
              axis2 > 1e-20f
                  ? std::clamp(dot(t.position - b.from, axis) / axis2, 0.f, 1.f)
                  : 0;
          float distance = length(t.position - (b.from + axis * u));
          if (distance >= radius)
            continue;
          float edge = 1 - distance * distance / (radius * radius),
                alpha = std::clamp(edge * edge * b.pressure * b.strength *
                                       settings.flow,
                                   0.f, 1.f);
          auto &p = copy->pixels[y * 32 + x];
          auto old = p;
          if (settings.channel == PaintChannel::Mask)
            p.data[13] =
                byte(unit(p.data[13]) * (1 - alpha) + settings.mask * alpha);
          else {
            int channel = int(settings.channel), coverage = 9 + channel;
            float previous = unit(p.data[coverage]);
            if (settings.erase)
              p.data[coverage] = byte(previous * (1 - alpha));
            else {
              float next = alpha + previous * (1 - alpha);
              p.data[coverage] = byte(next);
              auto mix = [&](int i, float value, bool srgb) {
                float old_value = unit(p.data[i]);
                if (srgb)
                  old_value = srgb_to_linear(old_value);
                float result =
                    (value * alpha + old_value * previous * (1 - alpha)) /
                    std::max(next, 1e-12f);
                p.data[i] = byte(srgb ? linear_to_srgb(result) : result);
              };
              if (channel == 0)
                for (int k = 0; k < 4; ++k)
                  mix(k, settings.color[k], k < 3);
              else if (channel == 1)
                mix(4, settings.metallic, false);
              else if (channel == 2)
                mix(5, settings.roughness, false);
              else
                for (int k = 0; k < 3; ++k)
                  mix(6 + k, settings.emission[k], true);
            }
          }
          tile_changed |= p != old;
        }
      if (tile_changed) {
        if (!l.tiles[tile] &&
            ++allocated * sizeof(PaintTile) > 128 * 1024 * 1024)
          throw std::length_error("Paint storage budget exceeded");
        l.tiles[tile] = std::move(copy);
        changed = true;
      }
    }
  } catch (...) {
    l.tiles = std::move(prior);
    throw;
  }
  if (changed)
    ++revision_;
}
bool PaintedMesh::end_stroke() {
  if (!editing_)
    return false;
  editing_ = false;
  auto &tiles = active().tiles;
  if (before_ == tiles) {
    before_.clear();
    return false;
  }
  size_t bytes = 0;
  for (size_t i = 0; i < tiles.size(); ++i)
    if (before_[i] != tiles[i])
      bytes += sizeof(PaintTile) * 2;
  for (auto &s : redo_)
    history_bytes_ -= s.bytes;
  redo_.clear();
  undo_.push_back({active_, std::move(before_), tiles, bytes});
  history_bytes_ += bytes;
  while (undo_.size() > 1 &&
         (history_bytes_ > 64 * 1024 * 1024 || undo_.size() > 128)) {
    history_bytes_ -= undo_.front().bytes;
    undo_.pop_front();
  }
  return true;
}
void PaintedMesh::cancel_stroke() {
  if (editing_) {
    active().tiles = std::move(before_);
    editing_ = false;
    ++revision_;
  }
}
bool PaintedMesh::undo() {
  if (editing_)
    throw std::logic_error("Finish the paint stroke before undo");
  if (undo_.empty())
    return false;
  auto s = std::move(undo_.back());
  undo_.pop_back();
  active_ = s.layer;
  active().tiles = s.before;
  redo_.push_back(std::move(s));
  ++revision_;
  return true;
}
bool PaintedMesh::redo() {
  if (editing_)
    throw std::logic_error("Finish the paint stroke before redo");
  if (redo_.empty())
    return false;
  auto s = std::move(redo_.back());
  redo_.pop_back();
  active_ = s.layer;
  active().tiles = s.after;
  undo_.push_back(std::move(s));
  ++revision_;
  return true;
}
std::shared_ptr<PaintedMesh> PaintedMesh::snapshot() const {
  auto p = std::shared_ptr<PaintedMesh>(new PaintedMesh);
  p->surface_ = surface_;
  p->layers_ = layers_;
  p->active_ = active_;
  p->next_ = next_;
  p->revision_ = revision_;
  return p;
}
std::shared_ptr<const TextureMaps>
PaintedMesh::composite(const Material &material) const {
  material.validate();
  auto maps = std::make_shared<TextureMaps>();
  uint32_t r = resolution();
  for (auto image : {&maps->color, &maps->metal_rough, &maps->emission}) {
    image->width = image->height = r;
    image->rgba.resize(size_t(r) * r * 4);
  }
  for (uint32_t y = 0; y < r; ++y)
    for (uint32_t x = 0; x < r; ++x) {
      auto color = material.color;
      auto emission = material.emissive;
      float metal = material.metallic, rough = material.roughness;
      int32_t owner = surface_->padding[size_t(y) * r + x];
      uint32_t px = owner >= 0 ? uint32_t(owner) % r : x,
               py = owner >= 0 ? uint32_t(owner) / r : y;
      for (auto &l : layers_)
        if (l.visible && l.opacity > 0 && owner >= 0) {
          auto &p = pixel(l.id, px, py);
          float mask = unit(p.data[13]) * l.opacity;
          auto blend = [&](float base, int value, int coverage, bool srgb) {
            float v = unit(p.data[value]);
            if (srgb)
              v = srgb_to_linear(v);
            float a = mask * unit(p.data[coverage]);
            return base * (1 - a) + v * a;
          };
          for (int k = 0; k < 4; ++k)
            color[k] = blend(color[k], k, 9, k < 3);
          metal = blend(metal, 4, 10, false);
          rough = blend(rough, 5, 11, false);
          for (int k = 0; k < 3; ++k)
            emission[k] = blend(emission[k], 6 + k, 12, true);
        }
      size_t i = (size_t(y) * r + x) * 4;
      for (int k = 0; k < 3; ++k) {
        maps->color.rgba[i + k] = byte(linear_to_srgb(color[k]));
        maps->emission.rgba[i + k] = byte(linear_to_srgb(emission[k]));
      }
      maps->color.rgba[i + 3] = byte(color[3]);
      maps->transparent |= color[3] < .999f;
      maps->metal_rough.rgba[i] = 255;
      maps->metal_rough.rgba[i + 1] = byte(rough);
      maps->metal_rough.rgba[i + 2] = byte(metal);
      maps->metal_rough.rgba[i + 3] = 255;
      maps->emission.rgba[i + 3] = 255;
    }
  return maps;
}
std::optional<Vec3> PaintedMesh::raycast(Vec3 origin, Vec3 direction,
                                         float maximum) const {
  if (!finite(origin) || !finite(direction) || length(direction) < 1e-10f ||
      !std::isfinite(maximum) || maximum <= 0)
    throw std::invalid_argument("Invalid mesh ray");
  direction = normalized(direction);
  float nearest = maximum;
  bool found = false;
  auto &m = mesh();
  for (size_t k = 0; k < m.indices.size(); k += 3) {
    auto a = m.positions[m.indices[k]], e1 = m.positions[m.indices[k + 1]] - a,
         e2 = m.positions[m.indices[k + 2]] - a;
    auto p = cross(direction, e2);
    float det = dot(e1, p);
    if (std::abs(det) < 1e-8f * length(e1) * length(e2))
      continue;
    float f = 1 / det;
    auto s = origin - a;
    float u = f * dot(s, p);
    if (u < 0 || u > 1)
      continue;
    auto q = cross(s, e1);
    float v = f * dot(direction, q);
    if (v < 0 || u + v > 1)
      continue;
    float t = f * dot(e2, q);
    if (t >= 0 && t <= nearest) {
      found = true;
      nearest = t;
    }
  }
  return found ? std::optional<Vec3>(origin + direction * nearest)
               : std::nullopt;
}
Vec3 PaintedMesh::normal(Vec3 point) const {
  auto &m = mesh();
  float distance = INFINITY;
  Vec3 result{0, 1, 0};
  for (size_t i = 0; i < m.positions.size(); ++i) {
    auto d = dot(m.positions[i] - point, m.positions[i] - point);
    if (d < distance) {
      distance = d;
      result = m.normals[i];
    }
  }
  return result;
}
namespace {
void word(std::vector<uint8_t> &out, uint32_t v) {
  for (int k = 0; k < 4; ++k)
    out.push_back(uint8_t(v >> (8 * k)));
}
uint32_t word(const std::vector<uint8_t> &bytes, size_t offset) {
  uint32_t v = 0;
  for (int k = 0; k < 4; ++k)
    v |= uint32_t(bytes[offset + k]) << (8 * k);
  return v;
}
} // namespace
nlohmann::json PaintedMesh::encode() const {
  if (editing_)
    throw std::logic_error("Finish the paint stroke before saving");
  nlohmann::json j = {{"resolution", resolution()},
                      {"active", active_},
                      {"next_id", next_},
                      {"layers", nlohmann::json::array()}};
  std::vector<uint8_t> positions, normals, uv, indices;
  for (auto p : mesh().positions)
    for (float f : {p.x, p.y, p.z})
      word(positions, std::bit_cast<uint32_t>(f));
  for (auto p : mesh().normals)
    for (float f : {p.x, p.y, p.z})
      word(normals, std::bit_cast<uint32_t>(f));
  for (auto p : mesh().uv)
    for (float f : p)
      word(uv, std::bit_cast<uint32_t>(f));
  for (auto i : mesh().indices)
    word(indices, i);
  j["mesh"] = {{"positions", nlohmann::json::binary(positions)},
               {"normals", nlohmann::json::binary(normals)},
               {"uv", nlohmann::json::binary(uv)},
               {"indices", nlohmann::json::binary(indices)}};
  for (auto &l : layers_) {
    nlohmann::json tiles = nlohmann::json::array();
    for (size_t i = 0; i < l.tiles.size(); ++i)
      if (l.tiles[i]) {
        std::vector<uint8_t> bytes;
        bytes.reserve(32 * 32 * 14);
        for (auto &p : l.tiles[i]->pixels)
          bytes.insert(bytes.end(), p.data.begin(), p.data.end());
        tiles.push_back(
            {{"index", i}, {"data", nlohmann::json::binary(bytes)}});
      }
    j["layers"].push_back({{"id", l.id},
                           {"name", l.name},
                           {"visible", l.visible},
                           {"opacity", l.opacity},
                           {"tiles", std::move(tiles)}});
  }
  return j;
}
std::shared_ptr<PaintedMesh> PaintedMesh::decode(const nlohmann::json &j) {
  auto &m = j.at("mesh");
  auto blob = [&](const char *name, size_t stride,
                  size_t limit) -> const std::vector<uint8_t> & {
    auto &v = m.at(name);
    if (!v.is_binary() || v.get_binary().empty() ||
        v.get_binary().size() % stride ||
        v.get_binary().size() / stride > limit)
      throw std::runtime_error("Invalid persisted mesh buffer");
    return v.get_binary();
  };
  Mesh mesh;
  auto &p = blob("positions", 12, 2'000'000),
       &n = blob("normals", 12, 2'000'000), &uv = blob("uv", 8, 2'000'000),
       &indices = blob("indices", 12, 2'000'000);
  for (size_t i = 0; i < p.size(); i += 12)
    mesh.positions.push_back({std::bit_cast<float>(word(p, i)),
                              std::bit_cast<float>(word(p, i + 4)),
                              std::bit_cast<float>(word(p, i + 8))});
  for (size_t i = 0; i < n.size(); i += 12)
    mesh.normals.push_back({std::bit_cast<float>(word(n, i)),
                            std::bit_cast<float>(word(n, i + 4)),
                            std::bit_cast<float>(word(n, i + 8))});
  for (size_t i = 0; i < uv.size(); i += 8)
    mesh.uv.push_back({std::bit_cast<float>(word(uv, i)),
                       std::bit_cast<float>(word(uv, i + 4))});
  for (size_t i = 0; i < indices.size(); i += 4)
    mesh.indices.push_back(word(indices, i));
  auto &resolution = j.at("resolution");
  if ((!resolution.is_number_unsigned() &&
       (!resolution.is_number_integer() || resolution.get<int64_t>() < 64)) ||
      resolution.get<uint64_t>() > 2048)
    throw std::runtime_error("Invalid paint resolution");
  auto result = std::make_shared<PaintedMesh>(
      std::move(mesh), uint32_t(resolution.get<uint64_t>()));
  auto positive = [](const nlohmann::json &v) {
    if ((!v.is_number_unsigned() &&
         (!v.is_number_integer() || v.get<int64_t>() <= 0)) ||
        v.get<uint64_t>() == 0)
      throw std::runtime_error("Invalid paint identity");
    return v.get<uint64_t>();
  };
  result->active_ = positive(j.at("active"));
  result->next_ = positive(j.at("next_id"));
  auto &layers = j.at("layers");
  if (!layers.is_array() || layers.empty() || layers.size() > 16)
    throw std::length_error("Invalid paint layer count");
  result->layers_.clear();
  std::set<uint64_t> ids;
  size_t allocated = 0;
  for (auto &v : layers) {
    PaintLayer l;
    l.id = positive(v.at("id"));
    if (l.id >= result->next_ || !ids.insert(l.id).second)
      throw std::runtime_error("Invalid paint layer identity");
    l.name = v.at("name").get<std::string>();
    name_valid(l.name);
    l.visible = v.at("visible").get<bool>();
    l.opacity = v.at("opacity").get<float>();
    if (!std::isfinite(l.opacity) || l.opacity < 0 || l.opacity > 1)
      throw std::runtime_error("Invalid layer opacity");
    l.tiles.resize(result->surface_->tiles.size());
    auto &tiles = v.at("tiles");
    if (!tiles.is_array() || tiles.size() > l.tiles.size())
      throw std::length_error("Invalid paint tile count");
    for (auto &t : tiles) {
      auto &index = t.at("index"), &data = t.at("data");
      if (!index.is_number_unsigned() &&
          (!index.is_number_integer() || index.get<int64_t>() < 0))
        throw std::runtime_error("Invalid paint tile index");
      auto i = index.get<uint64_t>();
      if (i >= l.tiles.size() || l.tiles[i] || !data.is_binary() ||
          data.get_binary().size() != 32 * 32 * 14)
        throw std::runtime_error("Invalid paint tile data");
      allocated += sizeof(PaintTile);
      if (allocated > 128 * 1024 * 1024)
        throw std::length_error("Paint storage budget exceeded");
      auto tile = std::make_shared<PaintTile>();
      auto &bytes = data.get_binary();
      size_t offset = 0;
      for (auto &p : tile->pixels)
        for (auto &b : p.data)
          b = bytes[offset++];
      l.tiles[i] = std::move(tile);
    }
    result->layers_.push_back(std::move(l));
  }
  result->layer(result->active_);
  return result;
}
} // namespace dome
