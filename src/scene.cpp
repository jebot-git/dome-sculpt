// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include "io.hpp"
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>
namespace dome {
namespace {
void name_valid(const std::string &s) {
  if (s.empty() || s.size() > 256 || s.find('\0') != std::string::npos)
    throw std::invalid_argument(
        "Names must contain 1 to 256 bytes without NUL");
}
} // namespace
void Material::validate() const {
  name_valid(name);
  auto unit = [](float f) { return std::isfinite(f) && f >= 0 && f <= 1; };
  if (!std::all_of(color.begin(), color.end(), unit) || !unit(metallic) ||
      !unit(roughness) || !std::all_of(emissive.begin(), emissive.end(), unit))
    throw std::invalid_argument("Invalid material channel value");
}
void validate_transform(CanvasTransform t) {
  auto q = t.orientation;
  float norm = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
  if (!ob::finite(t) || std::abs(norm - 1) > .0001f || t.scale < .001f ||
      t.scale > 100 ||
      std::max({std::abs(t.position.x), std::abs(t.position.y),
                std::abs(t.position.z)}) > 100)
    throw std::invalid_argument("Invalid object transform");
}
Scene::Scene() {
  SculptObject object;
  object.id = 1;
  objects_.push_back(std::move(object));
}
SculptObject &Scene::object(uint64_t id) {
  auto it = std::find_if(objects_.begin(), objects_.end(),
                         [&](auto &o) { return o.id == id; });
  if (it == objects_.end())
    throw std::out_of_range("Unknown sculpt object");
  return *it;
}
const SculptObject &Scene::object(uint64_t id) const {
  return const_cast<Scene *>(this)->object(id);
}
void Scene::select(uint64_t id) {
  object(id);
  active_ = id;
}
uint64_t Scene::add(std::string name, Volume v, CanvasTransform t) {
  name_valid(name);
  validate_transform(t);
  if (objects_.size() >= 256 || next_ == UINT64_MAX)
    throw std::length_error("Scene object limit reached");
  SculptObject o;
  o.id = next_++;
  o.name = std::move(name);
  o.transform = t;
  o.volume = std::make_shared<Volume>(std::move(v));
  objects_.push_back(std::move(o));
  active_ = objects_.back().id;
  return active_;
}
uint64_t Scene::duplicate(uint64_t id) {
  auto source = object(id);
  auto name = source.name.substr(0, 240) + " copy";
  auto added = add(name, source.volume->geometry_snapshot(), source.transform);
  auto &o = object(added);
  o.material = source.material;
  o.visible = source.visible;
  o.mesh_active = source.mesh_active;
  if (source.painted)
    o.painted = source.painted->snapshot();
  return added;
}
void Scene::erase(uint64_t id) {
  object(id);
  if (objects_.size() == 1)
    throw std::logic_error("Keep at least one sculpt object");
  std::erase_if(objects_, [&](auto &o) { return o.id == id; });
  if (active_ == id)
    active_ = objects_.front().id;
}
Scene Scene::snapshot() const {
  Scene result = *this;
  for (auto &o : result.objects_) {
    o.volume = std::make_shared<Volume>(o.volume->geometry_snapshot());
    if (o.painted)
      o.painted = o.painted->snapshot();
  }
  return result;
}
void Scene::validate() const {
  if (objects_.empty() || objects_.size() > 256)
    throw std::length_error("Invalid scene object count");
  std::set<uint64_t> ids;
  size_t count = 0, texels = 0;
  for (auto &o : objects_) {
    if (!o.id || o.id >= next_ || !ids.insert(o.id).second || !o.volume ||
        o.volume->in_stroke())
      throw std::invalid_argument(
          "Invalid scene object identity or unfinished stroke");
    name_valid(o.name);
    validate_transform(o.transform);
    o.material.validate();
    if ((o.mesh_active && !o.painted) || (o.painted && o.painted->in_stroke()))
      throw std::runtime_error(
          "Invalid active mesh or unfinished paint stroke");
    if (o.painted)
      texels += size_t(o.painted->resolution()) * o.painted->resolution();
    if (texels > 16 * 1024 * 1024)
      throw std::length_error("Scene paint-map budget exceeded");
    count += o.volume->voxels().size();
    if (count > 4'000'000)
      throw std::length_error("Scene sample limit exceeded");
  }
  object(active_);
}
nlohmann::json Scene::encode() const {
  validate();
  nlohmann::json j = {{"format", "dome-sculpt"},
                      {"version", 3},
                      {"active", active_},
                      {"next_id", next_},
                      {"objects", nlohmann::json::array()}};
  for (auto &o : objects_) {
    auto t = o.transform;
    auto &m = o.material;
    j["objects"].push_back(
        {{"id", o.id},
         {"name", o.name},
         {"visible", o.visible},
         {"locked", o.locked},
         {"translation", {t.position.x, t.position.y, t.position.z}},
         {"rotation",
          {t.orientation.x, t.orientation.y, t.orientation.z, t.orientation.w}},
         {"scale", t.scale},
         {"material",
          {{"name", m.name},
           {"color", m.color},
           {"metallic", m.metallic},
           {"roughness", m.roughness},
           {"emissive", m.emissive},
           {"toon", m.toon},
           {"double_sided", m.double_sided}}},
         {"volume", o.volume->encode()},
         {"mesh_active", o.mesh_active},
         {"painted",
          o.painted ? o.painted->encode() : nlohmann::json(nullptr)}});
  }
  return j;
}
Scene Scene::decode(const nlohmann::json &j) {
  Scene result;
  if (j.at("format") != "dome-sculpt")
    throw std::runtime_error("Unsupported scene format");
  if (j.at("version") == 1) {
    result.active().volume = std::make_shared<Volume>(Volume::decode(j));
    return result;
  }
  if (j.at("version") != 2 && j.at("version") != 3)
    throw std::runtime_error("Unsupported scene version");
  auto id = [](const nlohmann::json &v) {
    if (!v.is_number_unsigned() &&
        (!v.is_number_integer() || v.get<int64_t>() <= 0))
      throw std::runtime_error("Invalid scene ID");
    auto n = v.get<uint64_t>();
    if (!n)
      throw std::runtime_error("Invalid scene ID");
    return n;
  };
  result.active_ = id(j.at("active"));
  result.next_ = id(j.at("next_id"));
  const auto &objects = j.at("objects");
  if (!objects.is_array() || objects.empty() || objects.size() > 256)
    throw std::length_error("Invalid scene object count");
  result.objects_.clear();
  size_t samples = 0, texels = 0;
  for (auto &v : objects) {
    SculptObject o;
    o.id = id(v.at("id"));
    o.name = v.at("name").get<std::string>();
    o.visible = v.at("visible").get<bool>();
    o.locked = v.at("locked").get<bool>();
    auto p = v.at("translation").get<std::array<float, 3>>();
    auto q = v.at("rotation").get<std::array<float, 4>>();
    o.transform = {{p[0], p[1], p[2]},
                   {q[0], q[1], q[2], q[3]},
                   v.at("scale").get<float>()};
    auto &m = v.at("material");
    o.material = {m.at("name").get<std::string>(),
                  m.at("color").get<std::array<float, 4>>(),
                  m.at("metallic").get<float>(),
                  m.at("roughness").get<float>(),
                  m.at("emissive").get<std::array<float, 3>>(),
                  m.at("toon").get<bool>(),
                  m.at("double_sided").get<bool>()};
    samples += v.at("volume").at("samples").size();
    if (samples > 4'000'000)
      throw std::length_error("Scene sample limit exceeded");
    o.volume = std::make_shared<Volume>(Volume::decode(v.at("volume")));
    if (j.at("version") == 3) {
      o.mesh_active = v.at("mesh_active").get<bool>();
      if (!v.at("painted").is_null()) {
        auto &size = v.at("painted").at("resolution");
        if ((!size.is_number_unsigned() &&
             (!size.is_number_integer() || size.get<int64_t>() < 64)) ||
            size.get<uint64_t>() > 2048)
          throw std::runtime_error("Invalid paint resolution");
        auto r = size.get<uint64_t>();
        texels += r * r;
        if (texels > 16 * 1024 * 1024)
          throw std::length_error("Scene paint-map budget exceeded");
        o.painted = PaintedMesh::decode(v.at("painted"));
      }
    }
    result.objects_.push_back(std::move(o));
  }
  result.validate();
  return result;
}
void Scene::save(const std::filesystem::path &path) const {
  auto bytes = nlohmann::json::to_cbor(encode());
  if (bytes.size() > 256 * 1024 * 1024)
    throw std::length_error("Scene exceeds file size limit");
  atomic_write(path, bytes);
}
Scene Scene::load(const std::filesystem::path &path) {
  if (std::filesystem::file_size(path) > 256 * 1024 * 1024)
    throw std::length_error("Scene exceeds file size limit");
  std::ifstream input(path, std::ios::binary);
  if (!input)
    throw std::runtime_error("Cannot open scene");
  std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(input)), {});
  return decode(nlohmann::json::from_cbor(bytes));
}
std::vector<ObjectSurface>
Scene::surfaces(bool atlas, const std::function<bool()> &cancelled) const {
  std::vector<ObjectSurface> result;
  for (auto &o : objects_)
    if (o.visible) {
      auto mesh = o.mesh_active ? o.painted->mesh()
                                : surface_mesh(*o.volume, cancelled);
      if (mesh.indices.empty())
        continue;
      if (atlas && !o.mesh_active)
        unwrap(mesh);
      result.push_back(
          {o.id, o.name, o.transform, o.material, std::move(mesh),
           o.mesh_active ? o.painted->composite(o.material) : nullptr});
    }
  return result;
}
} // namespace dome
