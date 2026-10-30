// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/retopo-authoring.hpp"
#include "dome/color-preparation.hpp"
#include "source-paint-pairing.hpp"
#include "normal-frame-runtime.hpp"
#include "polygon-scene-archive.hpp"
#include "source-placement-archive.hpp"
#include "polygon-paint-authority.hpp"
#include "text-utf8.hpp"
#include <map>
#include <nlohmann/json.hpp>
#include <set>
namespace dome {
void validate_polygon_derivative(const PolygonAuthority &authority,
                                 const PaintedMesh *paint, bool mesh_active,
                                 const PolygonLimits &limits) {
  authority.topology.validate(limits);
  if (authority.colors)
    authority.colors->validate(authority.topology, limits);
  if (!authority.active)
    return;
  if (authority.topology.faces().empty()) {
    if (mesh_active || paint)
      throw std::invalid_argument(
          "Empty polygon authority must have no active painting derivative");
    return;
  }
  if (!mesh_active || !paint)
    throw std::invalid_argument(
        "Active polygon authority requires its painting derivative");
  auto derivative = authority.topology.triangulate(limits).mesh;
  const auto &rest = paint->mesh();
  if (derivative.positions.size() != rest.positions.size() ||
      derivative.indices != rest.indices || derivative.uv != rest.uv)
    throw std::invalid_argument(
        "Polygon painting derivative differs from editable topology");
  for (size_t i = 0; i < rest.positions.size(); ++i) {
    const auto &a = derivative.positions[i], &b = rest.positions[i];
    if (a.x != b.x || a.y != b.y || a.z != b.z)
      throw std::invalid_argument(
          "Polygon REST derivative positions differ from editable topology");
  }
}
namespace detail {
namespace {
constexpr size_t maximum_nodes = 512, maximum_depth = 32,
                 maximum_entries = 8'000'000;
void name_valid(std::string_view name) {
  if (name.empty() || name.size() > 256 || name.find('\0') != name.npos ||
      !text::valid_utf8(name))
    throw std::invalid_argument("Invalid polygon source name");
}
void sum(size_t &n, size_t amount) {
  if (amount > maximum_entries - n)
    throw std::length_error(
        "Scene polygon component/provenance budget exceeded");
  n += amount;
}
uint64_t unsigned_id(const nlohmann::json &v) {
  if (!v.is_number_unsigned() &&
      (!v.is_number_integer() || v.get<int64_t>() < 0))
    throw std::invalid_argument("Invalid polygon source identity");
  return v.get<uint64_t>();
}
template <class T>
size_t find(const std::vector<std::shared_ptr<const T>> &values,
            const T *value) {
  for (size_t i = 0; i < values.size(); ++i)
    if (values[i].get() == value)
      return i;
  return values.size();
}
template <class T>
uint32_t insert(std::vector<std::shared_ptr<const T>> &values,
                std::shared_ptr<const T> value) {
  auto index = find(values, value.get());
  if (index == values.size()) {
    if (index >= maximum_nodes)
      throw std::length_error("Scene polygon snapshot count exceeded");
    values.push_back(std::move(value));
  }
  return uint32_t(index);
}
nlohmann::json material(const Material &m) {
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
Material material(const nlohmann::json &j) {
  if (!j.is_object() || j.size() != 10)
    throw std::invalid_argument("Invalid polygon source material");
  Material m;
  m.name = j.at("name").get<std::string>();
  m.color = j.at("color").get<std::array<float, 4>>();
  m.metallic = j.at("metallic").get<float>();
  m.roughness = j.at("roughness").get<float>();
  m.emissive = j.at("emissive").get<std::array<float, 3>>();
  m.toon = j.at("toon").get<bool>();
  m.double_sided = j.at("double_sided").get<bool>();
  auto mode = unsigned_id(j.at("alpha"));
  if (mode > 3)
    throw std::invalid_argument("Invalid polygon material alpha");
  m.alpha = Material::AlphaMode(mode);
  m.alpha_cutoff = j.at("alpha_cutoff").get<float>();
  m.unlit = j.at("unlit").get<bool>();
  m.validate();
  return m;
}
void topology_preflight(const nlohmann::json &j, size_t &entries) {
  if (!j.is_object() || j.size() != 5 || j.at("version") != 1 ||
      !j.at("version").is_number_integer())
    throw std::invalid_argument("Invalid polygon topology archive");
  const auto &v = j.at("vertices"), &e = j.at("edges"), &f = j.at("faces");
  if (!v.is_array() || !e.is_array() || !f.is_array() || v.size() > 500'000 ||
      f.size() > 500'000 || e.size() > 2'000'000)
    throw std::length_error("Polygon component count exceeded");
  sum(entries, v.size());
  sum(entries, e.size());
  sum(entries, f.size());
  size_t corners = 0;
  for (const auto &vertex : v) {
    if (!vertex.is_array() || vertex.size() != 5 || !vertex[4].is_array() ||
        vertex[4].size() > 128)
      throw std::invalid_argument("Invalid polygon provenance record");
    sum(entries, vertex[4].size());
  }
  for (const auto &face : f) {
    if (!face.is_object() || face.size() != 3 || !face.at("corners").is_array())
      throw std::invalid_argument("Invalid polygon face record");
    auto n = face.at("corners").size();
    if (n > 256 || n > 2'000'000 - corners)
      throw std::length_error("Polygon corner count exceeded");
    corners += n;
    sum(entries, n);
  }
}
} // namespace
uint32_t polygon_archive_index(const nlohmann::json &j, size_t count) {
  auto value = unsigned_id(j);
  if (value >= count || value > UINT32_MAX)
    throw std::invalid_argument("Invalid polygon archive reference");
  return uint32_t(value);
}
uint32_t PolygonSceneArchive::add(std::shared_ptr<const PolygonAuthority> value,bool deep) {
  if(!value)throw std::invalid_argument("Null polygon authority");
  auto existing=find(authorities_,value.get());
  if(existing<authorities_.size() && (!deep || deeply_validated_.contains(value.get())))return uint32_t(existing);
  collect(value,{},deep);return uint32_t(find(authorities_,value.get()));
}
uint32_t PolygonSceneArchive::add_origin(std::shared_ptr<const PolygonOrigin> value,bool deep) {
  if(!value)throw std::invalid_argument("Null direct object source");
  direct_origins_=true;
  auto existing=find(origins_,value.get());
  if(existing<origins_.size() && (!deep || deeply_validated_origins_.contains(value.get())))return uint32_t(existing);
  collect({},value,deep);return uint32_t(find(origins_,value.get()));
}
void PolygonSceneArchive::collect(std::shared_ptr<const PolygonAuthority> root,
    std::shared_ptr<const PolygonOrigin> source,bool deep) {
  std::set<const PolygonAuthority*> active_a,done_a;
  std::set<const PolygonOrigin*> active_o,done_o;
  std::function<void(std::shared_ptr<const PolygonAuthority>,size_t)> authority;
  std::function<void(std::shared_ptr<const PolygonOrigin>,size_t)> origin;
  authority=[&](auto a,size_t depth){
    if(depth>maximum_depth)throw std::length_error("Object source history depth exceeded");
    if(active_a.contains(a.get()))throw std::invalid_argument("Object source history cycle");
    if(done_a.contains(a.get()))return;
    active_a.insert(a.get());
    const bool known=find(authorities_,a.get())<authorities_.size();insert(authorities_,a);
    if(!known){
      sum(entries_,a->topology.vertices().size());sum(entries_,a->topology.edges().size());sum(entries_,a->topology.faces().size());
      for(const auto &v:a->topology.vertices())sum(entries_,v.provenance.size());
      for(const auto &f:a->topology.faces())sum(entries_,f.corners.size());
    }
    if(deep && !deeply_validated_.contains(a.get())){a->topology.validate();if(a->colors)a->colors->validate(a->topology);}
    if(a->colors){auto before=colors_.entries();colors_.add(a->colors);sum(entries_,colors_.entries()-before);}
    if(a->origin)origin(a->origin,depth);
    if(deep)deeply_validated_.insert(a.get());
    active_a.erase(a.get());done_a.insert(a.get());
  };
  origin=[&](auto o,size_t depth){
    if(depth>maximum_depth)throw std::length_error("Object source history depth exceeded");
    if(active_o.contains(o.get()))throw std::invalid_argument("Object source history cycle");
    if(done_o.contains(o.get()))return;
    active_o.insert(o.get());insert(origins_,o);
    const bool validate_deep=deep && !deeply_validated_origins_.contains(o.get());
          if (!o->volume || o->volume->in_stroke())
            throw std::invalid_argument(
                "Polygon source requires a finalized retained Volume");
          name_valid(o->name);
          o->material.validate();
          if(o->placement)o->placement->validate();
          else validate_transform(o->world_transform);
          insert(volumes_, o->volume);
          require_polygon_paint_owner(o->paint.get(),o->polygon.get(),o->mesh_active);
          if (o->paint) {
            if (o->paint->in_stroke())
              throw std::invalid_argument(
                  "Polygon source has unfinished paint");
            insert(paints_, o->paint);
          }
          if (o->mesh_active && !o->paint)
            throw std::invalid_argument(
                "Polygon source has no active rest mesh");
          if (o->deformation) {
            if (!o->paint)
              throw std::invalid_argument(
                  "Polygon source deformation has no rest mesh");
            if (validate_deep)
              o->deformation->validate(o->paint->mesh());
          }
          if ((o->appearance || o->material_bindings || o->retopology) &&
              !o->paint)
            throw std::invalid_argument(
                "Polygon source attributes have no rest mesh");
          if (validate_deep && o->appearance) {
            if (o->appearance->source_vertices.size() !=
                o->paint->mesh().positions.size())
              throw std::invalid_argument(
                  "Polygon source appearance remap differs from rest mesh");
          }
          if (o->material_bindings) {
            o->material_bindings->validate(validate_deep);
            if (validate_deep && o->material_bindings->normal_frames)
              normal_frame_current_authority(*o->material_bindings,
                                             o->paint->mesh(),
                                             o->deformation.get());
          }
          source_paint_pairing(o->paint.get(),o->material_bindings.get(),o->appearance.get(),validate_deep);
          const auto projection=o->paint?o->paint->color_projection():nullptr;
          const auto preparation=o->material_bindings?
              o->material_bindings->color_preparation:nullptr;
          if(o->polygon && o->polygon->active && o->polygon->colors && (projection || preparation))
            throw std::invalid_argument("Retained polygon corner colors conflict with prepared color authority");
          if(bool(projection)!=bool(preparation))
            throw std::invalid_argument("Polygon source color projection and retained preparation differ");
          if(projection) {
            if(projection->vertex_rgba!=preparation->projection->vertex_rgba ||
               projection->maximum_error!=preparation->projection->maximum_error ||
               projection->quality_cells!=preparation->projection->quality_cells)
              throw std::invalid_argument("Polygon source color projection is not its recorded prepared field");
            projection->validate(o->paint->resolution());
            if(validate_deep)
              preparation->validate_current(o->paint->mesh(),o->appearance.get());
          }
          if (o->retopology) {
            if (!o->retopology->source)
              throw std::invalid_argument(
                  "Invalid retained polygon retopology source");
            if (validate_deep)
              o->retopology->topology.validate();
          }

    if(o->polygon){authority(o->polygon,depth+1);if(validate_deep)validate_polygon_derivative(*o->polygon,o->paint.get(),o->mesh_active);}
    if(o->shape_source)origin(o->shape_source,depth+1);
    if(deep)deeply_validated_origins_.insert(o.get());
    active_o.erase(o.get());done_o.insert(o.get());
  };
  if(root)authority(root,1);else origin(source,1);
}
const std::shared_ptr<const PolygonAuthority> &
PolygonSceneArchive::at(uint32_t i) const {
  if (i >= authorities_.size())
    throw std::invalid_argument("Invalid polygon authority reference");
  return authorities_[i];
}
nlohmann::json
PolygonSceneArchive::encode(PaintImageArchive &images,
                            CreatorAssetArchive &models,
                            SourceMaterialArchive &materials,SourcePlacementArchive *placements) const {
  const bool affine=std::any_of(origins_.begin(),origins_.end(),[](const auto&o){return bool(o->placement);});
  const bool shapes=direct_origins_ || std::any_of(origins_.begin(),origins_.end(),[](const auto&o){return bool(o->shape_source);});
  if(affine&&!placements)throw std::logic_error("Polygon source placement requires its shared archive");
  nlohmann::json j = {{"version", shapes?4:!colors_.empty()?3:affine?2:1},
                      {"authorities", nlohmann::json::array()},
                      {"origins", nlohmann::json::array()},
                      {"volumes", nlohmann::json::array()},
                      {"paints", nlohmann::json::array()}};
  for (const auto &a : authorities_)
    j["authorities"].push_back(
        {{"topology", a->topology.encode()},
         {"active", a->active},
         {"origin", a->origin ? nlohmann::json(find(origins_, a->origin.get()))
                              : nlohmann::json(nullptr)}});
  if (!colors_.empty()) {
    auto encoded = colors_.encode();
    j["color_bases"] = std::move(encoded["color_bases"]);
    j["color_fields"] = std::move(encoded["color_fields"]);
    for (size_t i=0;i<authorities_.size();++i) {
      const auto &field=authorities_[i]->colors;
      j["authorities"][i]["colors"]=field
          ? nlohmann::json(find(colors_.fields(),field.get()))
          : nlohmann::json(nullptr);
    }
  }
  for (const auto &o : origins_) {
    auto t = o->world_transform;
    nlohmann::json retopo = nullptr;
    if (o->retopology)
      retopo = {{"source", o->retopology->source},
                {"topology", o->retopology->topology.encode()}};
    j["origins"].push_back(
        {{"volume", find(volumes_, o->volume.get())},
         {"painted", o->paint ? nlohmann::json(find(paints_, o->paint.get()))
                              : nlohmann::json(nullptr)},
         {"material", material(o->material)},
         {"deformation", o->deformation
                             ? o->deformation->encode(o->paint->mesh())
                             : nlohmann::json(nullptr)},
         {"appearance", o->appearance ? encode_creator_appearance(
                                            *o->appearance, models, images)
                                      : nlohmann::json(nullptr)},
         {"material_bindings",
          o->material_bindings
              ? nlohmann::json(materials.add(o->material_bindings))
              : nlohmann::json(nullptr)},
         {"retopology", std::move(retopo)},
         {"polygon", o->polygon
                         ? nlohmann::json(find(authorities_, o->polygon.get()))
                         : nlohmann::json(nullptr)},
         {"mesh_active", o->mesh_active},
         {"translation", {t.position.x, t.position.y, t.position.z}},
         {"rotation",
          {t.orientation.x, t.orientation.y, t.orientation.z, t.orientation.w}},
         {"scale", t.scale},
         {"name", o->name},
         {"source_object", o->source_object}});
    if(shapes)j["origins"].back()["shape_source"]=o->shape_source?nlohmann::json(find(origins_,o->shape_source.get())):nlohmann::json(nullptr);
    if(o->placement) {
      auto &record=j["origins"].back();record.erase("translation");record.erase("rotation");record.erase("scale");
      record["placement"]=placements->add_capsule(o->placement);
    }
  }
  for (const auto &v : volumes_)
    j["volumes"].push_back(v->encode());
  for (const auto &p : paints_)
    j["paints"].push_back(p->encode(&images));
  return j;
}
void PolygonSceneArchive::preflight(const nlohmann::json &j,
                                    const nlohmann::json &objects) {
  if (!j.is_object() || !j.at("version").is_number_integer() ||
      (j.at("version") != 1 && j.at("version") != 2 && j.at("version") != 3 && j.at("version") != 4))
    throw std::invalid_argument("Unsupported polygon source archive");
  const bool colored = j.at("version") == 3 || (j.at("version")==4 && (j.contains("color_bases")||j.contains("color_fields")));
  if (j.size() != (colored ? 7u : 5u))
    throw std::invalid_argument("Incomplete polygon color source archive");
  for (const auto *key : {"authorities", "origins", "volumes", "paints"})
    if (!j.at(key).is_array() || j.at(key).size() > maximum_nodes)
      throw std::length_error("Polygon snapshot count exceeded");
  const auto &aa = j.at("authorities"), &oo = j.at("origins");
  size_t entries = 0;
  std::vector<uint8_t> used_colors;
  if (colored) {
    PolygonColorArchive::preflight(j.at("color_bases"),j.at("color_fields"),entries);
    if (j.at("color_fields").empty())
      throw std::invalid_argument("Empty Scene14 color archive");
    used_colors.resize(j.at("color_fields").size());
  }
  for (const auto &a : aa) {
    if (!a.is_object() || a.size() != (colored ? 4u : 3u) || !a.at("active").is_boolean())
      throw std::invalid_argument("Invalid polygon authority record");
    topology_preflight(a.at("topology"), entries);
    if (colored && !a.at("colors").is_null())
      used_colors[polygon_archive_index(a.at("colors"),used_colors.size())]=1;
    if (!a.at("origin").is_null())
      polygon_archive_index(a.at("origin"), oo.size());
  }
  for (const auto &o : oo) {
    const bool affine=o.contains("placement"),shapes=j.at("version")==4;
    if(shapes){if(!o.contains("shape_source"))throw std::invalid_argument("Incomplete object source history");if(!o.at("shape_source").is_null())polygon_archive_index(o.at("shape_source"),oo.size());}
    else if(o.contains("shape_source"))throw std::invalid_argument("Shape source requires archive version four");
    if (!o.is_object() || o.size() != (affine?12u:14u)+(shapes?1u:0u) || !o.at("mesh_active").is_boolean() ||
        !o.at("name").is_string())
      throw std::invalid_argument("Invalid polygon origin record");
    if(affine) {
      if(j.at("version")<2 || o.contains("translation")||o.contains("rotation")||o.contains("scale") || unsigned_id(o.at("placement"))>UINT32_MAX)
        throw std::invalid_argument("Invalid exact polygon source placement declaration");
    } else {auto p=o.at("translation").get<std::array<float,3>>();auto q=o.at("rotation").get<std::array<float,4>>();validate_transform({{p[0],p[1],p[2]},{q[0],q[1],q[2],q[3]},o.at("scale").get<float>()});}
    name_valid(o.at("name").get_ref<const std::string &>());
    unsigned_id(o.at("source_object"));
    polygon_archive_index(o.at("volume"), j.at("volumes").size());
    if (!o.at("painted").is_null())
      polygon_archive_index(o.at("painted"), j.at("paints").size());
    if (!o.at("polygon").is_null())
      polygon_archive_index(o.at("polygon"), aa.size());
    if(!o.at("painted").is_null()&&polygon_paint_policy(j.at("paints")[polygon_archive_index(o.at("painted"),j.at("paints").size())])){
      if(o.at("mesh_active")!=true||o.at("polygon").is_null())
        throw std::invalid_argument("Retained polygon-authored paint requires its own active authority");
      auto authority=polygon_archive_index(o.at("polygon"),aa.size());
      if(aa[authority].at("active")!=true||aa[authority].at("topology").at("faces").empty())
        throw std::invalid_argument("Retained polygon-authored paint has inactive authority");
    }
    if (o.at("painted").is_null() &&
        (o.at("mesh_active") == true || !o.at("deformation").is_null() ||
         !o.at("appearance").is_null() ||
         !o.at("material_bindings").is_null() || !o.at("retopology").is_null()))
      throw std::invalid_argument(
          "Retained polygon source attributes need a rest mesh");
  }
  std::vector<uint8_t> status_a(aa.size()),status_o(oo.size()),
      used_volumes(j.at("volumes").size()),used_paints(j.at("paints").size());
  std::vector<size_t> height_a(aa.size()),height_o(oo.size());
  std::function<size_t(size_t,size_t)> authority,origin;
  auto enter=[&](auto &status,auto &height,size_t i,size_t depth){
    if(depth>maximum_depth)throw std::length_error("Object source history depth exceeded");
    if(status[i]==1)throw std::invalid_argument("Object source history cycle");
    if(status[i]==2){if(height[i]>maximum_depth-depth+1)throw std::length_error("Object source history depth exceeded");return false;}
    status[i]=1;return true;
  };
  authority=[&](size_t i,size_t depth)->size_t{
    if(!enter(status_a,height_a,i,depth))return height_a[i];
    size_t h=1;if(!aa[i].at("origin").is_null())h=origin(polygon_archive_index(aa[i].at("origin"),oo.size()),depth);
    status_a[i]=2;return height_a[i]=h;
  };
  origin=[&](size_t i,size_t depth)->size_t{
    if(!enter(status_o,height_o,i,depth))return height_o[i];
    const auto &o=oo[i];used_volumes[polygon_archive_index(o.at("volume"),used_volumes.size())]=1;
    if(!o.at("painted").is_null())used_paints[polygon_archive_index(o.at("painted"),used_paints.size())]=1;
    size_t child=0;
    if(!o.at("polygon").is_null())child=authority(polygon_archive_index(o.at("polygon"),aa.size()),depth+1);
    if(o.contains("shape_source")&&!o.at("shape_source").is_null())child=std::max(child,origin(polygon_archive_index(o.at("shape_source"),oo.size()),depth+1));
    status_o[i]=2;return height_o[i]=child+1;
  };
  for(const auto &o:objects){
    if(!o.contains("polygon"))throw std::invalid_argument("Incomplete polygon scene fields");
    if(!o.at("polygon").is_null())authority(polygon_archive_index(o.at("polygon"),aa.size()),1);
    if(o.contains("shape_source")){
      if(j.at("version")!=4)throw std::invalid_argument("Direct shape source requires archive version four");
      if(!o.at("shape_source").is_null())origin(polygon_archive_index(o.at("shape_source"),oo.size()),1);
    }
  }
  auto all = [](const auto &v) {
    return std::all_of(v.begin(), v.end(), [](auto n) { return n != 0; });
  };
  if (!all(status_a) || !all(status_o) || !all(used_volumes) ||
      !all(used_paints) || !all(used_colors))
    throw std::invalid_argument("Unreachable polygon source archive records");
}
PolygonSceneArchive PolygonSceneArchive::decode(
    const nlohmann::json &j, const PaintImageArchive &images,
    const CreatorAssetArchive &models, const SourceMaterialArchive &materials,const SourcePlacementArchive *placements) {
  PolygonSceneArchive out;out.direct_origins_=j.at("version")==4;
  const bool colored=j.at("version")==3 || j.contains("color_fields");
  if(colored)out.colors_=PolygonColorArchive::decode(j.at("color_bases"),j.at("color_fields"));
  for (const auto &v : j.at("volumes"))
    out.volumes_.push_back(std::make_shared<const Volume>(Volume::decode(v)));
  for (const auto &p : j.at("paints"))
    out.paints_.push_back(PaintedMesh::decode(p, &images, true));
  std::vector<std::shared_ptr<PolygonAuthority>> authorities;
  std::vector<std::shared_ptr<PolygonOrigin>> origins;
  for (const auto &a : j.at("authorities")) {
    auto value = std::make_shared<PolygonAuthority>();
    value->topology = PolygonMesh::decode(a.at("topology"));
    value->active = a.at("active").get<bool>();
    if (colored && !a.at("colors").is_null())
      value->colors=out.colors_.at(polygon_archive_index(a.at("colors"),out.colors_.fields().size()));
    authorities.push_back(value);
    out.authorities_.push_back(value);
  }
  for (const auto &o : j.at("origins")) {
    auto value = std::make_shared<PolygonOrigin>();
    value->volume = out.volumes_[polygon_archive_index(o.at("volume"),
                                                       out.volumes_.size())];
    if (!o.at("painted").is_null())
      value->paint = out.paints_[polygon_archive_index(o.at("painted"),
                                                       out.paints_.size())];
    value->material = material(o.at("material"));
    value->mesh_active = o.at("mesh_active").get<bool>();
    value->name = o.at("name").get<std::string>();
    value->source_object = unsigned_id(o.at("source_object"));
    if(o.contains("placement")) {
      if(!placements)throw std::invalid_argument("Polygon source placement archive is missing");
      value->placement=placements->capsule(uint32_t(unsigned_id(o.at("placement"))));
    } else {
      auto p = o.at("translation").get<std::array<float, 3>>();
      auto q = o.at("rotation").get<std::array<float, 4>>();
      value->world_transform = {{p[0], p[1], p[2]}, {q[0], q[1], q[2], q[3]},o.at("scale").get<float>()};
      validate_transform(value->world_transform);
    }
    if (!o.at("deformation").is_null())
      value->deformation = std::make_shared<const DeformationState>(
          DeformationState::decode(o.at("deformation"), value->paint->mesh()));
    if (!o.at("appearance").is_null())
      value->appearance = decode_creator_appearance(o.at("appearance"), models);
    if (!o.at("material_bindings").is_null())
      value->material_bindings = materials.at(polygon_archive_index(
          o.at("material_bindings"), materials.bindings().size()));
    if (!o.at("retopology").is_null()) {
      const auto &r = o.at("retopology");
      value->retopology = std::make_shared<const RetopoPatch>(RetopoPatch{
          unsigned_id(r.at("source")),
          RetopoMesh::decode(r.at("topology").get_ref<const std::string &>())});
    }
    if (!o.at("polygon").is_null())
      value->polygon = out.at(
          polygon_archive_index(o.at("polygon"), out.authorities_.size()));
    origins.push_back(value);
    out.origins_.push_back(value);
  }
  for(size_t i=0;i<origins.size();++i){const auto &record=j.at("origins")[i];
    if(record.contains("shape_source")&&!record.at("shape_source").is_null())origins[i]->shape_source=out.origins_[polygon_archive_index(record.at("shape_source"),out.origins_.size())];
  }
  for (size_t i = 0; i < authorities.size(); ++i)
    if (!j.at("authorities")[i].at("origin").is_null())
      authorities[i]->origin = out.origins_[polygon_archive_index(
          j.at("authorities")[i].at("origin"), out.origins_.size())];
  return out;
}
} // namespace detail
} // namespace dome
