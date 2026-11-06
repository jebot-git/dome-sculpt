// SPDX-License-Identifier: GPL-3.0-or-later
#include "texture-set-archive.hpp"
#include "dome/bake-recipe.hpp"
#include "dome/polygon-scene.hpp"
#include "dome/source-paint.hpp"
#include "texture-set-paint-access.hpp"
#include <algorithm>
#include <bit>
#include <map>
namespace dome::detail {
namespace {
using Json = nlohmann::json;
uint64_t integer(const Json &v, uint64_t maximum) {
  if ((!v.is_number_unsigned() &&
       (!v.is_number_integer() || v.get<int64_t>() < 0)) ||
      v.get<uint64_t>() > maximum)
    throw std::invalid_argument("Invalid texture-set archive identity or size");
  return v.get<uint64_t>();
}
void word(std::vector<uint8_t> &v, uint64_t x, size_t bytes) {
  for (size_t k = 0; k < bytes; ++k)
    v.push_back(uint8_t(x >> (8 * k)));
}
uint64_t word(const std::vector<uint8_t> &v, size_t at, size_t bytes) {
  uint64_t x = 0;
  for (size_t k = 0; k < bytes; ++k)
    x |= uint64_t(v.at(at + k)) << (8 * k);
  return x;
}
const std::vector<uint8_t> &blob(const Json &j, size_t stride, size_t maximum,
                                 bool empty = false) {
  if (!j.is_binary() || (!empty && j.get_binary().empty()) ||
      j.get_binary().size() % stride ||
      j.get_binary().size() / stride > maximum)
    throw std::invalid_argument(
        "Invalid texture-set geometry or identity buffer");
  return j.get_binary();
}
Json material(const Material &m) {
  return {{"name", m.name},
          {"color", m.color},
          {"metallic", m.metallic},
          {"roughness", m.roughness},
          {"emissive", m.emissive},
          {"toon", m.toon},
          {"double_sided", m.double_sided},
          {"alpha", int(m.alpha)},
          {"alpha_cutoff", m.alpha_cutoff},
          {"unlit", m.unlit}};
}
Material material(const Json &j) {
  if (!j.is_object() || j.size() != 10)
    throw std::invalid_argument("Incomplete texture-set material");
  Material m{j.at("name").get<std::string>(),
             j.at("color").get<std::array<float, 4>>(),
             j.at("metallic").get<float>(),
             j.at("roughness").get<float>(),
             j.at("emissive").get<std::array<float, 3>>(),
             j.at("toon").get<bool>(),
             j.at("double_sided").get<bool>()};
  m.alpha = Material::AlphaMode(integer(j.at("alpha"), 3));
  m.alpha_cutoff = j.at("alpha_cutoff").get<float>();
  m.unlit = j.at("unlit").get<bool>();
  m.validate();
  return m;
}
template <class T>
bool same(const std::shared_ptr<const T> &a,
          const std::shared_ptr<const T> &b) {
  return a && a == b && !a.owner_before(b) && !b.owner_before(a);
}
void primary_context(
    const std::shared_ptr<const SourceMaterialBindings> &binding,
    const std::shared_ptr<const CreatorAppearance> &appearance,
    const std::shared_ptr<const ObjectBakeRecipe> &recipe,
    const TextureSetArchiveField &field) {
  const auto matching = [](const auto &a, const auto &b) {
    return (!a && !b) || same(a, b);
  };
  if (!matching(binding, field.binding) ||
      !matching(appearance, field.appearance))
    throw std::invalid_argument("Primary field disagrees with its explicitly "
                                "declared source/material owner");
  if (bool(recipe) != bool(field.recipe) ||
      (recipe && !same(recipe, field.recipe) &&
       recipe->encode() != field.recipe->encode()))
    throw std::invalid_argument(
        "Primary field disagrees with its explicitly declared bake recipe");
}
// Initial connected codec slice. Unsupported history/normal/source owners are
// explicit development guards, never omitted from the wire. Their actual
// archive/context adapters remain required before general texture-set
// admission.
void plain(const PaintedMesh &p, const SourceMaterialBindings *b,
           const CreatorAppearance *a, const ObjectBakeRecipe *r) {
  if (b || a || r || p.paint_sampling_domain() ||
      p.source_sampling_prepared() || p.source_maps() || p.color_projection() ||
      p.normal_projection() || (p.baked() && p.baked()->normal_field))
    throw std::invalid_argument("Texture-set source/normal/recipe graph needs "
                                "its owning Scene40 adapter");
}
} // namespace
TextureSetArchive::TextureSetArchive(TextureSetLimits l)
    : limits_(std::move(l)) {
  limits_.validate();
}
void TextureSetArchive::charge(size_t count, size_t unit) {
  if (unit && count > (limits_.maximum_bytes - bytes_) / unit)
    throw std::length_error(
        "Texture-set archive exceeds its owning byte budget");
  bytes_ += count * unit;
}
void TextureSetArchive::step(size_t n) {
  if (n > limits_.maximum_work - work_)
    throw std::length_error("Texture-set archive exceeds its work budget");
  work_ += n;
  if (limits_.cancelled && limits_.cancelled())
    throw std::runtime_error("Texture-set archive cancelled");
}
uint32_t
TextureSetArchive::add(const std::shared_ptr<const TextureSetTopology> &t) {
  if (!t || !t->geometry())
    throw std::invalid_argument("Texture-set archive lost its geometry owner");
  for (size_t i = 0; i < topology_.size(); ++i) {
    step();
    if (same(topology_[i], t))
      return uint32_t(i);
  }
  if (topology_.size() >= limits_.maximum_sets_per_scene)
    throw std::length_error("Too many retained texture-set topologies");
  uint32_t geometry = 0;
  for (; geometry < geometry_.size(); ++geometry) {
    step();
    if (same(geometry_[geometry], t->geometry()))
      break;
  }
  if (geometry == geometry_.size()) {
    const auto &m = *t->geometry();
    step(m.positions.size() + m.indices.size());
    charge(m.positions.size() + m.normals.size(), sizeof(Vec3));
    charge(m.uv.size(), sizeof(std::array<float, 2>));
    charge(m.indices.size(), sizeof(uint32_t));
    charge(m.uv_pins.size() + m.uv_seams.size(), 1);
    geometry_.push_back(t->geometry());
  }
  charge(t->faces_.size() + t->corners_.size(), sizeof(uint64_t));
  topology_.push_back(t);
  topology_geometry_.push_back(geometry);
  return uint32_t(topology_.size() - 1);
}
Json TextureSetArchive::encode(const SculptObject &o, PaintImageArchive &images,
                               NormalSamplingArchive *normals,
                               PaintSamplingArchive *sampling) {
  if (!o.texture_sets)
    throw std::invalid_argument(
        "Texture-set archive requires actual collection membership");
  const auto &c = *o.texture_sets;
  c.validate(o, true, limits_);
  if (shared_)
    return encode_shared(o.texture_sets, texture_set_view(o, 0), images,
                         normals, sampling);
  ++legacy_holders_;
  if (c.primary_source())
    throw std::invalid_argument(
        "Source-backed membership requires the shared Scene40 field archive");
  for (const auto &f : c.additional())
    if (f.source)
      throw std::invalid_argument(
          "Source-backed membership requires the shared Scene40 field archive");
  if (c.additional_.size() + 1 > limits_.maximum_sets_per_scene - sets_)
    throw std::length_error(
        "Texture-set archive exceeds aggregate Scene set count");
  sets_ += c.additional_.size() + 1;
  plain(*o.painted, o.material_bindings.get(), o.appearance.get(),
        o.bake_recipe.get());
  for (const auto &r : c.additional_)
    plain(*r.painted, r.material_bindings.get(), r.appearance.get(),
          r.bake_recipe.get());
  const auto id = add(c.topology_);
  std::vector<uint8_t> slots;
  slots.reserve(c.slots_->size() * 4);
  for (auto x : *c.slots_) {
    step();
    word(slots, x, 4);
  }
  Json j = {{"version", 1},
            {"topology", id},
            {"active", c.active_},
            {"next_id", c.next_},
            {"slots", Json::binary(std::move(slots))},
            {"primary", TextureSetPaintAccess::encode(*o.painted, &images,
                                                      normals, sampling)},
            {"additional", Json::array()}};
  for (const auto &r : c.additional_) {
    step();
    j["additional"].push_back(
        {{"id", r.id},
         {"name", r.name},
         {"material", material(r.material)},
         {"painted", TextureSetPaintAccess::encode(*r.painted, &images, normals,
                                                   sampling)}});
  }
  return j;
}
Json TextureSetArchive::geometry_records() const {
  Json out = {
      {"version", 1}, {"meshes", Json::array()}, {"topologies", Json::array()}};
  for (const auto &owner : geometry_) {
    const auto &m = *owner;
    std::vector<uint8_t> p, n, uv, indices;
    p.reserve(m.positions.size() * 12);
    n.reserve(m.normals.size() * 12);
    uv.reserve(m.uv.size() * 8);
    indices.reserve(m.indices.size() * 4);
    for (auto v : m.positions)
      for (float x : {v.x, v.y, v.z})
        word(p, std::bit_cast<uint32_t>(x), 4);
    for (auto v : m.normals)
      for (float x : {v.x, v.y, v.z})
        word(n, std::bit_cast<uint32_t>(x), 4);
    for (auto v : m.uv)
      for (float x : v)
        word(uv, std::bit_cast<uint32_t>(x), 4);
    for (auto x : m.indices)
      word(indices, x, 4);
    out["meshes"].push_back({{"positions", Json::binary(std::move(p))},
                             {"normals", Json::binary(std::move(n))},
                             {"uv", Json::binary(std::move(uv))},
                             {"indices", Json::binary(std::move(indices))},
                             {"uv_pins", Json::binary(m.uv_pins)},
                             {"uv_seams", Json::binary(m.uv_seams)}});
  }
  for (size_t at = 0; at < topology_.size(); ++at) {
    const auto &t = topology_[at];
    std::vector<uint8_t> faces, corners;
    faces.reserve(t->faces_.size() * 8);
    corners.reserve(t->corners_.size() * 8);
    for (auto x : t->faces_)
      word(faces, x, 8);
    for (auto x : t->corners_)
      word(corners, x, 8);
    out["topologies"].push_back({{"mesh", topology_geometry_.at(at)},
                                 {"faces", Json::binary(std::move(faces))},
                                 {"corners", Json::binary(std::move(corners))},
                                 {"polygon_ids", t->polygon_ids_}});
  }
  if (shared_) {
    out["version"] = 2;
    out["assignments"] = Json::array();
    out["domains"] = Json::array();
    out["rasters"] = Json::array();
    out["paints"] = Json::array();
    out["collections"] = Json::array();
    out["field_sources"] = field_records_ ? field_records_() : field_sources_;
    for (const auto &slots : assignments_) {
      std::vector<uint8_t> bytes;
      bytes.reserve(slots->size() * 4);
      for (auto x : *slots)
        word(bytes, x, 4);
      out["assignments"].push_back(Json::binary(std::move(bytes)));
    }
    for (const auto &d : domains_)
      out["domains"].push_back({{"topology", d.topology},
                                {"assignments", d.assignment},
                                {"set", d.owner->set()}});
    for (const auto &r : rasters_)
      out["rasters"].push_back(
          {{"domain", r.domain}, {"resolution", r.resolution}});
    for (const auto &p : paints_)
      out["paints"].push_back({{"raster", p.raster}, {"painted", p.payload}});
    for (const auto &c : collections_)
      out["collections"].push_back(c.wire);
  }
  return out;
}
TextureSetArchive TextureSetArchive::decode_geometry(const Json &j,
                                                     TextureSetLimits limits) {
  TextureSetArchive out(std::move(limits));
  const bool shared =
      j.is_object() && j.contains("version") && j.at("version") == 2;
  if (!j.is_object() || j.size() != (shared ? 9u : 3u) ||
      integer(j.at("version"), 2) != (shared ? 2u : 1u) ||
      !j.at("meshes").is_array() || j.at("meshes").empty() ||
      j.at("meshes").size() > out.limits_.maximum_sets_per_scene ||
      !j.at("topologies").is_array() || j.at("topologies").empty() ||
      j.at("topologies").size() > out.limits_.maximum_sets_per_scene)
    throw std::invalid_argument(
        "Scene40 needs its actual bounded texture-set geometry table");
  // First measure every encoded buffer before any Mesh/vector reconstruction.
  for (const auto &v : j.at("meshes")) {
    if (!v.is_object() || v.size() != 6)
      throw std::invalid_argument("Invalid texture-set mesh record");
    const auto &p = blob(v.at("positions"), 12, 2'000'000),
               &n = blob(v.at("normals"), 12, 2'000'000),
               &uv = blob(v.at("uv"), 8, 2'000'000),
               &i = blob(v.at("indices"), 12, out.limits_.maximum_triangles);
    if (p.size() != n.size() || p.size() / 12 != uv.size() / 8)
      throw std::invalid_argument("Texture-set vertex arrays disagree");
    const auto &pins = blob(v.at("uv_pins"), 1, p.size() / 12, true),
               &seams = blob(v.at("uv_seams"), 1, i.size() / 4, true);
    if ((!pins.empty() && pins.size() != p.size() / 12) ||
        (!seams.empty() && seams.size() != i.size() / 4))
      throw std::invalid_argument("Invalid texture-set UV edit flags");
    out.charge(p.size() + n.size() + uv.size() + i.size() + pins.size() +
                   seams.size(),
               1);
    out.step(p.size() / 12 + i.size() / 4);
  }
  std::set<uint32_t> used_meshes;
  for (const auto &v : j.at("topologies")) {
    if (!v.is_object() || v.size() != 4 || !v.at("polygon_ids").is_boolean())
      throw std::invalid_argument("Invalid texture-set topology record");
    const auto geometry =
        uint32_t(integer(v.at("mesh"), j.at("meshes").size() - 1));
    used_meshes.insert(geometry);
    const auto &i = j.at("meshes")[geometry].at("indices").get_binary();
    const auto &f = blob(v.at("faces"), 8, out.limits_.maximum_triangles),
               &c = blob(v.at("corners"), 8, out.limits_.maximum_corners);
    if (f.size() / 8 != i.size() / 12 || c.size() / 8 != i.size() / 4)
      throw std::invalid_argument(
          "Texture-set topology identities differ from actual corners");
    out.charge(f.size() + c.size(), 1);
    out.step(f.size() / 8 + c.size() / 8);
  }
  if (used_meshes.size() != j.at("meshes").size())
    throw std::invalid_argument("Unreachable texture-set mesh records");
  for (const auto &v : j.at("meshes")) {
    auto m = std::make_shared<Mesh>();
    auto vectors = [&](const char *key, std::vector<Vec3> &target) {
      const auto &b = v.at(key).get_binary();
      target.reserve(b.size() / 12);
      for (size_t at = 0; at < b.size(); at += 12)
        target.push_back({std::bit_cast<float>(uint32_t(word(b, at, 4))),
                          std::bit_cast<float>(uint32_t(word(b, at + 4, 4))),
                          std::bit_cast<float>(uint32_t(word(b, at + 8, 4)))});
    };
    vectors("positions", m->positions);
    vectors("normals", m->normals);
    const auto &uv = v.at("uv").get_binary();
    m->uv.reserve(uv.size() / 8);
    for (size_t at = 0; at < uv.size(); at += 8)
      m->uv.push_back({std::bit_cast<float>(uint32_t(word(uv, at, 4))),
                       std::bit_cast<float>(uint32_t(word(uv, at + 4, 4)))});
    const auto &i = v.at("indices").get_binary();
    m->indices.reserve(i.size() / 4);
    for (size_t at = 0; at < i.size(); at += 4)
      m->indices.push_back(uint32_t(word(i, at, 4)));
    m->uv_pins = v.at("uv_pins").get_binary();
    m->uv_seams = v.at("uv_seams").get_binary();
    m->validate();
    out.geometry_.push_back(std::move(m));
  }
  for (const auto &v : j.at("topologies")) {
    auto t = std::shared_ptr<TextureSetTopology>(new TextureSetTopology);
    const auto geometry =
        uint32_t(integer(v.at("mesh"), out.geometry_.size() - 1));
    t->geometry_ = out.geometry_[geometry];
    const auto &f = v.at("faces").get_binary(),
               &c = v.at("corners").get_binary();
    t->faces_.reserve(f.size() / 8);
    t->corners_.reserve(c.size() / 8);
    for (size_t at = 0; at < f.size(); at += 8)
      t->faces_.push_back(word(f, at, 8));
    for (size_t at = 0; at < c.size(); at += 8)
      t->corners_.push_back(word(c, at, 8));
    t->polygon_ids_ = v.at("polygon_ids").get<bool>();
    out.topology_.push_back(std::move(t));
    out.topology_geometry_.push_back(geometry);
  }
  if (shared)
    out.decode_shared_tables(j);
  return out;
}
void TextureSetArchive::decode(SculptObject &o, const Json &j,
                               const PaintImageArchive &images,
                               const NormalSamplingArchive *normals,
                               const PaintSamplingArchive *sampling,
                               const SourcePaintDecodeContext &context) {
  if (shared_) {
    auto result = decode_holder(j, o.material, bool(o.polygon), images, normals,
                                sampling, context);
    primary_context(o.material_bindings, o.appearance, o.bake_recipe,
                    result.primary);
    auto candidate = o;
    candidate.painted = result.primary.paint;
    candidate.texture_sets = result.collection;
    candidate.material_bindings = result.primary.binding;
    candidate.appearance = result.primary.appearance;
    candidate.bake_recipe = result.primary.recipe;
    result.collection->validate(candidate, true, limits_);
    o = std::move(candidate);
    return;
  }
  if (!j.is_object() || j.size() != 7 || integer(j.at("version"), 1) != 1 ||
      !j.at("additional").is_array() || j.at("additional").empty() ||
      j.at("additional").size() + 1 > limits_.maximum_sets_per_object)
    throw std::invalid_argument("Invalid owning texture-set collection record");
  if (j.at("additional").size() + 1 > limits_.maximum_sets_per_scene - sets_)
    throw std::length_error("Scene texture-set count exceeds bounds");
  sets_ += j.at("additional").size() + 1;
  const auto id = uint32_t(integer(j.at("topology"), topology_.size() - 1));
  const auto &t = topology_.at(id);
  const auto &encoded = blob(j.at("slots"), 4, limits_.maximum_triangles);
  if (encoded.size() / 4 != t->faces_.size())
    throw std::invalid_argument("Texture-set assignment omitted logical faces");
  auto c = std::shared_ptr<TextureSetCollection>(new TextureSetCollection);
  c->topology_ = t;
  c->active_ = TextureSetId(integer(j.at("active"), UINT32_MAX));
  c->next_ = integer(j.at("next_id"), uint64_t(UINT32_MAX) + 1);
  std::set<TextureSetId> ids{0};
  for (const auto &v : j.at("additional")) {
    if (!v.is_object() || v.size() != 4)
      throw std::invalid_argument("Incomplete texture-set member");
    auto set = TextureSetId(integer(v.at("id"), UINT32_MAX));
    if (!set || set >= c->next_ || !ids.insert(set).second)
      throw std::invalid_argument("Reused texture-set identity");
  }
  if (!ids.contains(c->active_))
    throw std::invalid_argument("Unknown active texture set");
  auto slots = std::make_shared<std::vector<TextureSetId>>();
  slots->reserve(encoded.size() / 4);
  std::map<uint64_t, TextureSetId> face_slots;
  for (size_t at = 0; at < encoded.size(); at += 4) {
    step();
    auto set = TextureSetId(word(encoded, at, 4));
    if (!ids.contains(set) || !t->faces_[at / 4])
      throw std::invalid_argument("Foreign texture-set face/slot");
    auto [it, fresh] = face_slots.emplace(t->faces_[at / 4], set);
    if (!fresh && it->second != set)
      throw std::invalid_argument("Polygon face has conflicting texture sets");
    slots->push_back(set);
  }
  for (auto corner : t->corners_)
    if (!corner)
      throw std::invalid_argument("Texture-set corner identity is missing");
  c->slots_ = std::move(slots);
  auto domain = [&](TextureSetId set) {
    auto d = std::shared_ptr<TextureSetFaceDomain>(new TextureSetFaceDomain);
    d->topology_ = t;
    d->set_ = set;
    d->assignments_ = c->slots_;
    for (size_t f = 0; f < c->slots_->size(); ++f) {
      step();
      if ((*c->slots_)[f] == set)
        d->triangles_.push_back(uint32_t(f));
    }
    std::shared_ptr<const TextureSetFaceDomain> sealed = std::move(d);
    sealed->self_ = sealed;
    return sealed;
  };
  c->primary_ = domain(0);
  auto primary =
      TextureSetPaintAccess::decode(j.at("primary"), &images, bool(o.polygon),
                                    normals, sampling, context, c->primary_);
  plain(*primary, o.material_bindings.get(), o.appearance.get(),
        o.bake_recipe.get());
  for (const auto &v : j.at("additional")) {
    TextureSetRecord r;
    r.id = TextureSetId(integer(v.at("id"), UINT32_MAX));
    r.name = v.at("name").get<std::string>();
    r.material = material(v.at("material"));
    r.domain = domain(r.id);
    r.painted =
        TextureSetPaintAccess::decode(v.at("painted"), &images, bool(o.polygon),
                                      normals, sampling, context, r.domain);
    plain(*r.painted, nullptr, nullptr, nullptr);
    c->additional_.push_back(std::move(r));
  }
  // No mutable draft or partially decoded collection escapes a failure.
  auto candidate = o;
  candidate.painted = std::move(primary);
  candidate.texture_sets = c;
  c->validate(candidate, true, limits_);
  reached_.insert(id);
  o = std::move(candidate);
}
void TextureSetArchive::require_reachable() const {
  if (reached_.size() != topology_.size())
    throw std::invalid_argument("Unreachable texture-set geometry records");
  if (shared_ && (used_assignments_.size() != assignments_.size() ||
                  used_domains_.size() != domains_.size() ||
                  used_rasters_.size() != rasters_.size() ||
                  used_paints_.size() != paints_.size() ||
                  used_collections_.size() != collections_.size()))
    throw std::invalid_argument("Unreachable shared texture-set owner records");
  if (shared_) {
    if (field_complete_)
      field_complete_();
    else if (!field_sources_.empty())
      throw std::invalid_argument(
          "Source table has no authenticated field reader");
  }
}
namespace {
uint32_t index(const Json &j, size_t count) {
  if (!count)
    throw std::invalid_argument(
        "Texture-set reference targets an empty owner table");
  return uint32_t(integer(j, count - 1));
}
TextureSetView origin_primary(const PolygonOrigin &o) {
  TextureSetView f;
  f.id = 0;
  f.name = o.material.name;
  f.painted = o.paint;
  f.material = o.material;
  f.appearance = o.appearance;
  f.material_bindings = o.material_bindings;
  f.bake_recipe = o.bake_recipe;
  f.domain = o.texture_sets->primary_domain();
  f.source = o.texture_sets->primary_source();
  return f;
}
TextureSetView record_view(const TextureSetRecord &r) {
  TextureSetView f;
  f.id = r.id;
  f.name = r.name;
  f.painted = r.painted;
  f.material = r.material;
  f.appearance = r.appearance;
  f.material_bindings = r.material_bindings;
  f.bake_recipe = r.bake_recipe;
  f.domain = r.domain;
  f.source = r.source;
  return f;
}
} // namespace
void TextureSetArchive::shared_records(bool enabled) {
  if (enabled == shared_)
    return;
  if (legacy_holders_ || !collections_.empty())
    throw std::logic_error(
        "Select shared texture-set records before encoding any holder");
  shared_ = enabled;
}
void TextureSetArchive::field_encoder(FieldEncoder fn,
                                      std::function<Json()> records) {
  if (!shared_ || !collections_.empty())
    throw std::logic_error("Install field writer before shared holders");
  encode_field_ = std::move(fn);
  field_records_ = std::move(records);
}
void TextureSetArchive::field_decoder(FieldDecoder fn,
                                      std::function<void()> complete) {
  if (!shared_ || !used_paints_.empty())
    throw std::logic_error(
        "Install field reader before shared paint resolution");
  decode_field_ = std::move(fn);
  field_complete_ = std::move(complete);
}
const std::shared_ptr<const Mesh> &
TextureSetArchive::geometry_at(uint32_t i) const {
  if (i >= geometry_.size())
    throw std::out_of_range("Unknown actual texture-set geometry owner");
  return geometry_[i];
}
void TextureSetArchive::charge_json(const Json &j, size_t depth) {
  if (depth > 64)
    throw std::length_error("Texture-set native payload nesting exceeds bound");
  step();
  charge(1, 128);
  if (j.is_binary())
    charge(j.get_binary().size(), 2);
  else if (j.is_string())
    charge(j.get_ref<const std::string &>().size() + 1, 1);
  else if (j.is_object()) {
    for (auto it = j.begin(); it != j.end(); ++it) {
      charge(it.key().size() + 1, 1);
      charge_json(it.value(), depth + 1);
    }
  } else if (j.is_array())
    for (const auto &v : j)
      charge_json(v, depth + 1);
}
uint32_t TextureSetArchive::add_assignment(
    const std::shared_ptr<const std::vector<TextureSetId>> &slots) {
  if (!slots || slots->empty() || slots->size() > limits_.maximum_triangles)
    throw std::invalid_argument("Shared membership has no actual slot owner");
  for (size_t i = 0; i < assignments_.size(); ++i) {
    step();
    if (same(assignments_[i], slots))
      return uint32_t(i);
  }
  if (assignments_.size() >= limits_.maximum_sets_per_scene)
    throw std::length_error("Too many actual assignment owners");
  charge(slots->size(), sizeof(TextureSetId));
  assignments_.push_back(slots);
  return uint32_t(assignments_.size() - 1);
}
uint32_t TextureSetArchive::add_domain(
    const std::shared_ptr<const TextureSetFaceDomain> &d) {
  if (!d || !d->topology() || !d->matches(d->topology()->geometry()))
    throw std::invalid_argument(
        "Shared paint requires an authenticated member domain");
  d->authenticate_owner(d);
  for (size_t i = 0; i < domains_.size(); ++i) {
    step();
    if (same(domains_[i].owner, d))
      return uint32_t(i);
  }
  if (domains_.size() >= limits_.maximum_sets_per_scene)
    throw std::length_error("Too many actual member-domain owners");
  const auto topology = add(d->topology_),
             assignment = add_assignment(d->assignments_);
  charge(d->triangles_.size(), sizeof(uint32_t));
  domains_.push_back({d, topology, assignment});
  return uint32_t(domains_.size() - 1);
}
uint32_t TextureSetArchive::add_paint(const TextureSetView &f,
                                      PaintImageArchive &images,
                                      NormalSamplingArchive *normals,
                                      PaintSamplingArchive *sampling) {
  if (!f.painted || !f.domain ||
      !same(TextureSetPaintAccess::domain(*f.painted), f.domain))
    throw std::invalid_argument(
        "Shared paint record lost its actual membership");
  for (size_t i = 0; i < paints_.size(); ++i) {
    step();
    if (same(paints_[i].owner, f.painted)) {
      if (!same(paints_[i].source, f.source) && (paints_[i].source || f.source))
        throw std::invalid_argument(
            "One paint owner has contradictory source authorities");
      return uint32_t(i);
    }
  }
  if (paints_.size() >= limits_.maximum_sets_per_scene)
    throw std::length_error("Too many actual painted-grid owners");
  const auto domain = add_domain(f.domain);
  const auto raster = TextureSetPaintAccess::raster_owner(*f.painted);
  if (!raster)
    throw std::invalid_argument("Paint has no actual raster owner");
  uint32_t raster_id = 0;
  for (; raster_id < rasters_.size(); ++raster_id) {
    step();
    if (same(rasters_[raster_id].owner, raster))
      break;
  }
  if (raster_id == rasters_.size()) {
    if (rasters_.size() >= limits_.maximum_sets_per_scene)
      throw std::length_error("Too many actual raster owners");
    charge(TextureSetPaintAccess::initial_raster_bytes(f.painted->resolution()),
           1);
    rasters_.push_back({raster, domain, f.painted->resolution(), {}});
  } else if (rasters_[raster_id].domain != domain ||
             rasters_[raster_id].resolution != f.painted->resolution())
    throw std::invalid_argument(
        "One raster owner has contradictory membership or dimensions");
  Json payload;
  if (encode_field_)
    payload =
        encode_field_(f, topology_geometry_.at(domains_[domain].topology));
  else {
    if (f.source)
      throw std::invalid_argument(
          "Source-backed paint requires its actual shared field writer");
    plain(*f.painted, f.material_bindings.get(), f.appearance.get(),
          f.bake_recipe.get());
    payload =
        TextureSetPaintAccess::encode(*f.painted, &images, normals, sampling);
  }
  charge_json(payload);
  PaintRecord entry;
  entry.owner = f.painted;
  entry.source = f.source;
  entry.raster = raster_id;
  entry.payload = std::move(payload);
  paints_.push_back(std::move(entry));
  return uint32_t(paints_.size() - 1);
}
uint32_t TextureSetArchive::add_collection(
    const std::shared_ptr<const TextureSetCollection> &c,
    PaintImageArchive &images, NormalSamplingArchive *normals,
    PaintSamplingArchive *sampling) {
  for (size_t i = 0; i < collections_.size(); ++i) {
    step();
    if (same(collections_[i].owner, c))
      return uint32_t(i);
  }
  if (!c || collections_.size() >= limits_.maximum_sets_per_scene)
    throw std::length_error("Too many actual material collection owners");
  const auto topology = add(c->topology_),
             assignment = add_assignment(c->slots_),
             primary = add_domain(c->primary_);
  Json wire = {{"topology", topology},      {"assignments", assignment},
               {"primary_domain", primary}, {"active", c->active_},
               {"next_id", c->next_},       {"additional", Json::array()}};
  for (const auto &r : c->additional_) {
    step();
    const auto domain = add_domain(r.domain),
               paint = add_paint(record_view(r), images, normals, sampling);
    wire["additional"].push_back({{"id", r.id},
                                  {"name", r.name},
                                  {"material", material(r.material)},
                                  {"domain", domain},
                                  {"painted", paint}});
  }
  charge_json(wire);
  CollectionRecord entry;
  entry.owner = c;
  entry.wire = std::move(wire);
  collections_.push_back(std::move(entry));
  return uint32_t(collections_.size() - 1);
}
Json TextureSetArchive::encode_shared(
    const std::shared_ptr<const TextureSetCollection> &c,
    const TextureSetView &p, PaintImageArchive &images,
    NormalSamplingArchive *normals, PaintSamplingArchive *sampling) {
  if (!shared_)
    throw std::logic_error(
        "Retained material collection requires shared texture-set records");
  const auto paint = add_paint(p, images, normals, sampling),
             collection = add_collection(c, images, normals, sampling);
  return {{"version", 2}, {"collection", collection}, {"primary", paint}};
}
Json TextureSetArchive::encode(const PolygonOrigin &o,
                               PaintImageArchive &images,
                               NormalSamplingArchive *normals,
                               PaintSamplingArchive *sampling) {
  if (!o.texture_sets)
    throw std::invalid_argument("Retained holder has no material collection");
  o.texture_sets->validate(o, true, limits_);
  return encode_shared(o.texture_sets, origin_primary(o), images, normals,
                       sampling);
}
void TextureSetArchive::decode_shared_tables(const Json &j) {
  shared_ = true;
  for (const auto *name : {"assignments", "domains", "rasters", "paints",
                           "collections", "field_sources"}) {
    if (!j.at(name).is_array() ||
        j.at(name).size() > limits_.maximum_sets_per_scene ||
        ((std::string_view(name) != "field_sources") && j.at(name).empty()))
      throw std::invalid_argument(
          "Invalid bounded shared texture-set owner table");
  }
  // Inspect and charge the complete payload graph before constructing a raster,
  // paint or material collection. Mesh/topology storage was measured
  // separately.
  for (const auto *name : {"assignments", "domains", "rasters", "paints",
                           "collections", "field_sources"})
    charge_json(j.at(name));
  for (const auto &v : j.at("assignments")) {
    const auto &b = blob(v, 4, limits_.maximum_triangles);
    charge(b.size(), 1);
    auto slots = std::make_shared<std::vector<TextureSetId>>();
    slots->reserve(b.size() / 4);
    for (size_t n = 0; n < b.size(); n += 4) {
      step();
      slots->push_back(TextureSetId(word(b, n, 4)));
    }
    assignments_.push_back(std::move(slots));
  }
  for (const auto &v : j.at("domains")) {
    if (!v.is_object() || v.size() != 3)
      throw std::invalid_argument("Invalid actual domain record");
    const auto t = index(v.at("topology"), topology_.size()),
               a = index(v.at("assignments"), assignments_.size());
    const auto set = TextureSetId(integer(v.at("set"), UINT32_MAX));
    const auto &topology = topology_[t];
    const auto &slots = assignments_[a];
    if (slots->size() != topology->faces_.size())
      throw std::invalid_argument(
          "Domain assignment dimensions differ from actual topology");
    size_t count = 0;
    for (auto x : *slots) {
      step();
      count += x == set;
    }
    charge(count, sizeof(uint32_t));
    auto d = std::shared_ptr<TextureSetFaceDomain>(new TextureSetFaceDomain);
    d->topology_ = topology;
    d->assignments_ = slots;
    d->set_ = set;
    d->triangles_.reserve(count);
    for (size_t n = 0; n < slots->size(); ++n)
      if (slots->at(n) == set)
        d->triangles_.push_back(uint32_t(n));
    std::shared_ptr<const TextureSetFaceDomain> sealed = d;
    d->self_ = sealed;
    domains_.push_back({std::move(sealed), t, a});
  }
  for (const auto &v : j.at("rasters")) {
    if (!v.is_object() || v.size() != 2)
      throw std::invalid_argument("Invalid actual raster record");
    const auto domain = index(v.at("domain"), domains_.size()),
               r = uint32_t(integer(v.at("resolution"), 2048));
    if (r < 64 || (r & (r - 1)))
      throw std::invalid_argument("Invalid actual raster resolution");
    charge(TextureSetPaintAccess::initial_raster_bytes(r), 1);
    rasters_.push_back({{}, domain, r, {}});
  }
  for (const auto &v : j.at("paints")) {
    if (!v.is_object() || v.size() != 2 || !v.at("painted").is_object() ||
        !v.at("painted").contains("version") ||
        v.at("painted").at("version") != 7)
      throw std::invalid_argument(
          "Shared paint table requires actual Paint7 payloads");
    PaintRecord p;
    p.raster = index(v.at("raster"), rasters_.size());
    p.payload = v.at("painted");
    paints_.push_back(std::move(p));
  }
  for (const auto &v : j.at("collections")) {
    if (!v.is_object() || v.size() != 6 || !v.at("additional").is_array() ||
        v.at("additional").empty() ||
        v.at("additional").size() + 1 > limits_.maximum_sets_per_object)
      throw std::invalid_argument("Invalid shared collection record");
    const auto t = index(v.at("topology"), topology_.size()),
               a = index(v.at("assignments"), assignments_.size()),
               p = index(v.at("primary_domain"), domains_.size());
    if (domains_[p].topology != t || domains_[p].assignment != a ||
        domains_[p].owner->set() != 0)
      throw std::invalid_argument(
          "Shared collection lost its actual primary domain");
    const auto next = integer(v.at("next_id"), uint64_t(UINT32_MAX) + 1),
               active = integer(v.at("active"), UINT32_MAX);
    std::set<TextureSetId> ids{0};
    for (const auto &r : v.at("additional")) {
      step();
      if (!r.is_object() || r.size() != 5 || !r.at("name").is_string())
        throw std::invalid_argument("Invalid shared material member");
      const auto id = TextureSetId(integer(r.at("id"), UINT32_MAX));
      if (!id || id >= next || !ids.insert(id).second)
        throw std::invalid_argument("Reused shared material identity");
      const auto d = index(r.at("domain"), domains_.size()),
                 paint = index(r.at("painted"), paints_.size());
      if (domains_[d].topology != t || domains_[d].assignment != a ||
          domains_[d].owner->set() != id ||
          rasters_[paints_[paint].raster].domain != d)
        throw std::invalid_argument(
            "Shared member borrowed a foreign paint/domain");
      material(r.at("material"));
      if (r.at("name").get_ref<const std::string &>().empty() ||
          r.at("name").get_ref<const std::string &>().size() > 256)
        throw std::invalid_argument("Invalid shared material name");
    }
    if (!ids.contains(TextureSetId(active)))
      throw std::invalid_argument("Foreign shared active material");
    std::map<uint64_t, TextureSetId> assigned;
    for (size_t n = 0; n < assignments_[a]->size(); ++n) {
      step();
      const auto slot = assignments_[a]->at(n);
      if (!ids.contains(slot) || !topology_[t]->faces_[n])
        throw std::invalid_argument(
            "Shared assignment references a foreign material/face");
      auto [it, fresh] = assigned.emplace(topology_[t]->faces_[n], slot);
      if (!fresh && it->second != slot)
        throw std::invalid_argument(
            "One logical face has inconsistent shared slots");
    }
    for (auto corner : topology_[t]->corners_)
      if (!corner)
        throw std::invalid_argument("Missing shared logical corner identity");
    CollectionRecord c;
    c.wire = v;
    collections_.push_back(std::move(c));
  }
  field_sources_ = j.at("field_sources");
}
const std::shared_ptr<const TextureSetFaceDomain> &
TextureSetArchive::domain_at(uint32_t i) {
  if (i >= domains_.size())
    throw std::out_of_range("Unknown shared domain");
  const auto &r = domains_[i];
  used_domains_.insert(i);
  used_assignments_.insert(r.assignment);
  reached_.insert(r.topology);
  return r.owner;
}
TextureSetArchiveField
TextureSetArchive::paint_at(uint32_t i, const Material &m, bool polygon,
                            const PaintImageArchive &images,
                            const NormalSamplingArchive *normals,
                            const PaintSamplingArchive *sampling,
                            const SourcePaintDecodeContext &context) {
  if (i >= paints_.size())
    throw std::out_of_range("Unknown shared painted owner");
  auto &p = paints_[i];
  if (p.state == 1)
    throw std::invalid_argument("Cyclic shared paint/source record");
  if (p.state == 2) {
    used_paints_.insert(i);
    return p.decoded;
  }
  p.state = 1;
  try {
    auto &r = rasters_.at(p.raster);
    const auto &domain = domain_at(r.domain);
    TextureSetArchiveField value;
    if (decode_field_)
      value = decode_field_(p.payload, domain,
                            topology_geometry_.at(domains_[r.domain].topology),
                            m, r.decoded.get(),polygon);
    else {
      value.paint = TextureSetPaintAccess::decode(p.payload, &images, polygon,
                                                  normals, sampling, context,
                                                  domain, r.decoded.get());
      plain(*value.paint, nullptr, nullptr, nullptr);
    }
    if (!value.paint ||
        !same(value.paint->mesh_owner(), domain->topology()->geometry()) ||
        !same(TextureSetPaintAccess::domain(*value.paint), domain) ||
        value.paint->resolution() != r.resolution)
      throw std::invalid_argument(
          "Decoded shared paint lost its actual geometry/domain/dimensions");
    const auto raster = TextureSetPaintAccess::raster_owner(*value.paint);
    if (r.decoded &&
        !same(raster, TextureSetPaintAccess::raster_owner(*r.decoded)))
      throw std::invalid_argument(
          "Field reader failed to preserve the actual shared raster owner");
    if (!r.decoded)
      r.decoded = value.paint;
    p.decoded = std::move(value);
    p.state = 2;
    used_rasters_.insert(p.raster);
    used_paints_.insert(i);
    return p.decoded;
  } catch (...) {
    p.state = 0;
    throw;
  }
}
std::shared_ptr<TextureSetCollection> TextureSetArchive::collection_at(
    uint32_t i, bool polygon, const PaintImageArchive &images,
    const NormalSamplingArchive *normals, const PaintSamplingArchive *sampling,
    const SourcePaintDecodeContext &context) {
  if (i >= collections_.size())
    throw std::out_of_range("Unknown shared collection");
  auto &entry = collections_[i];
  if (entry.state == 1)
    throw std::invalid_argument("Cyclic shared collection/source record");
  if (entry.state == 2) {
    used_collections_.insert(i);
    return entry.decoded;
  }
  entry.state = 1;
  try {
    const auto &v = entry.wire;
    auto c = std::shared_ptr<TextureSetCollection>(new TextureSetCollection);
    c->topology_ = topology_.at(index(v.at("topology"), topology_.size()));
    c->slots_ =
        assignments_.at(index(v.at("assignments"), assignments_.size()));
    c->primary_ = domain_at(index(v.at("primary_domain"), domains_.size()));
    c->active_ = TextureSetId(integer(v.at("active"), UINT32_MAX));
    c->next_ = integer(v.at("next_id"), uint64_t(UINT32_MAX) + 1);
    for (const auto &r : v.at("additional")) {
      TextureSetRecord f;
      f.id = TextureSetId(integer(r.at("id"), UINT32_MAX));
      f.name = r.at("name").get<std::string>();
      f.material = material(r.at("material"));
      f.domain = domain_at(index(r.at("domain"), domains_.size()));
      const auto p =
          paint_at(index(r.at("painted"), paints_.size()), f.material, polygon,
                   images, normals, sampling, context);
      f.painted = p.paint;
      f.source = p.source;
      f.material_bindings = p.binding;
      f.appearance = p.appearance;
      f.bake_recipe = p.recipe;
      c->additional_.push_back(std::move(f));
    }
    entry.decoded = std::move(c);
    entry.state = 2;
    used_collections_.insert(i);
    return entry.decoded;
  } catch (...) {
    entry.state = 0;
    throw;
  }
}
std::vector<const Json *>
TextureSetArchive::referenced_paints(const Json &holder, const Json &geometry) {
  if (!holder.is_object() || holder.size() != 3 || holder.at("version") != 2 ||
      !geometry.is_object() || geometry.at("version") != 2)
    throw std::invalid_argument(
        "Texture-set shared reference requires actual owning geometry2");
  const auto &collections = geometry.at("collections");
  const auto &paints = geometry.at("paints");
  const auto &rasters = geometry.at("rasters");
  const auto &domains = geometry.at("domains");
  for (const auto *table : {&collections, &paints, &rasters, &domains})
    if (!table->is_array() || table->empty() || table->size() > 4096)
      throw std::length_error(
          "Shared texture-set preflight table bound exceeded");
  const auto &c =
      collections.at(index(holder.at("collection"), collections.size()));
  const auto domain = index(c.at("primary_domain"), domains.size());
  const auto &primary = paints.at(index(holder.at("primary"), paints.size()));
  const auto &raster = rasters.at(index(primary.at("raster"), rasters.size()));
  if (index(raster.at("domain"), domains.size()) != domain)
    throw std::invalid_argument("Shared primary has foreign domain");
  if (!c.at("additional").is_array() || c.at("additional").empty() ||
      c.at("additional").size() > 63)
    throw std::length_error("Shared texture-set member count exceeded");
  std::vector<const Json *> result{&primary.at("painted")};
  for (const auto &field : c.at("additional")) {
    const auto &p = paints.at(index(field.at("painted"), paints.size()));
    const auto &r = rasters.at(index(p.at("raster"), rasters.size()));
    if (index(r.at("domain"), domains.size()) !=
        index(field.at("domain"), domains.size()))
      throw std::invalid_argument("Shared additional field has foreign domain");
    result.push_back(&p.at("painted"));
  }
  return result;
}
void TextureSetArchive::preflight_reference(const Json &holder,
                                            const Json &geometry) {
  (void)referenced_paints(holder, geometry);
}
void TextureSetArchive::preflight_holder(const Json &j) const {
  if (!shared_ || !j.is_object() || j.size() != 3 || j.at("version") != 2)
    throw std::invalid_argument(
        "Shared owner requires an actual version2 holder reference");
  const auto c = index(j.at("collection"), collections_.size()),
             p = index(j.at("primary"), paints_.size());
  const auto d =
      index(collections_[c].wire.at("primary_domain"), domains_.size());
  if (rasters_.at(paints_[p].raster).domain != d)
    throw std::invalid_argument(
        "Primary paint references another collection's actual domain");
}
TextureSetArchive::Holder
TextureSetArchive::decode_holder(const Json &j, const Material &m, bool polygon,
                                 const PaintImageArchive &images,
                                 const NormalSamplingArchive *normals,
                                 const PaintSamplingArchive *sampling,
                                 const SourcePaintDecodeContext &context) {
  preflight_holder(j);
  const auto id = index(j.at("collection"), collections_.size());
  auto primary = paint_at(index(j.at("primary"), paints_.size()), m, polygon,
                          images, normals, sampling, context);
  auto c = collection_at(id, polygon, images, normals, sampling, context);
  auto &entry = collections_[id];
  if (entry.primary_source_ready) {
    if (!same(c->primary_source_, primary.source) &&
        (c->primary_source_ || primary.source))
      throw std::invalid_argument(
          "Shared collection has contradictory primary source owners");
  } else {
    c->primary_source_ = primary.source;
    entry.primary_source_ready = true;
  }
  return {std::move(c), std::move(primary)};
}
void TextureSetArchive::decode(PolygonOrigin &o, const Json &j,
                               const PaintImageArchive &images,
                               const NormalSamplingArchive *normals,
                               const PaintSamplingArchive *sampling,
                               const SourcePaintDecodeContext &context) {
  auto result = decode_holder(j, o.material, bool(o.polygon), images, normals,
                              sampling, context);
  primary_context(o.material_bindings, o.appearance, o.bake_recipe,
                  result.primary);
  auto candidate = o;
  candidate.paint = result.primary.paint;
  candidate.texture_sets = result.collection;
  candidate.material_bindings = result.primary.binding;
  candidate.appearance = result.primary.appearance;
  candidate.bake_recipe = result.primary.recipe;
  result.collection->validate(candidate, true, limits_);
  o = std::move(candidate);
}

} // namespace dome::detail
