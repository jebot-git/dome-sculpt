// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include "polygon-paint-authority.hpp"
#include "scene-feature-version.hpp"
#include "dome/source-placement.hpp"
#include "source-placement-archive.hpp"
#include "dome/import-source.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/source-maps.hpp"
#include "dome/creator-archive.hpp"
#include "dome/material-bindings.hpp"
#include "dome/color-preparation.hpp"
#include "normal-frame-runtime.hpp"
#include "dome/retopo-authoring.hpp"
#include "io.hpp"
#include "text-utf8.hpp"
#include "volume-archive.hpp"
#include "polygon-scene-archive.hpp"
#include "polygon-scene-budget.hpp"
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
namespace dome {
namespace {
void name_valid(const std::string &s) {
  if (s.empty() || s.size() > 256 || s.find('\0') != std::string::npos || !text::valid_utf8(s))
    throw std::invalid_argument(
        "Names must contain 1 to 256 valid UTF-8 bytes without NUL");
}
void retopology_acyclic(const std::map<uint64_t,uint64_t> &references) {
  for(const auto &reference:references) {
    std::set<uint64_t> visited;auto current=reference.first;
    for(;;) {
      auto found=references.find(current);if(found==references.end())break;
      if(!visited.insert(current).second)throw std::invalid_argument("Retopology reference cycle");
      current=found->second;
    }
  }
}
} // namespace
void Material::validate() const {
  name_valid(name);
  auto unit = [](float f) { return std::isfinite(f) && f >= 0 && f <= 1; };
  if (alpha < AlphaMode::Automatic || alpha > AlphaMode::Blend || !unit(alpha_cutoff))
    throw std::invalid_argument("Invalid material alpha mode/cutoff");
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
bool source_is_dormant(const SculptObject &o) {
  if(o.polygon && o.polygon->active)return false;
  return !o.mesh_active && bool(o.appearance || o.material_bindings || o.deformation || o.retopology || o.polygon || o.shape_source ||
    (o.painted && (o.painted->source_maps() || o.painted->baked() || o.painted->has_paint_data())));
}
void Scene::require_exportable_sources() const {
  for(const auto &o:objects_)if(o.visible && source_is_dormant(o))
    throw std::invalid_argument("Original source data is dormant for "+o.name+
      "; restore the original mesh or explicitly reproject and bake its source data before export");
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
  o.id = next_;
  o.name = std::move(name);
  o.transform = t;
  o.volume = std::make_shared<Volume>(std::move(v));
  objects_.push_back(std::move(o));
  active_ = objects_.back().id;
  ++next_;
  return active_;
}
uint64_t Scene::duplicate(uint64_t id) {
  auto source=object(id);
  name_valid(source.name);validate_transform(source.transform);
  source.locked=false; // A duplicate starts editable, matching add().
  if(objects_.size()>=256||next_==UINT64_MAX)throw std::length_error("Scene object limit reached");
  source.name=text::utf8_prefix(source.name,240)+" copy";
  source.volume=std::make_shared<Volume>(source.volume->geometry_snapshot());
  if(source.painted)source.painted=source.painted->snapshot();
  source.id=next_;
  objects_.push_back(std::move(source));
  active_=next_++;return active_;
}
Mesh ObjectSurface::preview_mesh() const {
  return deformation ? deformation->preview(mesh) : mesh;
}
void Scene::set_deformation(uint64_t id, DeformationState state) {
  auto &o = object(id);
  if (!o.mesh_active || !o.painted || o.painted->in_stroke())
    throw std::logic_error("Deformation requires a finalized active rest mesh");
  state.validate(o.painted->mesh());
  size_t entries = state.resource_entries();
  for (const auto &other : objects_)
    if (other.id != id && other.deformation) {
      entries += other.deformation->resource_entries();
      if (entries > 8'000'000)
        throw std::length_error("Scene deformation budget exceeded");
    }
  o.deformation = std::make_shared<const DeformationState>(std::move(state));
}
void Scene::clear_deformation(uint64_t id) { object(id).deformation.reset(); }
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
  for (auto &g : result.groups_)
    if (g.humanoid) g.humanoid = std::make_shared<const HumanoidControls>(*g.humanoid);
  for (auto &o : result.objects_) {
    o.volume = std::make_shared<Volume>(o.volume->geometry_snapshot());
    if (o.painted)
      o.painted = o.painted->snapshot();
  }
  return result;
}
void Scene::validate(bool deep_sources) const {
  if (objects_.empty() || objects_.size() > 256)
    throw std::length_error("Invalid scene object count");
  if (groups_.size() > 128) throw std::length_error("Scene group limit exceeded");
  std::set<uint64_t> group_ids;
  for (auto &g : groups_) {
    if (!g.id || g.id >= next_group_ || !group_ids.insert(g.id).second)
      throw std::invalid_argument("Invalid scene group identity");
    name_valid(g.name);
    validate_transform(g.transform);
    if (g.humanoid) g.humanoid->validate();
    auto path=group_path(g.id);
    const bool affine=std::any_of(path.begin(),path.end(),[](const auto&p){return bool(p.source_placement)||bool(p.affine_edit);});
    if(affine){AffineTransform world;for(const auto &parent:path)world=world*group_local_matrix(parent);}
    else {CanvasTransform world{};for(const auto &parent:path)world=ob::compose(world,parent.transform);validate_transform(world);}
  }
  std::set<uint64_t> ids;
  PaintImageArchive source_images;
  CreatorAssetArchive source_models;
  size_t remap_vertices = 0, source_bytes = 0;
  std::set<const std::vector<uint8_t> *> originals;
  size_t count = 0, texels = 0, deformation_entries = 0, paint_bytes = 0, origin_vertices = 0, topology_entries = 0;
  std::map<uint64_t,uint64_t> retopology_sources;
  SourceMaterialArchive source_materials;
  detail::PolygonSceneArchive polygons;
  std::set<const PaintedMesh *> retained_paint;
  std::set<const CreatorSourceModel *> retained_models;
  auto retain_model=[&](const std::shared_ptr<const CreatorSourceModel> &model) {
    if(!model || !model->original_bytes || !model->asset || !model->decoded_images)
      throw std::invalid_argument("Missing original placement source model");
    if(!retained_models.insert(model.get()).second)return;
    if(model->source_receipt)model->source_receipt->validate();
    const size_t stored=model->original_bytes->size()+(model->source_receipt?model->source_receipt->stored_bytes():0);
    if(stored>64*1024*1024)throw std::length_error("Original placement source exceeds native asset budget");
    if(originals.insert(model->original_bytes.get()).second)source_bytes+=stored;
    if(originals.size()>16 || source_bytes>128*1024*1024)throw std::length_error("Original placement sources exceed native model budget");
    if(deep_sources)source_models.add_model(*model,source_images);
    else for(const auto &image:*model->decoded_images)source_images.add(image);
  };
  auto retain_appearance = [&](const CreatorAppearance &a, size_t vertices) {
    if(a.source_vertices.size()!=vertices)throw std::invalid_argument("Creator source does not match its retained mesh");
    remap_vertices+=a.source_vertices.size();
    if(remap_vertices>8'000'000)throw std::length_error("Creator source remap budget exceeded");
    if(!a.original_bytes || !a.asset || !a.decoded_images || a.original_bytes->size()>64*1024*1024)
      throw std::invalid_argument("Missing or oversized creator source");
    if(a.source_receipt)a.source_receipt->validate();
    size_t stored=a.original_bytes->size()+(a.source_receipt?a.source_receipt->stored_bytes():0);
    if(stored>64*1024*1024)throw std::length_error("Original source plus canonical adapter exceeds native asset budget");
    if(originals.insert(a.original_bytes.get()).second)source_bytes+=stored;
    if(originals.size()>16 || source_bytes>128*1024*1024)throw std::length_error("Creator source archive budget exceeded");
    if(deep_sources)source_models.add(a,source_images);
    else for(const auto &image:*a.decoded_images)source_images.add(image);
  };
  auto retain_paint = [&](const std::shared_ptr<const PaintedMesh> &paint,bool origin) {
    if(!paint || (origin && !retained_paint.insert(paint.get()).second))return;
    if(paint->in_stroke())throw std::invalid_argument("Retained UV paint source has an unfinished stroke");
    if(const auto &maps=paint->source_maps()) {
      maps->validate();
      for(const auto *slot:{&maps->color,&maps->metal_rough,&maps->emission,&maps->normal,&maps->occlusion})
        if(slot->image)source_images.add(slot->image);
    }
    texels+=size_t(paint->resolution())*paint->resolution();paint_bytes+=paint->storage_bytes();
    if(paint_bytes>256*1024*1024)throw std::length_error("Scene paint storage budget exceeded");
    if(texels>16*1024*1024)throw std::length_error("Scene paint-map budget exceeded");
  };
  for(const auto &g:groups_)if(g.source_placement)retain_model(g.source_placement->model);
  for (auto &o : objects_) {
    if (!o.id || o.id >= next_ || !ids.insert(o.id).second || !o.volume ||
        o.volume->in_stroke())
      throw std::invalid_argument(
          "Invalid scene object identity or unfinished stroke");
    name_valid(o.name);
    detail::require_polygon_paint_owner(o.painted.get(),o.polygon.get(),o.mesh_active);
    if(o.shape_source)polygons.add_origin(o.shape_source,deep_sources);
    if(o.polygon){polygons.add(o.polygon,deep_sources);if(deep_sources)validate_polygon_derivative(*o.polygon,o.painted.get(),o.mesh_active);}
    validate_transform(o.transform);
    if(has_affine_placement(o.id))(void)world_matrix(o.id);
    else validate_transform(world_transform(o.id));
    o.material.validate();
    if(o.retopology) {
      const auto &patch=*o.retopology;
      if(!patch.source || patch.source>=next_ || patch.source==o.id || !o.painted)
        throw std::invalid_argument("Invalid retopology source or retained display mesh");
      topology_entries+=patch.topology.vertices().size()+patch.topology.edges().size()+patch.topology.faces().size();
      if(topology_entries>1'000'000)throw std::length_error("Scene retopology budget exceeded");
      if(deep_sources)patch.topology.validate();
      retopology_sources.emplace(o.id,patch.source);
    }
    if ((o.mesh_active && !o.painted) || (o.painted && o.painted->in_stroke()))
      throw std::runtime_error(
          "Invalid active mesh or unfinished paint stroke");
    if (o.deformation) {
      if (!o.painted)
        throw std::invalid_argument("Deformation has no retained rest mesh");
      o.deformation->validate(o.painted->mesh());
      deformation_entries += o.deformation->resource_entries();
      if (deformation_entries > 8'000'000)
        throw std::length_error("Scene deformation budget exceeded");
    }
    if(o.appearance) {
      if(!o.painted)throw std::invalid_argument("Creator source has no rest mesh");
      retain_appearance(*o.appearance,o.painted->mesh().positions.size());
    }
    retain_paint(o.painted,false);
    const auto projection=o.painted?o.painted->color_projection():nullptr;
    const auto preparation=o.material_bindings?o.material_bindings->color_preparation:nullptr;
    if(o.polygon && o.polygon->active && o.polygon->colors && (projection || preparation))
      throw std::invalid_argument("Current polygon corner colors conflict with prepared color authority");
    if(bool(projection)!=bool(preparation))throw std::invalid_argument("Current color projection and retained preparation differ");
    if(projection) {
      if(projection->vertex_rgba!=preparation->projection->vertex_rgba || projection->maximum_error!=preparation->projection->maximum_error || projection->quality_cells!=preparation->projection->quality_cells)
        throw std::invalid_argument("Current color projection is not the recorded prepared field");
      projection->validate(o.painted->resolution());source_images.add(projection->vertex_rgba);
      if(deep_sources)preparation->validate_current(o.painted->mesh(),o.appearance.get());
    }
    if(o.material_bindings) {
      if(!o.painted)throw std::invalid_argument("Resolved material has no persisted painting mesh");
      source_materials.add(o.material_bindings,deep_sources);
      if(deep_sources && o.material_bindings->normal_frames)detail::normal_frame_current_authority(*o.material_bindings,o.painted->mesh(),o.deformation.get());
    }
    const auto mask_samples=o.volume->mask_values().size();
    count += std::max(o.volume->voxels().size(), o.volume->stored_samples()-mask_samples)+mask_samples;
    if (count > 4'000'000)
      throw std::length_error("Scene sample limit exceeded");
  }
  for (const auto &v : polygons.volumes()) {
    const auto mask_samples=v->mask_values().size();
    const size_t n = std::max(v->voxels().size(), v->stored_samples()-mask_samples)+mask_samples;
    if (n > 4'000'000 - count)
      throw std::length_error("Scene retained polygon sculpt budget exceeded");
    count += n;
  }
  for (const auto &p : polygons.paints()) {
    retain_paint(p, true);
    if(p->mesh().positions.size()>8'000'000-origin_vertices)throw std::length_error("Retained polygon source geometry exceeds scene budget");
    origin_vertices+=p->mesh().positions.size();
  }
  for (const auto &origin : polygons.origins()) {
    if(origin->placement) {
      origin->placement->validate();
      for(const auto &g:origin->placement->groups)if(g.source_placement)retain_model(g.source_placement->model);
    }
    if (origin->deformation) {
      deformation_entries += origin->deformation->resource_entries();
      if (deformation_entries > 8'000'000)
        throw std::length_error(
            "Scene retained polygon deformation budget exceeded");
    }
    if (origin->appearance)
      retain_appearance(*origin->appearance,
                        origin->paint->mesh().positions.size());
    if (origin->material_bindings)
      source_materials.add(origin->material_bindings, deep_sources);
    if (origin->retopology) {
      const auto &t = origin->retopology->topology;
      topology_entries +=
          t.vertices().size() + t.edges().size() + t.faces().size();
      if (topology_entries > 1'000'000)
        throw std::length_error(
            "Scene retained polygon retopology budget exceeded");
    }
  }
  NormalFrameArchive normal_frames;
  for (const auto &binding : source_materials.bindings())
    if (binding->normal_frames)
      normal_frames.add(binding->normal_frames, deep_sources);
  normal_frames.storage_bytes();
  for(const auto &binding:source_materials.bindings())
    for(const auto &view:binding->roles)if(view.image)source_images.add(view.image);
  for(const auto &origin:source_materials.origins()) {
    const bool fresh_paint=!retained_paint.contains(origin->paint.get());
    retain_paint(origin->paint,true);if(fresh_paint)origin_vertices+=origin->paint->mesh().positions.size();
    if(origin_vertices>8'000'000)throw std::length_error("Retained original UV geometry exceeds scene budget");
    if(origin->deformation){deformation_entries+=origin->deformation->resource_entries();if(deformation_entries>8'000'000)throw std::length_error("Scene deformation budget includes retained original frame authority");}
    if(origin->appearance)retain_appearance(*origin->appearance,origin->paint->mesh().positions.size());
    for(const auto &view:origin->roles)if(view.image)source_images.add(view.image);
  }
  std::set<const SourceColorPreparation *> preparations;
  for(const auto &binding:source_materials.bindings())if(binding->color_preparation && preparations.insert(binding->color_preparation.get()).second) {
    const auto &p=*binding->color_preparation;p.validate(deep_sources);
    source_images.add(p.projection->vertex_rgba);source_images.add(p.initial_projection->vertex_rgba);
    const bool fresh_paint=!retained_paint.contains(p.original_paint.get());
    retain_paint(p.original_paint,true);if(fresh_paint)origin_vertices+=p.original_paint->mesh().positions.size();
    if(origin_vertices>8'000'000)throw std::length_error("Color preparation original geometry exceeds scene budget");
    if(p.original_deformation){deformation_entries+=p.original_deformation->resource_entries();if(deformation_entries>8'000'000)throw std::length_error("Scene deformation budget includes preconversion color authority");}
    if(p.original_appearance)retain_appearance(*p.original_appearance,p.original_paint->mesh().positions.size());
  }
  retopology_acyclic(retopology_sources);
  object(active_);
}
nlohmann::json Scene::encode() const {
  validate();
  bool baked = std::any_of(objects_.begin(), objects_.end(), [](const auto &o) { return o.painted && o.painted->baked(); });
  bool resolved=std::any_of(objects_.begin(),objects_.end(),[](const auto &o){return bool(o.material_bindings) || (!o.mesh_active && bool(o.deformation));});
  bool retopology=std::any_of(objects_.begin(),objects_.end(),[](const auto &o){return bool(o.retopology);});
  const bool framed=std::any_of(objects_.begin(),objects_.end(),[](const auto &o){if(!o.material_bindings)return false;const auto &b=*o.material_bindings;return bool(b.normal_frames) || (b.uv_origin && (b.uv_origin->deformation || !b.uv_origin->creator_kind.empty() || !b.sampling_reparameterized));});
  unsigned feature_version=0;for(const auto&o:objects_)feature_version=std::max({feature_version,detail::minimum_scene_version(o.deformation.get()),detail::minimum_scene_version(o.material_bindings.get()),detail::minimum_scene_version(o.appearance.get())});
  if(std::any_of(groups_.begin(),groups_.end(),[](const auto&g){return bool(g.source_placement)||bool(g.affine_edit);}) ||
      std::any_of(objects_.begin(),objects_.end(),[](const auto&o){return bool(o.affine_edit);}))feature_version=std::max(feature_version,13u);
  for(const auto&o:objects_)feature_version=std::max({feature_version,detail::minimum_scene_version(o.polygon.get()),detail::minimum_scene_version(o.shape_source.get())});
  const bool polygon_fields=feature_version>=12 || std::any_of(objects_.begin(),objects_.end(),[](const auto&o){return bool(o.polygon);});
  const bool extended=resolved || retopology || feature_version>=8;
  nlohmann::json j = {{"format", "dome-sculpt"},
                      {"version", std::max(feature_version,unsigned(framed ? 9 : extended ? 8 : baked ? 6 : 5))},
                      {"active", active_},
                      {"next_id", next_},
                      {"objects", nlohmann::json::array()}};
  PaintImageArchive source_images;
  CreatorAssetArchive source_models;
  SourceMaterialArchive source_materials;
  detail::PolygonSceneArchive polygons;
  detail::SourcePlacementArchive placements;
  for(const auto &o:objects_){if(o.polygon)polygons.add(o.polygon,false);if(o.shape_source)polygons.add_origin(o.shape_source,false);}
  j["next_group_id"] = next_group_;
  j["groups"] = nlohmann::json::array();
  for (auto &g : groups_) {
    auto t = g.transform;
    j["groups"].push_back({{"id", g.id}, {"parent", g.parent}, {"name", g.name},
      {"translation", {t.position.x,t.position.y,t.position.z}},
      {"rotation", {t.orientation.x,t.orientation.y,t.orientation.z,t.orientation.w}},
      {"scale", t.scale}, {"humanoid", g.humanoid ? g.humanoid->encode() : nlohmann::json(nullptr)}});
    if(g.source_placement)j["groups"].back()["source_placement"]=placements.add_node(g.source_placement);
    if(g.affine_edit)j["groups"].back()["affine_edit"]=placements.add_edit(g.affine_edit);
  }
  size_t retopo_bytes=0;
  for (auto &o : objects_) {
    auto t = o.transform;
    auto &m = o.material;
    nlohmann::json patch=nullptr;
    if(o.retopology) {
      auto payload=o.retopology->topology.encode();
      if(payload.size()>64*1024*1024-retopo_bytes)throw std::length_error("Scene retopology payload budget exceeded");
      retopo_bytes+=payload.size();patch={{"source",o.retopology->source},{"topology",std::move(payload)}};
    }
    j["objects"].push_back(
        {{"id", o.id},
         {"group", o.group},
         {"appearance", o.appearance ? encode_creator_appearance(*o.appearance, source_models, source_images) : nlohmann::json(nullptr)},
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
           {"double_sided", m.double_sided},
           {"alpha", int(m.alpha)}, {"alpha_cutoff", m.alpha_cutoff}, {"unlit", m.unlit}}},
         {"volume", o.volume->encode()},
         {"mesh_active", o.mesh_active},
         {"painted", o.painted ? o.painted->encode(&source_images) : nlohmann::json(nullptr)},
         {"deformation", o.deformation
                             ? o.deformation->encode(o.painted->mesh())
                             : nlohmann::json(nullptr)}});
    if(polygon_fields)j["objects"].back()["polygon"]=o.polygon?nlohmann::json(polygons.add(o.polygon)):nlohmann::json(nullptr);
    if(o.affine_edit)j["objects"].back()["affine_edit"]=placements.add_edit(o.affine_edit);
    if(feature_version>=16)j["objects"].back()["shape_source"]=o.shape_source?nlohmann::json(polygons.add_origin(o.shape_source)):nlohmann::json(nullptr);
    if(extended) {
      j["objects"].back()["material_bindings"]=o.material_bindings
        ?nlohmann::json(source_materials.add(o.material_bindings)):nlohmann::json(nullptr);
      j["objects"].back()["retopology"]=std::move(patch);
    }
  }
  if(polygon_fields)j["polygon_sources"]=polygons.encode(source_images,source_models,source_materials,&placements);
  if(extended)j["source_materials"]=source_materials.encode(source_images,source_models);
  if(!placements.empty())j["affine_state"]=placements.encode(source_images,source_models);
  j["source_images"] = source_images.encode();
  j["source_models"] = source_models.encode();
  return j;
}
Scene Scene::decode(const nlohmann::json &j) {
  Scene result;
  if(!j.is_object() || (!j.at("format").is_string() || j.at("format").get_ref<const std::string&>()!="dome-sculpt") || !j.at("version").is_number_integer())
    throw std::runtime_error("Unsupported scene format or version type");
  const auto version=j.at("version").get<int64_t>();
  bool flat_volume=false;
  for(const char *key:{"samples","voxel_size","distance_band","sculpt_layers","active_sculpt_layer","next_sculpt_layer","sculpt_mask","sculpt_controls"})
    flat_volume|=j.contains(key);
  if(flat_volume || version==1) {
    if(j.contains("objects"))throw std::runtime_error("Mixed flat Volume and Scene fields");
    result.active().volume=std::make_shared<Volume>(Volume::decode(j));return result;
  }
  if(version<2 || version>16 || version==15)throw std::runtime_error("Unsupported scene version");
  const auto &objects = j.at("objects");
  if (!objects.is_array() || objects.empty() || objects.size() > 256)
    throw std::length_error("Invalid scene object count");
  bool affine_refs=j.contains("affine_state");
  if(j.contains("groups"))for(const auto &g:j.at("groups"))affine_refs|=g.contains("source_placement")||g.contains("affine_edit");
  for(const auto &o:objects)affine_refs|=o.contains("affine_edit");
  if(j.contains("polygon_sources"))for(const auto &o:j.at("polygon_sources").at("origins"))affine_refs|=o.contains("placement");
  if(affine_refs && version<13)throw std::invalid_argument("Exact affine placement requires Scene13");
  if(affine_refs && !j.contains("affine_state"))throw std::invalid_argument("Missing exact source-placement archive");
  if(j.contains("affine_state")) {
    const auto &state=j.at("affine_state");detail::SourcePlacementArchive::preflight(state,j.at("source_models").size());
    if(j.contains("groups"))for(const auto &g:j.at("groups")) {
      if(g.contains("source_placement"))detail::placement_index(g.at("source_placement"),state.at("nodes").size());
      if(g.contains("affine_edit"))detail::placement_index(g.at("affine_edit"),state.at("edits").size());
    }
    for(const auto &o:objects)if(o.contains("affine_edit"))detail::placement_index(o.at("affine_edit"),state.at("edits").size());
    if(j.contains("polygon_sources"))for(const auto &o:j.at("polygon_sources").at("origins"))if(o.contains("placement"))detail::placement_index(o.at("placement"),state.at("capsules").size());
  }
  for(const auto &o:objects){
    if(version>=16){if(!o.contains("shape_source"))throw std::invalid_argument("Incomplete shape source scene fields");}
    else if(o.contains("shape_source"))throw std::invalid_argument("Shape source requires Scene16");
  }
  if(j.contains("polygon_sources") && j.at("polygon_sources").at("version")==4 && version<16)
    throw std::invalid_argument("Direct shape source archive requires Scene16");
  const bool polygon_fields = version >= 12;
  if (polygon_fields) {
    if (!j.contains("polygon_sources"))
      throw std::invalid_argument("Incomplete polygon source scene fields");
    if(version<13 && j.at("polygon_sources").at("version")!=1)throw std::invalid_argument("Exact polygon placement requires Scene13");
    if(version<14 && j.at("polygon_sources").at("version")==3)
      throw std::invalid_argument("Current polygon corner colors require Scene14");
    detail::PolygonSceneArchive::preflight(j.at("polygon_sources"), objects);
    const auto &material_records=j.at("source_materials").at("bindings");
    const auto &model_records=j.at("source_models");
    if(!material_records.is_array() || material_records.size()>256 || !model_records.is_array() || model_records.size()>16)
      throw std::length_error("Invalid Scene12 shared source archive count");
    auto references=[&](const nlohmann::json &o) {
      if(!o.at("material_bindings").is_null())detail::polygon_archive_index(o.at("material_bindings"),material_records.size());
      if(!o.at("appearance").is_null())detail::polygon_archive_index(o.at("appearance").at("asset"),model_records.size());
    };
    for(const auto &o:objects)references(o);
    for(const auto &o:j.at("polygon_sources").at("origins"))references(o);
    detail::PolygonSceneBudget budget;
    for (const auto &o : objects) {
      budget.volume(o.at("volume"));
      budget.paint(o.at("painted"));
      budget.deformation(o.at("deformation"), unsigned(version));
      budget.appearance(o.at("appearance"));
      budget.retopology(o.at("retopology"));
    }
    const auto &poly = j.at("polygon_sources");
    for (const auto &v : poly.at("volumes"))
      budget.volume(v);
    for (const auto &p : poly.at("paints"))
      budget.paint(p,true);
    for (const auto &o : poly.at("origins")) {
      budget.deformation(o.at("deformation"), unsigned(version));
      budget.appearance(o.at("appearance"));
      budget.retopology(o.at("retopology"));
    }
    const auto &materials = j.at("source_materials");
    for (const auto &p : materials.at("paints"))
      budget.paint(p,true);
    for (const auto &o : materials.at("origins")) {
      if (o.contains("deformation"))
        budget.deformation(o.at("deformation"), unsigned(version));
      budget.appearance(o.at("appearance"));
    }
  } else {
    if (j.contains("polygon_sources"))
      throw std::invalid_argument("Polygon authority requires Scene12");
    for (const auto &o : objects)
      if (o.contains("polygon"))
        throw std::invalid_argument("Polygon authority fields require Scene12");
  }
  // Admit policy context before allocating any decoded SurfaceMap. A table
  // association is only structural permission; final deep validation proves
  // each owner's exact current/retained polygon REST/index/UV derivative.
  for(const auto &o:objects)if(o.contains("painted")&&detail::polygon_paint_policy(o.at("painted"))){
    if(!polygon_fields||!o.contains("polygon")||o.at("polygon").is_null()||o.at("mesh_active")!=true)
      throw std::invalid_argument("Polygon-authored paint requires authenticated Scene12 polygon authority");
    auto index=detail::polygon_archive_index(o.at("polygon"),j.at("polygon_sources").at("authorities").size());
    if(j.at("polygon_sources").at("authorities")[index].at("active")!=true||j.at("polygon_sources").at("authorities")[index].at("topology").at("faces").empty())
      throw std::invalid_argument("Polygon-authored paint has an inactive current authority");
  }
  if(j.contains("source_materials")&&j.at("source_materials").is_object()&&j.at("source_materials").contains("paints"))
    for(const auto &paint:j.at("source_materials").at("paints"))if(detail::polygon_paint_policy(paint))
      throw std::invalid_argument("Source UV origins cannot claim polygon paint authority; explicitly convert their UV source first");
  // Bound the complete serialized sculpt payload before decoding any object.
  // Masks are independent of displacement layers and must share the Scene
  // quota.
  size_t sculpt_entries = 0;
  for (const auto &object : objects) {
    const auto &volume = object.at("volume");
    (void)archive_detail::volume_header(volume);
    const auto &samples = volume.at("samples");
    if (!samples.is_array())
      throw std::runtime_error("Invalid sculpt samples");
    size_t layers = 0;
    if (volume.contains("sculpt_layers")) {
      const auto &records = volume.at("sculpt_layers");
      if (!records.is_array() || records.size() > 16)
        throw std::runtime_error("Invalid sculpt layer count");
      for(const auto &layer:records) {
        const auto &delta=layer.at("delta");
        if(!delta.is_array() || delta.size()>4'000'000-layers) throw std::length_error("Scene sculpt storage limit exceeded");
        layers+=delta.size();
      }
    }
    size_t entries=std::max(samples.size(),layers);
    if(entries>4'000'000) throw std::length_error("Scene sculpt storage limit exceeded");
    if(volume.contains("sculpt_mask")) {
      const auto &values=volume.at("sculpt_mask").at("values");
      if(!values.is_array() || values.size()>4'000'000-entries) throw std::length_error("Scene sculpt mask storage limit exceeded");
      entries+=values.size();
    }
    if(entries>4'000'000-sculpt_entries) throw std::length_error("Scene sculpt storage limit exceeded");
    sculpt_entries+=entries;
  }
  size_t binding_keys=0,retopology_keys=0;
  for(const auto &object:objects) {
    if(!object.is_object())throw std::runtime_error("Invalid scene object record");
    binding_keys+=object.contains("material_bindings");retopology_keys+=object.contains("retopology");
  }
  bool material_fields=false,retopology_fields=false;
  if(version>=7) {
    // Two independent historical writers used version seven. Presence declares
    // a complete feature shape; malformed/partial material data must never be
    // reinterpreted as the other format merely because retopology is present.
    if(version>=8 || j.contains("source_materials") || binding_keys) {
      if(!j.contains("source_materials") || binding_keys!=objects.size())
        throw std::runtime_error("Incomplete source-material scene fields");
      material_fields=true;
    }
    if(version>=8 || retopology_keys) {
      if(retopology_keys!=objects.size())throw std::runtime_error("Incomplete retopology scene fields");
      retopology_fields=true;
    }
    if(!material_fields && !retopology_fields)throw std::runtime_error("Unknown legacy version-seven scene shape");
  } else {
    if(j.contains("source_materials"))throw std::runtime_error("Resolved source materials require scene version seven or later");
    for(const auto &object:objects)
      if((object.contains("material_bindings") && !object.at("material_bindings").is_null()) ||
         (object.contains("retopology") && !object.at("retopology").is_null()))
        throw std::runtime_error("Retained material/topology records require scene version seven or later");
  }
  const bool dormant_deformation=version>=8 || (version==7 && material_fields);
  // The pre-integration baked checkpoint used v5 without group/archive fields.
  bool legacy_baked5 = j.at("version") == 5 && !j.contains("groups") &&
      !j.contains("source_images") && !j.contains("source_models") &&
      std::any_of(j.at("objects").begin(),j.at("objects").end(),[](const auto &o) {
        return !o.at("painted").is_null() && o.at("painted").contains("baked");
      });
  bool archives = j.at("version") >= 5 && !legacy_baked5;
  if(j.at("version")>=7) {
    // Include retained original paint surfaces before any implicit UV raster
    // allocation; a tiny file must not request hundreds of huge SurfaceMaps.
    size_t pixels=0,storage=0;
    auto preflight=[&](const nlohmann::json &paint) {
      if(paint.is_null())return;
      const auto &resolution=paint.at("resolution");
      if(!resolution.is_number_unsigned() && (!resolution.is_number_integer() || resolution.get<int64_t>()<64))
        throw std::runtime_error("Invalid retained painting resolution");
      auto r=resolution.get<uint64_t>();
      if(r<64 || r>2048 || (r&(r-1)))throw std::runtime_error("Invalid retained painting resolution");
      pixels+=size_t(r)*r;
      const auto &layers=paint.at("layers");
      if(!layers.is_array() || layers.empty() || layers.size()>16)throw std::runtime_error("Invalid retained paint layers");
      for(const auto &layer:layers) {
        if(!layer.at("tiles").is_array())throw std::runtime_error("Invalid retained paint tiles");
        storage+=layer.at("tiles").size()*sizeof(PaintTile);
        if(layer.contains("pbr_color_tiles")) {
          if(version<10 || !layer.at("pbr_color_tiles").is_array())throw std::runtime_error("Prepared PBR layers require scene10");
          storage+=layer.at("pbr_color_tiles").size()*sizeof(PaintColorTile);
        }
      }
      if(paint.contains("baked"))for(const auto *key:{"color","orm","emission","normal","coverage"}) {
        const auto &bytes=paint.at("baked").at(key);
        if(!bytes.is_binary())throw std::runtime_error("Invalid retained baked buffer");
        storage+=bytes.get_binary().size();
      }
      if(paint.contains("color_projection") && version<10)throw std::runtime_error("Prepared color projection requires scene10");
      if(paint.contains("baked") && paint.at("baked").at("version")!=1) {
        if(version<10 || paint.at("baked").at("version")!=2 || !paint.at("baked").at("pbr_color").is_binary())throw std::runtime_error("Resolved baked PBR requires scene10");
        storage+=paint.at("baked").at("pbr_color").get_binary().size();
      }
      if(pixels>16*1024*1024 || storage>256*1024*1024)
        throw std::length_error("Scene retained painting allocations exceed budget");
    };
    if(material_fields) {
      const auto &materials=j.at("source_materials");
      if(!materials.is_object() || !materials.at("version").is_number_integer() || (materials.at("version")!=1 && (version<9 || materials.at("version")!=2) && (version<10 || materials.at("version")!=3)) ||
         !materials.at("paints").is_array() || materials.at("paints").size()>256)
        throw std::runtime_error("Invalid retained source-material archive");
      for(const auto &paint:materials.at("paints"))preflight(paint);
    }
    if(!j.at("objects").is_array() || j.at("objects").empty() || j.at("objects").size()>256)
      throw std::runtime_error("Invalid scene object count");
    for(const auto &object:j.at("objects"))preflight(object.at("painted"));
  }
  auto id = [](const nlohmann::json &v) {
    if (!v.is_number_unsigned() &&
        (!v.is_number_integer() || v.get<int64_t>() <= 0))
      throw std::runtime_error("Invalid scene ID");
    auto n = v.get<uint64_t>();
    if (!n)
      throw std::runtime_error("Invalid scene ID");
    return n;
  };
  const auto next_id=id(j.at("next_id"));
  std::set<uint64_t> scene_ids;
  for(const auto &object:objects) {
    const auto object_id=id(object.at("id"));
    if(object_id>=next_id || !scene_ids.insert(object_id).second)throw std::runtime_error("Invalid scene object identity");
  }
  if(!scene_ids.contains(id(j.at("active"))))throw std::runtime_error("Invalid active scene identity");
  if(retopology_fields) {
    size_t payload_bytes=0,entries=0;std::map<uint64_t,uint64_t> references;
    for(const auto &object:objects)if(!object.at("retopology").is_null()) {
      const auto &patch=object.at("retopology");
      if(!patch.is_object() || patch.size()!=2 || !patch.at("topology").is_string())
        throw std::runtime_error("Invalid retopology record");
      if(object.at("painted").is_null())throw std::runtime_error("Retopology has no retained display mesh");
      const auto source=id(patch.at("source")),object_id=id(object.at("id"));
      if(source>=next_id || source==object_id)throw std::runtime_error("Invalid retopology source reference");
      references.emplace(object_id,source);
      const auto &payload=patch.at("topology").get_ref<const std::string &>();
      if(payload.size()>64*1024*1024-payload_bytes)throw std::length_error("Scene retopology payload budget exceeded");
      payload_bytes+=payload.size();
      // Count every patch before allocating any persistent topology maps.
      // The temporary parse is independently byte/depth bounded; RetopoMesh's
      // decoder still validates all IDs, rows and geometry afterwards.
      auto metadata=nlohmann::json::parse(payload,[](int depth,nlohmann::json::parse_event_t,nlohmann::json &) {
        if(depth>32)throw std::length_error("Topology JSON nesting exceeds limit");
        return true;
      });
      if(!metadata.is_object() || metadata.size()!=5 || !metadata.at("version").is_number_integer() || metadata.at("version")!=1)
        throw std::runtime_error("Unsupported topology format");
      for(const auto *field:{"vertices","edges","faces"}) {
        const auto &records=metadata.at(field);const size_t maximum=std::string_view(field)=="edges"?300'000:100'000;
        if(!records.is_array() || records.size()>maximum)throw std::length_error("Topology record budget exceeded");
        entries+=records.size();
        if(entries>1'000'000)throw std::length_error("Scene retopology budget exceeded");
      }
    }
    retopology_acyclic(references);
  }
  PaintImageArchive source_images;
  if (archives) source_images = PaintImageArchive::decode(j.at("source_images"));
  CreatorAssetArchive source_models;
  if (archives) {
    if(version<11)for(const auto &model:j.at("source_models"))if(model.contains("original_source"))
      throw std::runtime_error("Original format source receipts require scene version eleven");
    source_models = CreatorAssetArchive::decode(j.at("source_models"), source_images);
  }
  if(material_fields)for(const auto&origin:j.at("source_materials").at("origins"))if(origin.contains("deformation")&&detail::minimum_deformation_scene_version(origin.at("deformation"))>version)throw std::runtime_error("Retained explicit inverse binds require scene version eleven");
  if(material_fields&&j.at("source_materials").contains("color_preparations"))for(const auto &preparation:j.at("source_materials").at("color_preparations"))
    if(detail::minimum_deformation_scene_version(preparation.at("deformation"))>version)throw std::runtime_error("Preconversion explicit inverse binds require scene version eleven");
  size_t preflight_deformation=material_fields?SourceMaterialArchive::deformation_entries(j.at("source_materials")):0;
  if(version>=4)for(const auto &o:objects)if(!o.at("deformation").is_null()) {
    const auto &d=o.at("deformation");
    if(detail::minimum_deformation_scene_version(d)>version)throw std::runtime_error("Explicit inverse binds require scene version eleven");
    preflight_deformation+=d.at("pose").size()+d.at("morph_weights").size();
    if(!d.at("skin").is_null())for(const auto &v:d.at("skin").at("vertices"))preflight_deformation+=v.size();
    if(!d.at("morphs").is_null())for(const auto &t:d.at("morphs").at("targets"))preflight_deformation+=t.at("delta").size();
    if(preflight_deformation>8'000'000)throw std::length_error("Scene original and current deformation allocations exceed budget");
  }
  SourceMaterialArchive source_materials;
  if(material_fields)source_materials=SourceMaterialArchive::decode(j.at("source_materials"),source_images,source_models);
  detail::SourcePlacementArchive placements;
  if(j.contains("affine_state"))placements=detail::SourcePlacementArchive::decode(j.at("affine_state"),source_models);
  detail::PolygonSceneArchive polygons;
  if(polygon_fields)polygons=detail::PolygonSceneArchive::decode(j.at("polygon_sources"),source_images,source_models,source_materials,&placements);
  size_t remap_vertices = 0;
  result.active_ = id(j.at("active"));
  result.next_ = id(j.at("next_id"));
  auto nonnegative_id = [&](const nlohmann::json &v) {
    if (!v.is_number_unsigned() && (!v.is_number_integer() || v.get<int64_t>() < 0))
      throw std::runtime_error("Invalid group reference");
    return v.get<uint64_t>();
  };
  if (archives) {
    result.next_group_ = id(j.at("next_group_id"));
    const auto &groups = j.at("groups");
    if (!groups.is_array() || groups.size() > 128) throw std::length_error("Invalid group count");
    for (const auto &v : groups) {
      SceneGroup g;
      g.id = id(v.at("id")); g.parent = nonnegative_id(v.at("parent"));
      g.name = v.at("name").get<std::string>();
      auto p = v.at("translation").get<std::array<float,3>>();
      auto q = v.at("rotation").get<std::array<float,4>>();
      g.transform = {{p[0],p[1],p[2]}, {q[0],q[1],q[2],q[3]}, v.at("scale").get<float>()};
      if (!v.at("humanoid").is_null())
        g.humanoid = std::make_shared<const HumanoidControls>(HumanoidControls::decode(v.at("humanoid")));
      if(v.contains("source_placement"))g.source_placement=placements.node(detail::placement_index(v.at("source_placement"),j.at("affine_state").at("nodes").size()));
      if(v.contains("affine_edit"))g.affine_edit=placements.edit(detail::placement_index(v.at("affine_edit"),j.at("affine_state").at("edits").size()));
      result.groups_.push_back(std::move(g));
    }
  }
  result.objects_.clear();
  size_t samples = 0, texels = 0, deformation_entries = material_fields?SourceMaterialArchive::deformation_entries(j.at("source_materials")):0, paint_bytes = 0;
  for (auto &v : objects) {
    SculptObject o;
    o.id = id(v.at("id"));
    if (archives) o.group = nonnegative_id(v.at("group"));
    o.name = v.at("name").get<std::string>();
    o.visible = v.at("visible").get<bool>();
    o.locked = v.at("locked").get<bool>();
    auto p = v.at("translation").get<std::array<float, 3>>();
    auto q = v.at("rotation").get<std::array<float, 4>>();
    o.transform = {{p[0], p[1], p[2]},
                   {q[0], q[1], q[2], q[3]},
                   v.at("scale").get<float>()};
    if(v.contains("affine_edit"))o.affine_edit=placements.edit(detail::placement_index(v.at("affine_edit"),j.at("affine_state").at("edits").size()));
    auto &m = v.at("material");
    o.material = {m.at("name").get<std::string>(),
                  m.at("color").get<std::array<float, 4>>(),
                  m.at("metallic").get<float>(),
                  m.at("roughness").get<float>(),
                  m.at("emissive").get<std::array<float, 3>>(),
                  m.at("toon").get<bool>(),
                  m.at("double_sided").get<bool>()};
    if (archives) {
      auto alpha = m.at("alpha");
      if ((!alpha.is_number_integer() && !alpha.is_number_unsigned()) ||
          alpha.get<int64_t>() < 0 || alpha.get<int64_t>() > 3)
        throw std::runtime_error("Invalid material alpha mode");
      o.material.alpha = Material::AlphaMode(alpha.get<int>());
      o.material.alpha_cutoff = m.at("alpha_cutoff").get<float>();
      o.material.unlit = m.at("unlit").get<bool>();
    }
    samples += v.at("volume").at("samples").size();
    if (samples > 4'000'000)
      throw std::length_error("Scene sample limit exceeded");
    o.volume = std::make_shared<Volume>(Volume::decode(v.at("volume")));
    if (o.volume->stored_samples() > v.at("volume").at("samples").size())
      samples +=
          o.volume->stored_samples() - v.at("volume").at("samples").size();
    if (samples > 4'000'000)
      throw std::length_error("Scene sculpt layer budget exceeded");
    if (j.at("version") >= 3) {
      o.mesh_active = v.at("mesh_active").get<bool>();
      if (!v.at("painted").is_null()) {
        if(version<10) {
          const auto &paint=v.at("painted");
          if(paint.contains("color_projection") || (paint.contains("baked") && paint.at("baked").at("version")!=1))throw std::runtime_error("Prepared paint requires scene10");
          for(const auto &layer:paint.at("layers"))if(layer.contains("pbr_color_tiles"))throw std::runtime_error("Prepared PBR layers require scene10");
        }
        auto &size = v.at("painted").at("resolution");
        if ((!size.is_number_unsigned() &&
             (!size.is_number_integer() || size.get<int64_t>() < 64)) ||
            size.get<uint64_t>() > 2048)
          throw std::runtime_error("Invalid paint resolution");
        auto r = size.get<uint64_t>();
        texels += r * r;
        if (texels > 16 * 1024 * 1024)
          throw std::length_error("Scene paint-map budget exceeded");
        if (v.at("painted").contains("baked") && j.at("version") < 6 && !legacy_baked5)
          throw std::runtime_error("Baked textures require scene version 6");
        o.painted = PaintedMesh::decode(v.at("painted"), archives ? &source_images : nullptr, polygon_fields && !v.at("polygon").is_null() && o.mesh_active);
        paint_bytes += o.painted->storage_bytes();
        if (paint_bytes > 256 * 1024 * 1024) throw std::length_error("Scene paint storage budget exceeded");
      }
    }
    if(retopology_fields && !v.at("retopology").is_null()) {
      const auto &patch=v.at("retopology");
      o.retopology=std::make_shared<const RetopoPatch>(RetopoPatch{
        id(patch.at("source")),RetopoMesh::decode(patch.at("topology").get_ref<const std::string &>())});
    }
    if (j.at("version") >= 4 && !v.at("deformation").is_null()) {
      if (!o.painted || (!o.mesh_active && !dormant_deformation))
        throw std::runtime_error("Dormant deformation requires a retained rest mesh and source-material scene schema");
      const auto &d = v.at("deformation");
      deformation_entries += d.at("pose").size() + d.at("morph_weights").size();
      if (!d.at("skin").is_null())
        for (const auto &row : d.at("skin").at("vertices"))
          deformation_entries += row.size();
      if (!d.at("morphs").is_null())
        for (const auto &target : d.at("morphs").at("targets"))
          deformation_entries += target.at("delta").size();
      if (deformation_entries > 8'000'000)
        throw std::length_error("Scene deformation budget exceeded");
      o.deformation = std::make_shared<const DeformationState>(
          DeformationState::decode(d, o.painted->mesh()));
    }
    if (archives && !v.at("appearance").is_null()) {
      remap_vertices += v.at("appearance").at("source_vertices").get_binary().size() / 4;
      if (remap_vertices > 8'000'000) throw std::length_error("Creator source remap budget exceeded");
      o.appearance = decode_creator_appearance(v.at("appearance"), source_models);
    }
    if(material_fields && !v.at("material_bindings").is_null()) {
      const auto &binding=v.at("material_bindings");
      if((!binding.is_number_unsigned() && (!binding.is_number_integer() || binding.get<int64_t>()<0)) ||
         binding.get<uint64_t>()>UINT32_MAX)throw std::runtime_error("Invalid resolved material identity");
      o.material_bindings=source_materials.at(binding.get<uint32_t>());
    }
    if(polygon_fields && !v.at("polygon").is_null())o.polygon=polygons.at(detail::polygon_archive_index(v.at("polygon"),polygons.authorities().size()));
    if(version>=16 && !v.at("shape_source").is_null())o.shape_source=polygons.origins().at(detail::polygon_archive_index(v.at("shape_source"),polygons.origins().size()));
    result.objects_.push_back(std::move(o));
  }
  placements.require_reachable();
  result.validate();
  return result;
}
void Scene::save(const std::filesystem::path &path) const {
  auto bytes = nlohmann::json::to_cbor(encode());
  if (bytes.size() > 256 * 1024 * 1024)
    throw std::length_error("Scene exceeds file size limit");
  atomic_write(path, bytes);
}
Scene Scene::load(const std::filesystem::path &path) { return load(path, {}); }
Scene Scene::load(const std::filesystem::path &path, const std::function<bool()> &cancelled) {
  constexpr size_t limit = 256 * 1024 * 1024;
  auto check = [&] { if (cancelled && cancelled()) throw std::runtime_error("Project loading cancelled"); };
  check();
  if (std::filesystem::file_size(path) > limit)
    throw std::length_error("Scene exceeds file size limit");
  std::ifstream input(path, std::ios::binary);
  if (!input) throw std::runtime_error("Cannot open scene");
  std::vector<uint8_t> bytes;
  std::array<char, 65536> chunk;
  while (input) {
    check();
    input.read(chunk.data(), chunk.size());
    auto count = size_t(input.gcount());
    if (count > limit - bytes.size()) throw std::length_error("Scene exceeds file size limit");
    bytes.insert(bytes.end(), chunk.data(), chunk.data() + count);
  }
  if (!input.eof()) throw std::runtime_error("Cannot read scene");
  check();
  auto json = nlohmann::json::from_cbor(bytes);
  check();
  auto result = decode(json);
  check();
  return result;
}
std::vector<ObjectSurface>
Scene::surfaces(bool atlas, const std::function<bool()> &cancelled) const {
  require_exportable_sources();
  return surfaces_impl(atlas, false, cancelled);
}
std::vector<ObjectSurface> Scene::preview_surfaces(const std::function<bool()> &cancelled) const {
  return surfaces_impl(false, true, cancelled);
}
std::vector<ObjectSurface> Scene::preview_surfaces(const std::function<bool()> &cancelled, const SculptMesher &mesher) const {
  return surfaces_impl(false, true, cancelled, mesher);
}
std::vector<ObjectSurface> Scene::surfaces_impl(bool atlas, bool preview, const std::function<bool()> &cancelled, const SculptMesher &mesher) const {
  std::vector<ObjectSurface> result;
  for (auto &o : objects_)
    if (o.visible) {
      if(o.polygon && o.polygon->active && o.polygon->topology.faces().empty())continue;
      auto mesh = o.mesh_active ? o.painted->mesh()
                                : mesher ? mesher(o.id, *o.volume) : surface_mesh(*o.volume, cancelled);
      if (mesh.indices.empty())
        continue;
      if (atlas && !o.mesh_active)
        unwrap(mesh, 1024, 4, cancelled);
      result.push_back(
          {o.id, o.name, o.transform, o.material, std::move(mesh),
           o.mesh_active ? (preview ? o.painted->preview(o.material) : o.painted->composite(o.material)) : nullptr,
           o.mesh_active ? o.deformation : nullptr, group_path(o.group),
           o.mesh_active ? o.appearance : nullptr,
           o.mesh_active ? o.material_bindings : nullptr,
           source_is_dormant(o)});
      result.back().affine_edit=o.affine_edit;
      if(o.polygon && o.polygon->active && o.polygon->colors) {
        auto current=std::make_shared<PolygonColorSurface>();
        current->topology=std::shared_ptr<const PolygonMesh>(o.polygon,&o.polygon->topology);
        current->field=o.polygon->colors;
        result.back().polygon_colors=std::move(current);
      }
    }
  return result;
}
} // namespace dome
