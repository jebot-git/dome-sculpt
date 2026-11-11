#include "texture-set-field-source.hpp"
#include "texture-set-field-codec.hpp"
#include "texture-set-source-preflight.hpp"
#include "texture-set-surface.hpp"
#include "dome/texture-sets.hpp"
#include "texture-set-graph.hpp"
#include "texture-set-archive.hpp"
#include "polygon-surface-archive.hpp"
#include "polygon-surface-paint.hpp"
#include "polygon-surface-preview.hpp"
#include "color-bound-memo.hpp"
#include "emission-paint-access.hpp"
#include "native-scene-geometry-intern.hpp"
// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/scene.hpp"
#include "current-paint-preview.hpp"
#include "dome/scene-file.hpp"
#include "deformation-json.hpp"
#include "polygon-paint-authority.hpp"
#include "scene-feature-version.hpp"
#include "native-paint-frame-source.hpp"
#include "baked-normal-field.hpp"
#include "baked-normal-archive.hpp"
#include "canonical-normal-uv.hpp"
#include "canonical-paint-history.hpp"
#include "canonical-paint-archive.hpp"
#include "bake-recipe-remap.hpp"
#include "source-extra-uv-authority.hpp"
#include "creator-source-uv-archive.hpp"
#include "dome/normal-sampling.hpp"
#include "dome/normal-preview.hpp"
#include "dome/paint-sampling.hpp"
#include "dome/color-paint-sampling.hpp"
#include "paint-sampling-access.hpp"
#include "key-paint-adoption.hpp"
#include "material-key-context.hpp"
#include "paint-archive-shape.hpp"
#include "material-archive-version.hpp"
#include "source-paint-pairing.hpp"
#include "source-paint-export.hpp"
#include "polygon-color-target.hpp"
#include "dome/source-placement.hpp"
#include "source-placement-archive.hpp"
#include "dome/import-source.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/source-maps.hpp"
#include "dome/creator-archive.hpp"
#include "dome/material-bindings.hpp"
#include "dome/color-preparation.hpp"
#include "dome/normal-preparation.hpp"
#include "normal-projection-identity.hpp"
#include "normal-frame-runtime.hpp"
#include "dome/retopo-authoring.hpp"
#include "io.hpp"
#include "text-utf8.hpp"
#include "volume-archive.hpp"
#include "polygon-scene-archive.hpp"
#include "polygon-scene-budget.hpp"
#include "source-uv-archive.hpp"
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
std::vector<const nlohmann::json*> texture_holder_paints(const nlohmann::json&holder,const nlohmann::json&geometry){
 if(holder.is_null())return {};
 if(holder.is_object()&&holder.contains("version")&&holder.at("version")==2)return detail::TextureSetArchive::referenced_paints(holder,geometry);
 if(!holder.is_object()||holder.size()!=7||holder.at("version")!=1||!holder.at("additional").is_array()||holder.at("additional").empty()||holder.at("additional").size()>63)throw std::invalid_argument("Invalid legacy texture-set holder");
 std::vector<const nlohmann::json*>result{&holder.at("primary")};for(const auto&field:holder.at("additional"))result.push_back(&field.at("painted"));return result;
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
      !unit(roughness) || !std::all_of(emissive.begin(), emissive.end(), [](float v){return std::isfinite(v)&&v>=0&&v<=maximum_native_emission;}))
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
  return !o.mesh_active && bool(o.appearance || o.material_bindings || o.deformation || o.retopology || o.polygon || o.shape_source || o.rig_source ||
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
  if(texture_set_surface)throw std::logic_error("A texture-set bundle needs its complete per-material geometry consumer");
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
  if (objects_.size() == 1)throw std::logic_error("Keep at least one sculpt object");
  auto next=*this;
  detail::remap_scene_bake_recipes(next,{{id,unavailable_bake_object}},false);
  std::erase_if(next.objects_,[&](const auto &o){return o.id==id;});
  if(next.active_==id)next.active_=next.objects_.front().id;
  *this=std::move(next);
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
  detail::validate_texture_set_graph(*this);
  detail::ColorBoundScope color_bound_scope;
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
  NormalSamplingArchive retained_normal_domains;
  std::set<const PaintedMesh *> retained_paint;
  std::set<const PaintedMesh*> domain_paints,paired_domain_paints;
  std::set<const PaintSamplingDomain*> paint_domains;
  std::set<const Mesh*> paint_bases;size_t paint_domain_bytes=0;
  auto retain_domain=[&](const std::shared_ptr<const PaintSamplingDomain> &domain){
    if(!domain||!paint_domains.insert(domain.get()).second)return;
    if(paint_domains.size()>128)throw std::length_error("Scene writable paint-domain count exceeded");
    auto bytes=domain->source_corners().size()*sizeof(uint32_t);
    if(paint_bases.insert(domain->basis().get()).second)bytes=domain->retained_bytes();
    else if(const auto&geometry=domain->derived_geometry())bytes+=geometry->positions.size()*(2*sizeof(Vec3)+sizeof(std::array<float,2>))+geometry->indices.size()*sizeof(uint32_t)+geometry->uv_pins.size()+geometry->uv_seams.size();
    if(bytes>128*1024*1024-paint_domain_bytes)throw std::length_error("Scene retained writable paint basis exceeds aggregate bytes");paint_domain_bytes+=bytes;
    if(deep_sources)domain->storage_bytes();
  };
  auto pair_domain=[&](const std::shared_ptr<const PaintedMesh> &paint,const std::shared_ptr<const SourceMaterialBindings> &binding,const CreatorAppearance *appearance,const std::shared_ptr<const PaintedMesh>&source_view={}){
    if(!paint||!paint->paint_sampling_domain())return;
    const auto&readable=source_view?source_view:paint;detail::source_paint_pairing(readable.get(),binding.get(),appearance,deep_sources);paired_domain_paints.insert(paint.get());if(source_view)paired_domain_paints.insert(source_view.get());
  };
  NormalSamplingArchiveLimits normal_limits;std::set<const NormalSamplingDomain*> cheap_normal_domains;std::set<const NormalDetailAuthority*> cheap_normal_details;size_t cheap_normal_bytes=0;

  std::set<const CreatorSourceModel *> retained_models;
  std::set<const ImportedAsset *> extra_models;size_t extra_source_bytes=0;
  auto retain_extra_model=[&](const ImportedAsset &asset) {
    if(!extra_models.insert(&asset).second)return;
    const auto bytes=detail::extra_source_uv_storage(asset);
    if(bytes>detail::extra_source_uv_budget-extra_source_bytes)throw std::length_error("Scene original additional UV arrays exceed aggregate storage budget");
    extra_source_bytes+=bytes;
  };
  auto retain_model=[&](const std::shared_ptr<const CreatorSourceModel> &model) {
    if(!model || !model->original_bytes || !model->asset || !model->decoded_images)
      throw std::invalid_argument("Missing original placement source model");
    if(!retained_models.insert(model.get()).second)return;
    retain_extra_model(*model->asset);
    if(model->source_receipt)model->source_receipt->validate();
    const size_t stored=model->original_bytes->size()+(model->source_receipt?model->source_receipt->stored_bytes():0);
    if(stored>64*1024*1024)throw std::length_error("Original placement source exceeds native asset budget");
    if(originals.insert(model->original_bytes.get()).second)source_bytes+=stored;
    if(originals.size()>16 || source_bytes>128*1024*1024)throw std::length_error("Original placement sources exceed native model budget");
    if(deep_sources)source_models.add_model(*model,source_images);
    else for(const auto &image:*model->decoded_images)source_images.add(image);
  };
  auto retain_appearance = [&](const CreatorAppearance &a, const PaintedMesh &paint) {
    const auto vertices=paint.mesh().positions.size();
    if(a.source_vertices.size()!=vertices)throw std::invalid_argument("Creator source does not match its retained mesh");
    remap_vertices+=a.source_vertices.size();
    if(remap_vertices>8'000'000)throw std::length_error("Creator source remap budget exceeded");
    if(!a.original_bytes || !a.asset || !a.decoded_images || a.original_bytes->size()>64*1024*1024)
      throw std::invalid_argument("Missing or oversized creator source");
    retain_extra_model(*a.asset);
    detail::extra_source_uv_pair(paint,a);
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
    if(!paint->valid_paint_sampling_attachment())throw std::logic_error("Paint sampling attachment differs from its exact immutable geometry owner");
    if(!paint->valid_source_sampling_attachment())throw std::invalid_argument("Independent source paint lost its exact original sampling owner");
    if(const auto domain=paint->paint_sampling_domain()){domain_paints.insert(paint.get());retain_domain(domain);}
    if(paint->in_stroke())throw std::invalid_argument("Retained UV paint source has an unfinished stroke");
    if(const auto &maps=paint->source_maps()) {
      maps->validate_surface(paint->mesh());
      for(const auto *slot:{&maps->color,&maps->metal_rough,&maps->emission,&maps->normal,&maps->occlusion})
        if(slot->image)source_images.add(slot->image);
    }
    if(const auto &projection=paint->normal_projection()){
      projection->validate(paint->resolution(),deep_sources);source_images.add(projection->pbr);source_images.add(projection->mtoon);
      if(projection->sampling_domain){const auto &domain=projection->sampling_domain;
        if(deep_sources)retained_normal_domains.add(domain);
        else if(cheap_normal_domains.insert(domain.get()).second){domain->validate(false);auto bytes=domain->storage_bytes()-(domain->detail?domain->detail->coverage.size():0);
          if(domain->detail&&cheap_normal_details.insert(domain->detail.get()).second)bytes+=domain->detail->coverage.size();
          if(cheap_normal_domains.size()>normal_limits.max_domains||cheap_normal_details.size()>normal_limits.max_details||bytes>normal_limits.max_bytes-cheap_normal_bytes)throw std::length_error("Scene retained normal sampling resource budget exceeded");cheap_normal_bytes+=bytes;
        }
        source_images.add(domain->resolved_pbr);source_images.add(domain->resolved_mtoon);
        if(domain->detail){source_images.add(domain->detail->pbr);source_images.add(domain->detail->mtoon);}
      }
    }
    texels+=size_t(paint->resolution())*paint->resolution();paint_bytes+=paint->storage_bytes();
    if(auto original=paint->source_sampling_original())texels+=size_t(original->resolution())*original->resolution();
    if(paint_bytes>256*1024*1024)throw std::length_error("Scene paint storage budget exceeded");
    if(texels>16*1024*1024)throw std::length_error("Scene paint-map budget exceeded");
  };
  auto retain_surface_field=[&](const std::shared_ptr<const PaintedMesh>&paint,
      const std::shared_ptr<const SourceMaterialBindings>&binding,
      const std::shared_ptr<const CreatorAppearance>&appearance,
      const std::shared_ptr<const DeformationState>&deformation,
      const std::shared_ptr<const PolygonAuthority>&polygon,
      const std::shared_ptr<const detail::TextureSetFieldSource>&field_source={}){
    std::shared_ptr<const PaintedMesh>source_view;
    if(field_source){auto field=field_source->field();field.painted=paint;field.material_bindings=binding;field.appearance=appearance;source_view=field_source->read_view(field);retain_paint(field_source->original_paint(),true);pair_domain(field_source->original_paint(),binding,appearance.get());}
    const auto projection=paint?paint->color_projection():nullptr;
    const auto preparation=binding?binding->color_preparation:nullptr;
    detail::polygon_color_owner(polygon.get(),preparation.get(),deep_sources);
    const auto normal_projection=paint?paint->normal_projection():nullptr;
    const auto normal_preparation=binding?binding->normal_preparation:nullptr;
    if(paint&&paint->baked()&&paint->baked()->normal_field)
      detail::source_paint_pairing(source_view?source_view.get():paint.get(),binding.get(),appearance.get(),deep_sources);
    if(bool(normal_projection)!=bool(normal_preparation))throw std::invalid_argument("Current normal projection and retained interpretation differ");
    if(normal_projection){
      if(!detail::normal_projection_identity(*normal_projection,*normal_preparation->projection))throw std::invalid_argument("Prepared normal planes are not their recorded current intrinsic field");
      if(deep_sources)normal_preparation->validate_current(paint->mesh(),appearance.get());
    }
    if(bool(projection)!=bool(preparation))throw std::invalid_argument("Current color projection and retained preparation differ");
    if(projection) {
      if(projection->vertex_rgba!=preparation->projection->vertex_rgba || projection->maximum_error!=preparation->projection->maximum_error || projection->quality_cells!=preparation->projection->quality_cells)
        throw std::invalid_argument("Current color projection is not the recorded prepared field");
      projection->validate(paint->resolution());source_images.add(projection->vertex_rgba);
      if(deep_sources)preparation->validate_current(paint->mesh(),appearance.get());
    }
    if(binding) {
      if(!paint)throw std::invalid_argument("Resolved material has no persisted painting mesh");
      source_materials.add(binding,deep_sources);
      if(deep_sources && binding->normal_frames)detail::normal_frame_current_authority(*binding,paint->mesh(),deformation.get());
    }
    pair_domain(paint,binding,appearance.get(),source_view);
  };
  for(const auto &g:groups_)if(g.source_placement)retain_model(g.source_placement->model);
  for (auto &o : objects_) {
    if (!o.id || o.id >= next_ || !ids.insert(o.id).second || !o.volume ||
        o.volume->in_stroke())
      throw std::invalid_argument(
          "Invalid scene object identity or unfinished stroke");
    name_valid(o.name);
    if(o.texture_sets)o.texture_sets->validate(o,deep_sources);
    detail::validate_polygon_surface_model(o.mesh_active,o.painted.get(),o.polygon.get(),o.material_bindings.get(),o.deformation.get(),o.appearance.get());

    detail::require_polygon_paint_owner(o.painted.get(),o.polygon.get(),o.mesh_active);
    if(o.shape_source)polygons.add_origin(o.shape_source,deep_sources);
    if(o.rig_source)polygons.add_rig_origin(o.rig_source,deep_sources);
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
      retain_appearance(*o.appearance,*o.painted);
    }
    retain_paint(o.painted,false);
    if(o.texture_sets)for(const auto &set:o.texture_sets->additional())retain_paint(set.painted,false);
    retain_surface_field(o.painted,o.material_bindings,o.appearance,o.deformation,o.polygon,o.texture_sets?o.texture_sets->primary_source():nullptr);
    if(o.texture_sets)for(const auto &set:o.texture_sets->additional()){
      if(set.appearance)retain_appearance(*set.appearance,*set.painted);
      retain_surface_field(set.painted,set.material_bindings,set.appearance,o.deformation,o.polygon,set.source);
    }
    const auto mask_samples=o.volume->mask_values().size();
    count += std::max(o.volume->voxels().size(), o.volume->stored_samples()-mask_samples)+mask_samples;
    if (count > 4'000'000)
      throw std::length_error("Scene sample limit exceeded");
  }
  const bool canonical_controls=std::any_of(source_materials.bindings().begin(),source_materials.bindings().end(),[](const auto &b){return bool(b->canonical_paint_sampling)||bool(b->polygon_surface_transfer);})||
    std::any_of(polygons.origins().begin(),polygons.origins().end(),[](const auto &o){return detail::minimum_scene_version(o->material_bindings.get())==37||detail::minimum_scene_version(o->material_bindings.get())==39;});
  if(canonical_controls)detail::collect_canonical_paint_controls(source_materials,polygons,deep_sources);
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
  auto recipe_references=[&](const std::shared_ptr<const ObjectBakeRecipe> &recipe,uint64_t target){
    if(!recipe)return;recipe->validate();
    for(const auto ref:{recipe->source.object,recipe->settings.cage_object})
      if(ref&&ref!=unavailable_bake_object&&(!ids.contains(ref)||(target&&ref==target)))
        throw std::invalid_argument("Bake recipe has a missing or self live reference");
  };
  for(const auto &o:objects_){recipe_references(o.bake_recipe,o.id);if(o.texture_sets)for(const auto &set:o.texture_sets->additional())recipe_references(set.bake_recipe,o.id);}
  for(const auto &o:polygons.origins()){recipe_references(o->bake_recipe,0);if(o->texture_sets)for(const auto &set:o->texture_sets->additional())recipe_references(set.bake_recipe,0);}
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
      retain_appearance(*origin->appearance,*origin->paint);
    if (origin->material_bindings)
      source_materials.add(origin->material_bindings, deep_sources);
    if(origin->texture_sets){retain_paint(origin->paint,true);retain_surface_field(origin->paint,origin->material_bindings,origin->appearance,origin->deformation,origin->polygon,origin->texture_sets->primary_source());}
    else pair_domain(origin->paint,origin->material_bindings,origin->appearance.get());
    if(origin->texture_sets)for(const auto &set:origin->texture_sets->additional()){
      retain_paint(set.painted,true);if(set.appearance)retain_appearance(*set.appearance,*set.painted);
      retain_surface_field(set.painted,set.material_bindings,set.appearance,origin->deformation,origin->polygon,set.source);
    }
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
  std::set<const detail::NativePaintFrameSource*> native_frame_sources;size_t native_frame_metadata=0;
  for(const auto &binding:source_materials.bindings())if(const auto &f=binding->native_paint_frames;f&&native_frame_sources.insert(f.get()).second){
    if(native_frame_sources.size()>128)throw std::length_error("Native frame source count exceeds Scene budget");
    if(f->metadata_bytes()>16*1024*1024-native_frame_metadata)throw std::length_error("Native frame source certificates exceed Scene metadata budget");native_frame_metadata+=f->metadata_bytes();
    f->validate_binding(*binding,deep_sources);normal_frames.add(f->base_frames(),deep_sources);normal_frames.add(f->frames(),deep_sources);
    const bool fresh=!retained_paint.contains(f->before_paint().get());retain_paint(f->before_paint(),true);pair_domain(f->before_paint(),f->before_material(),nullptr);
    if(fresh){const auto n=f->before_paint()->mesh().positions.size();if(n>8'000'000-origin_vertices)throw std::length_error("Native frame capture geometry exceeds aggregate Scene budget");origin_vertices+=n;}
  }
  normal_frames.storage_bytes();
  for(const auto &binding:source_materials.bindings())
    for(const auto &view:binding->roles)if(view.image)source_images.add(view.image);
  std::set<const detail::CanonicalNormalConversion*> canonical_origins;
  for(const auto &binding:source_materials.bindings())if(binding->canonical_attributes&&canonical_origins.insert(binding->canonical_attributes.get()).second){
    const auto &c=*binding->canonical_attributes;
    retain_paint(c.before_paint(),true);source_images.add(c.projection()->pbr);source_images.add(c.projection()->mtoon);
    normal_frames.add(c.frames(),deep_sources);
  }
  std::set<const detail::CanonicalNormalUv*> canonical_uv_owners;size_t canonical_uv_bytes=0,canonical_uv_vertices=0;
  for(const auto &binding:source_materials.bindings())if(binding->canonical_uv&&canonical_uv_owners.insert(binding->canonical_uv.get()).second){
    const auto &uv=*binding->canonical_uv;
    if(uv.storage_bytes()>128*1024*1024-canonical_uv_bytes||uv.mesh()->positions.size()>8'000'000-canonical_uv_vertices)
      throw std::length_error("Retained canonical UV geometry exceeds aggregate Scene budget");
    canonical_uv_bytes+=uv.storage_bytes();canonical_uv_vertices+=uv.mesh()->positions.size();
  }
  for(const auto &origin:source_materials.origins()) {
    const bool fresh_paint=!retained_paint.contains(origin->paint.get());
    retain_paint(origin->paint,true);if(fresh_paint)origin_vertices+=origin->paint->mesh().positions.size();
    if(origin_vertices>8'000'000)throw std::length_error("Retained original UV geometry exceeds scene budget");
    if(origin->deformation){deformation_entries+=origin->deformation->resource_entries();if(deformation_entries>8'000'000)throw std::length_error("Scene deformation budget includes retained original frame authority");}
    if(origin->appearance)retain_appearance(*origin->appearance,*origin->paint);
    for(const auto &view:origin->roles)if(view.image)source_images.add(view.image);
  }
  std::set<const SourceColorPreparation *> preparations;
  std::set<const PolygonColorSurface*> color_sources;
  size_t color_uv_layout_bytes=0;
  size_t polygon_entries=polygons.component_entries();auto polygon_colors=polygons.color_archive();
  auto add_polygon=[&](size_t n){if(n>8'000'000-polygon_entries)throw std::length_error("Scene current color geometry/provenance exceeds polygon budget");polygon_entries+=n;};
  for(const auto &binding:source_materials.bindings())if(binding->color_preparation && preparations.insert(binding->color_preparation.get()).second) {
    const auto &p=*binding->color_preparation;p.validate(deep_sources);
    source_images.add(p.projection->vertex_rgba);source_images.add(p.initial_projection->vertex_rgba);
    for(const auto &descriptor:{p.polygon_colors,p.current_descriptor(),p.sampling_descriptor()})if(descriptor&&color_sources.insert(descriptor.get()).second){
      const auto &topology=*descriptor->topology;
      add_polygon(topology.vertices().size());add_polygon(topology.edges().size());add_polygon(topology.faces().size());
      for(const auto &v:topology.vertices())add_polygon(v.provenance.size());for(const auto &f:topology.faces())add_polygon(f.corners.size());
      auto before=polygon_colors.entries();polygon_colors.add(descriptor->field);add_polygon(polygon_colors.entries()-before);
    }
    const bool fresh_paint=!retained_paint.contains(p.original_paint.get());
    retain_paint(p.original_paint,true);pair_domain(p.original_paint,p.original_material,p.original_appearance.get());if(fresh_paint)origin_vertices+=p.original_paint->mesh().positions.size();
    if(origin_vertices>8'000'000)throw std::length_error("Color preparation original geometry exceeds scene budget");
    if(p.original_deformation){deformation_entries+=p.original_deformation->resource_entries();if(deformation_entries>8'000'000)throw std::length_error("Scene deformation budget includes preconversion color authority");}
    if(p.original_appearance)retain_appearance(*p.original_appearance,*p.original_paint);
    if(p.uv_reprojection){const auto &u=*p.uv_reprojection;const bool fresh=!retained_paint.contains(u.before_paint.get());retain_paint(u.before_paint,true);pair_domain(u.before_paint,u.before_material,u.before_appearance.get());if(fresh)origin_vertices+=u.before_paint->mesh().positions.size();
      if(origin_vertices>8'000'000)throw std::length_error("Colour UV retained geometry exceeds scene budget");
      if(u.before_deformation){deformation_entries+=u.before_deformation->resource_entries();if(deformation_entries>8'000'000)throw std::length_error("Colour UV retained deformation exceeds scene budget");}
      if(u.before_appearance)retain_appearance(*u.before_appearance,*u.before_paint);
      source_images.add(u.projection->vertex_rgba);const auto bytes=u.layout->storage_bytes();if(bytes>128*1024*1024-color_uv_layout_bytes)throw std::length_error("Colour UV layouts exceed scene budget");color_uv_layout_bytes+=bytes;
    }
  }
  for(const auto &binding:source_materials.bindings())if(binding->color_preparation&&binding->color_preparation->paint_sampling){const auto &u=*binding->color_preparation->paint_sampling;
    retain_domain(u.domain());const bool fresh=!retained_paint.contains(u.before_paint().get());retain_paint(u.before_paint(),true);pair_domain(u.before_paint(),u.before_material(),u.before_appearance().get());if(fresh)origin_vertices+=u.before_paint()->mesh().positions.size();
    if(origin_vertices>8'000'000)throw std::length_error("Retained writable paint original geometry exceeds scene budget");
    if(u.before_deformation()){deformation_entries+=u.before_deformation()->resource_entries();if(deformation_entries>8'000'000)throw std::length_error("Retained writable paint deformation exceeds scene budget");}
    if(u.before_appearance())retain_appearance(*u.before_appearance(),*u.before_paint());
    const auto bytes=u.layout()->storage_bytes();if(bytes>128*1024*1024-color_uv_layout_bytes)throw std::length_error("Retained writable paint layouts exceed scene budget");color_uv_layout_bytes+=bytes;
  }
  std::set<const detail::NativePaintSampling*> native_paints;size_t native_layout_bytes=0;
  for(const auto &binding:source_materials.bindings())if(binding->native_paint_sampling&&native_paints.insert(binding->native_paint_sampling.get()).second){
    const auto &p=*binding->native_paint_sampling;if(deep_sources)p.validate();
    const bool fresh=!retained_paint.contains(p.before_paint().get());retain_paint(p.before_paint(),true);pair_domain(p.before_paint(),p.before_material(),nullptr);retain_domain(p.domain());
    if(fresh){const auto count=p.before_paint()->mesh().positions.size();if(count>8'000'000-origin_vertices)throw std::length_error("Native paint history geometry exceeds scene budget");origin_vertices+=count;}
    const auto bytes=p.layout()->storage_bytes();if(bytes>128*1024*1024-native_layout_bytes)throw std::length_error("Native paint layouts exceed scene budget");native_layout_bytes+=bytes;
  }
  std::set<const SourceNormalPreparation*> normal_preparations;
  std::set<const SourceNormalUvLayout*> normal_uv_layouts;size_t normal_uv_layout_bytes=0;
  for(const auto &binding:source_materials.bindings())if(binding->normal_preparation&&normal_preparations.insert(binding->normal_preparation.get()).second){
    const auto &p=*binding->normal_preparation;p.validate(deep_sources);
    for(const auto &projection:{p.projection,p.initial_projection}){source_images.add(projection->pbr);source_images.add(projection->mtoon);}
    const bool fresh=!retained_paint.contains(p.original_paint.get());retain_paint(p.original_paint,true);pair_domain(p.original_paint,p.original_material,p.original_appearance.get());if(fresh)origin_vertices+=p.original_paint->mesh().positions.size();
    if(origin_vertices>8'000'000)throw std::length_error("Normal preparation original geometry exceeds scene budget");
    if(p.original_deformation){deformation_entries+=p.original_deformation->resource_entries();if(deformation_entries>8'000'000)throw std::length_error("Scene deformation budget includes pre-normal-preparation authority");}
    if(p.original_appearance)retain_appearance(*p.original_appearance,*p.original_paint);
    auto retain_normal_before=[&](const auto &u){const bool fresh_uv=!retained_paint.contains(u.before_paint.get());retain_paint(u.before_paint,true);pair_domain(u.before_paint,u.before_material,u.before_appearance.get());if(fresh_uv)origin_vertices+=u.before_paint->mesh().positions.size();if(origin_vertices>8'000'000)throw std::length_error("Normal UV retained geometry exceeds scene budget");if(u.before_deformation){deformation_entries+=u.before_deformation->resource_entries();if(deformation_entries>8'000'000)throw std::length_error("Normal UV retained deformation exceeds scene budget");}if(u.before_appearance)retain_appearance(*u.before_appearance,*u.before_paint);if(normal_uv_layouts.insert(u.layout.get()).second){auto bytes=u.layout->storage_bytes();if(bytes>128*1024*1024-normal_uv_layout_bytes)throw std::length_error("Normal UV layouts exceed scene budget");normal_uv_layout_bytes+=bytes;}};
    if(p.uv_reprojection)retain_normal_before(*p.uv_reprojection);
    if(p.sampling_domain){retain_normal_before(*p.sampling_domain);retain_domain(p.sampling_domain->metadata_sampling_domain);}
  }
  std::set<const DeformationState*> before_deformations;
  for(const auto &o:objects_)if(o.deformation)before_deformations.insert(o.deformation.get());
  for(const auto &o:polygons.origins())if(o->deformation)before_deformations.insert(o->deformation.get());
  for(const auto &o:source_materials.origins())if(o->deformation)before_deformations.insert(o->deformation.get());
  for(const auto &b:source_materials.bindings()){
    if(b->color_preparation){const auto &p=*b->color_preparation;if(p.original_deformation)before_deformations.insert(p.original_deformation.get());if(p.uv_reprojection&&p.uv_reprojection->before_deformation)before_deformations.insert(p.uv_reprojection->before_deformation.get());if(p.paint_sampling&&p.paint_sampling->before_deformation())before_deformations.insert(p.paint_sampling->before_deformation().get());}
    if(b->normal_preparation){const auto &p=*b->normal_preparation;if(p.original_deformation)before_deformations.insert(p.original_deformation.get());if(p.uv_reprojection&&p.uv_reprojection->before_deformation)before_deformations.insert(p.uv_reprojection->before_deformation.get());if(p.sampling_domain&&p.sampling_domain->before_deformation)before_deformations.insert(p.sampling_domain->before_deformation.get());}
  }
  std::set<const detail::SourceKeyDerivation*> keys;
  for(const auto &binding:source_materials.bindings())if(binding->key_derivation&&keys.insert(binding->key_derivation.get()).second){const auto &k=*binding->key_derivation;k.validate(deep_sources);
    const bool fresh=!retained_paint.contains(k.before_paint().get());retain_paint(k.before_paint(),true);pair_domain(k.before_paint(),k.before_material(),k.before_appearance().get());
    if(fresh){if(k.before_paint()->mesh().positions.size()>8'000'000-origin_vertices)throw std::length_error("Key before geometry exceeds scene budget");origin_vertices+=k.before_paint()->mesh().positions.size();}
    if(k.before_deformation()&&before_deformations.insert(k.before_deformation().get()).second){const auto n=k.before_deformation()->resource_entries();if(n>8'000'000-deformation_entries)throw std::length_error("Key before deformation exceeds scene budget");deformation_entries+=n;}
    if(k.before_appearance())retain_appearance(*k.before_appearance(),*k.before_paint());
  }
  for(const auto *paint:domain_paints)if(!paired_domain_paints.contains(paint))throw std::invalid_argument("Retained writable paint lacks its own exact paired field/normal material authority");
  retopology_acyclic(retopology_sources);
  object(active_);
}
nlohmann::json Scene::encode() const {
  detail::ColorBoundScope color_bound_scope;
  validate();
  bool baked = std::any_of(objects_.begin(), objects_.end(), [](const auto &o) { return o.painted && o.painted->baked(); });
  bool resolved=std::any_of(objects_.begin(),objects_.end(),[](const auto &o){return bool(o.material_bindings) || (!o.mesh_active && bool(o.deformation));});
  bool retopology=std::any_of(objects_.begin(),objects_.end(),[](const auto &o){return bool(o.retopology);});
  const bool framed=std::any_of(objects_.begin(),objects_.end(),[](const auto &o){if(!o.material_bindings)return false;const auto &b=*o.material_bindings;return bool(b.normal_frames) || (b.uv_origin && (b.uv_origin->deformation || !b.uv_origin->creator_kind.empty() || !b.sampling_reparameterized));});
  unsigned feature_version=detail::minimum_scene_version(*this);
  const auto set_graph=detail::validate_texture_set_graph(*this);
  const bool texture_sets=set_graph.collections!=0;
  if(texture_sets)feature_version=std::max(feature_version,40u);
  if(std::any_of(groups_.begin(),groups_.end(),[](const auto&g){return bool(g.source_placement)||bool(g.affine_edit);}) ||
      std::any_of(objects_.begin(),objects_.end(),[](const auto&o){return bool(o.affine_edit);}))feature_version=std::max(feature_version,13u);
  for(const auto&o:objects_)feature_version=std::max({feature_version,detail::minimum_scene_version(o.polygon.get()),detail::minimum_scene_version(o.shape_source.get()),o.rig_source?25u:0u,detail::minimum_scene_version(o.rig_source.get())});
  for(const auto &g:groups_)if(g.source_placement)feature_version=std::max(feature_version,detail::minimum_scene_version(g.source_placement->model.get()));
  const bool polygon_fields=feature_version>=12 || std::any_of(objects_.begin(),objects_.end(),[](const auto&o){return bool(o.polygon);});
  const bool extended=resolved || retopology || feature_version>=8;
  nlohmann::json j = {{"format", "dome-sculpt"},
                      {"version", std::max(feature_version,unsigned(framed ? 9 : extended ? 8 : baked ? 6 : 5))},
                      {"active", active_},
                      {"next_id", next_},
                      {"objects", nlohmann::json::array()}};
  PaintImageArchive source_images;
  detail::BakedNormalArchive layered_normals(source_images);
  CreatorAssetArchive source_models;
  SourceMaterialArchive source_materials;
  detail::PolygonSceneArchive polygons;
  detail::TextureSetArchive texture_archive;
  NormalSamplingArchive normal_domains;PaintSamplingArchive writable_domains;
  detail::SourcePlacementArchive placements;
  for(const auto &o:objects_){if(o.polygon)polygons.add(o.polygon,false);if(o.shape_source)polygons.add_origin(o.shape_source,false);if(o.rig_source)polygons.add_rig_origin(o.rig_source,false);}
  std::unique_ptr<detail::CanonicalPaintArchive> canonical_paints;
  std::unique_ptr<detail::PolygonSurfaceArchive> polygon_surfaces;
  // Scene40 composes its genuine set archive with independently owned fields.
  // The outer version does not manufacture a polygon/canonical capability.
  if(feature_version==37||feature_version==39||feature_version==40){
    for(const auto&o:objects_){if(o.material_bindings)source_materials.add(o.material_bindings);if(o.texture_sets)for(const auto&field:o.texture_sets->additional())if(field.material_bindings)source_materials.add(field.material_bindings);}
    for(size_t pass=0;pass<512;++pass){const auto old_origins=polygons.origins().size(),old_bindings=source_materials.bindings().size();detail::collect_canonical_paint_controls(source_materials,polygons,true);for(size_t i=0;i<polygons.origins().size();++i){const auto origin=polygons.origins()[i];if(origin->material_bindings)source_materials.add(origin->material_bindings);if(origin->texture_sets)for(const auto&field:origin->texture_sets->additional())if(field.material_bindings)source_materials.add(field.material_bindings);}if(old_origins==polygons.origins().size()&&old_bindings==source_materials.bindings().size())break;if(pass==511)throw std::length_error("Retained texture material/source closure exceeds bounded traversal");}
    detail::collect_canonical_paint_controls(source_materials,polygons,true);
    const bool needs_polygon=std::any_of(source_materials.bindings().begin(),source_materials.bindings().end(),[](const auto&b){return bool(b->polygon_surface_transfer);});
    if(needs_polygon){polygon_surfaces=std::make_unique<detail::PolygonSurfaceArchive>(polygons);for(const auto&b:source_materials.bindings())if(b->polygon_surface_transfer)polygon_surfaces->add(b);}
    if(feature_version==39&&(!polygon_surfaces||!polygon_surfaces->size()))throw std::invalid_argument("Scene39 writer lacks actual current field ownership");
    const bool needs_canonical=std::any_of(source_materials.bindings().begin(),source_materials.bindings().end(),[](const auto&b){return bool(b->canonical_paint_sampling);});
    if(needs_canonical){canonical_paints=std::make_unique<detail::CanonicalPaintArchive>(polygons,layered_normals,writable_domains);for(const auto&b:source_materials.bindings())if(b->canonical_paint_sampling)canonical_paints->add(b->canonical_paint_sampling);}
    if(feature_version==37&&!canonical_paints)throw std::invalid_argument("Scene37 writer lacks actual canonical paint ownership");
  }
  const auto sourceful=[](const auto&collection){if(!collection)return false;if(collection->primary_source())return true;return std::any_of(collection->additional().begin(),collection->additional().end(),[](const auto&field){return bool(field.source);});};
  const bool shared_sets=texture_sets&&(std::any_of(objects_.begin(),objects_.end(),[&](const auto&o){return sourceful(o.texture_sets);})||std::any_of(polygons.origins().begin(),polygons.origins().end(),[](const auto&o){return bool(o->texture_sets);}));
  std::unique_ptr<detail::TextureSetFieldCodec>field_codec;
  if(shared_sets){texture_archive.shared_records();field_codec=std::make_unique<detail::TextureSetFieldCodec>();texture_archive.field_encoder([&](const TextureSetView&field,uint32_t geometry){return field_codec->encode(field,geometry,source_images,&normal_domains,&writable_domains,[&](auto binding){return source_materials.add(std::move(binding));});},[&]{return field_codec->records();});}
  // Keep old UV-only Scene26 records free of fabricated rig-history fields.
  // Scene25 retains its original nullable layout; real nested history owns pool6.
  const bool rig_fields=feature_version==25 ||
    std::any_of(objects_.begin(),objects_.end(),[](const auto &o){return bool(o.rig_source);}) ||
    std::any_of(polygons.origins().begin(),polygons.origins().end(),[](const auto &o){return bool(o->rig_source)||bool(o->bake_recipe);});
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
         {"painted", o.painted&&!o.texture_sets ? o.painted->encode(&source_images,feature_version>=21?&normal_domains:nullptr,feature_version>=24?&writable_domains:nullptr) : nlohmann::json(nullptr)},
         {"deformation", o.deformation
                             ? o.deformation->encode(o.painted->mesh())
                             : nlohmann::json(nullptr)}});
    if(texture_sets)j["objects"].back()["texture_sets"]=o.texture_sets?texture_archive.encode(o,source_images,&normal_domains,&writable_domains):nlohmann::json(nullptr);
    if(polygon_fields)j["objects"].back()["polygon"]=o.polygon?nlohmann::json(polygons.add(o.polygon)):nlohmann::json(nullptr);
    if(o.affine_edit)j["objects"].back()["affine_edit"]=placements.add_edit(o.affine_edit);
    if(feature_version>=30)j["objects"].back()["bake_recipe"]=o.bake_recipe?o.bake_recipe->encode():nlohmann::json(nullptr);
    if(feature_version>=16)j["objects"].back()["shape_source"]=o.shape_source?nlohmann::json(polygons.add_origin(o.shape_source)):nlohmann::json(nullptr);
    if(rig_fields)j["objects"].back()["rig_source"]=o.rig_source?nlohmann::json(polygons.add_rig_origin(o.rig_source)):nlohmann::json(nullptr);
    if(extended) {
      j["objects"].back()["material_bindings"]=o.material_bindings
        ?nlohmann::json(source_materials.add(o.material_bindings)):nlohmann::json(nullptr);
      j["objects"].back()["retopology"]=std::move(patch);
    }
  }
  detail::PolygonColorArchive color_archive;
  bool corner_prepared=feature_version>=20&&feature_version<25;
  if(feature_version>=25){
    // New independent source UVs do not imply a corner-color preparation.
    // Historical Scene20/21/23/24 writer shapes stay unchanged.
    for(const auto &origin:polygons.origins())if(origin->material_bindings)source_materials.add(origin->material_bindings);
    corner_prepared=std::any_of(source_materials.bindings().begin(),source_materials.bindings().end(),[](const auto &binding){return binding->color_preparation&&binding->color_preparation->polygon_colors;});
  }
  if(polygon_fields)j["polygon_sources"]=polygons.encode(source_images,source_models,source_materials,&placements,corner_prepared?&color_archive:nullptr,feature_version>=21?&normal_domains:nullptr,feature_version>=24?&writable_domains:nullptr,texture_sets?&texture_archive:nullptr);
  if(extended)j["source_materials"]=source_materials.encode(source_images,source_models,corner_prepared?&color_archive:nullptr,feature_version>=21?&normal_domains:nullptr,feature_version>=24?&writable_domains:nullptr);
  if(const auto composed=detail::composed_material_scene_version(source_materials))j["version"]=std::max(j["version"].get<unsigned>(),composed);
  if(texture_sets){
    auto geometry=texture_archive.geometry_records();
    if(geometry.at("topologies").empty())throw std::invalid_argument("Retained texture-set history needs its polygon8 owning archive adapter");
    j["texture_set_geometry"]=std::move(geometry);
  }
  if(canonical_paints)j["canonical_paint"]=canonical_paints->encode();
  if(polygon_surfaces)j["polygon_surfaces"]=polygon_surfaces->encode();
  if(corner_prepared){auto records=color_archive.encode();j["polygon_sources"]["color_bases"]=std::move(records["color_bases"]);j["polygon_sources"]["color_fields"]=std::move(records["color_fields"]);}
  if(!placements.empty())j["affine_state"]=placements.encode(source_images,source_models);
  if(normal_domains.size())j["normal_sampling"]=normal_domains.encode(source_images);
  if(writable_domains.size())j["paint_sampling"]=writable_domains.encode();
  if(!layered_normals.empty())j["baked_normal_fields"]=layered_normals.encode();
  j["source_images"] = source_images.encode();
  j["source_models"] = source_models.encode();
  return j;
}
Scene Scene::decode(const nlohmann::json &j) {
  Scene result;
  if(!j.is_object() || (!j.at("format").is_string() || j.at("format").get_ref<const std::string&>()!="dome-sculpt") || !j.at("version").is_number_integer())
    throw std::runtime_error("Unsupported scene format or version type");
  const auto version=j.at("version").get<int64_t>();
  const bool normal_context=j.contains("normal_sampling"),paint_context=j.contains("paint_sampling");
  const bool polygon_materials=j.contains("source_materials")&&detail::polygon_surface_material_archive(j.at("source_materials"));
  const bool polygon_surface_context=(version==39||version==40)&&polygon_materials&&j.contains("polygon_surfaces");
  if((version==39||polygon_materials||j.contains("polygon_surfaces"))&&!polygon_surface_context)
    throw std::invalid_argument("Polygon field scene requires genuine material21/current field ownership");
  std::unique_ptr<detail::PolygonSurfacePreflight> polygon_preflight;
  if(polygon_surface_context)polygon_preflight=std::make_unique<detail::PolygonSurfacePreflight>(j);
  const bool canonical_materials=j.contains("source_materials")&&detail::canonical_paint_material_archive(j.at("source_materials"));
  const bool canonical_paint_context=(version==37||version==40||polygon_surface_context)&&paint_context&&j.contains("canonical_paint")&&canonical_materials;
  if((version==37||canonical_materials||j.contains("canonical_paint"))&&!canonical_paint_context)
    throw std::invalid_argument("Canonical paint scene requires genuine paint ownership and its complete pools");
  const bool native_paint_context=(version==32||version==35||version==36||version==37||version==40||polygon_surface_context)&&paint_context&&j.contains("source_materials")&&detail::native_paint_material_archive(j.at("source_materials"));
  if((version==32||version==35||version==36)&&!native_paint_context)throw std::invalid_argument("Scene32 requires its actual native paint owning capability");
  if(version!=32&&version!=40&&j.contains("source_materials")&&j.at("source_materials").at("version")==14)throw std::invalid_argument("Native paint material14 requires Scene32");
  if(version!=40&&(version==36)!=(j.contains("source_materials")&&j.at("source_materials").at("version")==18))throw std::invalid_argument("Scene36 requires exactly material18 native frame ownership");
  if(version!=40&&(version==35)!=(j.contains("source_materials")&&j.at("source_materials").at("version")==17))
    throw std::invalid_argument("Scene35 requires exactly the composed material17 owning context");
  const bool explicit_mirror_context=version>=29&&j.contains("source_materials")&&detail::explicit_mirror_material_archive(j.at("source_materials"))&&detail::encoded_key_context(j.at("source_materials"));
  if(version==29&&!explicit_mirror_context)throw std::invalid_argument("Scene29 requires its actual owning explicit Mirror capability");
  const SourcePaintDecodeContext source_paint_context{version>=28,version>=27};
  bool flat_volume=false;
  for(const char *key:{"samples","voxel_size","distance_band","sculpt_layers","active_sculpt_layer","next_sculpt_layer","sculpt_mask","sculpt_controls"})
    flat_volume|=j.contains(key);
  if(version>=30&&flat_volume)throw std::invalid_argument("Owning Scene30+ cannot be a flat Volume");
  if(flat_volume || version==1) {
    if(j.contains("objects"))throw std::runtime_error("Mixed flat Volume and Scene fields");
    result.active().volume=std::make_shared<Volume>(Volume::decode(j));return result;
  }
  if(version<2 || (version>21&&version!=22&&version!=23&&version!=24&&version!=25&&version!=26&&version!=27&&version!=28&&version!=29&&version!=30&&version!=31&&version!=32&&version!=33&&version!=34&&version!=35&&version!=36&&version!=37&&version!=39&&version!=40))throw std::runtime_error("Unsupported scene version");
  size_t layered_normal_bytes=0;
  const bool layered_context=version==33||version==34||
    ((version==35||version==36||version==37||version==40||polygon_surface_context)&&detail::layered_material_archive(j.at("source_materials")));
  if(layered_context){
    if(!j.contains("baked_normal_fields")||!j.contains("source_materials")||!detail::layered_material_archive(j.at("source_materials")))
      throw std::invalid_argument("Scene33 requires its complete material15/normal-field ownership graph");
    if(version!=35&&version!=36&&version!=37&&version!=40&&!polygon_surface_context&&(version==34)!=detail::canonical_uv_material_archive(j.at("source_materials")))throw std::invalid_argument("Scene34 requires actual material16 UV capability");
    layered_normal_bytes=detail::BakedNormalArchive::preflight(j.at("baked_normal_fields"),j.at("source_materials"));
    if(canonical_paint_context)detail::CanonicalPaintArchive::preflight(j.at("canonical_paint"),j.at("polygon_sources"),j.at("baked_normal_fields"),j.at("paint_sampling"),j.at("source_materials"),source_paint_context);
  }else if(j.contains("baked_normal_fields")||(j.contains("source_materials")&&detail::layered_material_archive(j.at("source_materials"))))
    throw std::invalid_argument("Independent baked normal ownership requires actual Scene33");
  if(j.contains("source_materials")&&detail::canonical_material_archive(j.at("source_materials"))&&version<31)
    throw std::invalid_argument("Canonical material13 requires actual Scene31 authority");
  if(version==31&&(!j.contains("source_materials")||!detail::canonical_material_archive(j.at("source_materials"))))
    throw std::invalid_argument("Scene31 requires actual canonical conversion authority");
  if(version<27&&j.contains("source_materials")&&j.at("source_materials").at("version")==11)
    throw std::invalid_argument("Physical material11 requires Scene27");
  if(version<29&&j.contains("source_materials")&&j.at("source_materials").at("version")==12)throw std::invalid_argument("Explicit Mirror material12 requires Scene29");
  if(version<30&&j.contains("polygon_sources")&&j.at("polygon_sources").at("version")==7)
    throw std::invalid_argument("Recipe history archive7 requires Scene30");
  for(const auto &object:j.at("objects")){
    if(version<30&&object.contains("bake_recipe"))throw std::invalid_argument("Bake recipes require Scene30");
    if(version>=30&&!object.contains("bake_recipe"))throw std::invalid_argument("Incomplete Scene30+ recipe fields");
  }
  const bool key_context=j.contains("source_materials")&&detail::encoded_key_context(j.at("source_materials"));
  auto key_feature=[&](const char *table,const char *field){return key_context&&detail::encoded_key_feature(j.at("source_materials"),table,field);};
  if(version==22&&(!key_context||!j.at("source_materials").at("key_derivations").is_array()||j.at("source_materials").at("key_derivations").empty()))throw std::invalid_argument("Scene22 requires actual owning native key derivation");
  if(j.contains("source_materials")&&detail::material_archive_base_version(j.at("source_materials"))==10&&version<22)throw std::invalid_argument("Owning native key derivation requires Scene22");
  const bool rig_pool=j.contains("polygon_sources")&&(j.at("polygon_sources").at("version")==6||j.at("polygon_sources").at("version")==7||j.at("polygon_sources").at("version")==8);
  const bool shared_corner_context=j.contains("polygon_sources")&&(j.at("polygon_sources").at("version")==5||(rig_pool&&j.at("polygon_sources").at("shared_colors").get<bool>()));
  if(j.contains("polygon_sources")&&j.at("polygon_sources").at("version")==8&&(version!=40||!j.contains("texture_set_geometry")||j.at("texture_set_geometry").at("version")!=2))throw std::invalid_argument("Retained texture history8 requires actual Scene40/shared geometry2");
  if(version<25&&rig_pool)throw std::invalid_argument("Rig source archive requires Scene25");
  if(paint_context&&version<24)throw std::invalid_argument("Independent writable paint requires explicit Scene24");
  if(version==24&&(!paint_context||!j.contains("source_materials")||(detail::material_archive_base_version(j.at("source_materials"))!=9&&!(key_context&&key_feature("color_preparations","paint_sampling")))))throw std::invalid_argument("Scene24 requires its actual paired material9 and writable domain pool");
  if(version<24&&j.contains("source_materials")&&detail::material_archive_base_version(j.at("source_materials"))==9)throw std::invalid_argument("Material9 requires explicit Scene24");
  if(version==23&&(!j.contains("source_materials")||(detail::material_archive_base_version(j.at("source_materials"))!=8&&!(key_context&&key_feature("color_preparations","uv_reprojection")))||!j.contains("polygon_sources")||j.at("polygon_sources").at("version")!=5))throw std::invalid_argument("Scene23 requires actual current corner colour UV authority");
  if(version<23&&j.contains("source_materials")&&detail::material_archive_base_version(j.at("source_materials"))==8)throw std::invalid_argument("Current colour UV replay requires Scene23");
  if(version<26 && j.contains("source_models"))detail::preflight_creator_source_coordinates(j.at("source_models"),unsigned(version));
  if(version==21&&(!j.contains("normal_sampling")||!j.contains("source_materials")||detail::material_archive_base_version(j.at("source_materials"))!=7))
    throw std::invalid_argument("Scene21 requires its owning retained normal sampling archive");
  if(version<21&&(j.contains("normal_sampling")||(j.contains("source_materials")&&detail::material_archive_base_version(j.at("source_materials"))==7)))
    throw std::invalid_argument("Retained normal sampling requires Scene21");
  if(version==20&&(!j.contains("source_materials")||detail::material_archive_base_version(j.at("source_materials"))!=6||!j.contains("polygon_sources")||j.at("polygon_sources").at("version")!=5))throw std::invalid_argument("Scene20 requires explicit current corner preparation authority");
  if(version<20&&((j.contains("source_materials")&&detail::material_archive_base_version(j.at("source_materials"))==6)||(j.contains("polygon_sources")&&j.at("polygon_sources").at("version")==5)))throw std::invalid_argument("Current corner preparation requires Scene20");
  if(version==19&&(!j.contains("source_materials")||!j.at("source_materials").contains("version")||detail::material_archive_base_version(j.at("source_materials"))!=5))throw std::invalid_argument("Scene19 requires material archive5 normal UV authority");
  if(version<19&&j.contains("source_materials")&&j.at("source_materials").contains("version")&&detail::material_archive_base_version(j.at("source_materials"))==5)throw std::invalid_argument("Normal UV projection requires Scene19");
  const auto &objects = j.at("objects");
  if (!objects.is_array() || objects.empty() || objects.size() > 256)
    throw std::length_error("Invalid scene object count");
  if((version==40)!=j.contains("texture_set_geometry"))throw std::invalid_argument("Texture sets require exactly their Scene40 owning geometry archive");
  size_t texture_slots=0;
  for(const auto&o:objects){
    if(version==40&&!o.contains("texture_sets"))throw std::invalid_argument("Incomplete Scene40 texture-set fields");
    if(version<40&&o.contains("texture_sets"))throw std::invalid_argument("Texture-set membership requires Scene40");
    if(version==40&&!o.at("texture_sets").is_null()&&(!o.at("painted").is_null()||o.at("mesh_active")!=true))throw std::invalid_argument("Texture-set geometry cannot have a second mutable primary owner");
  }
  if(version==40)for(const auto&o:objects){
    size_t count=1; // The implicit material slot also belongs to a volume object.
    if(!o.at("texture_sets").is_null()){
      count=1;
      const auto fields=texture_holder_paints(o.at("texture_sets"),j.at("texture_set_geometry"));
      if(fields.size()<2||fields.size()>64)throw std::invalid_argument("Incomplete texture-set collection");count=fields.size();
    }
    if(count>256-texture_slots)throw std::length_error("Scene texture-set count exceeds bounds");texture_slots+=count;
  }
  std::optional<detail::TextureSetArchive> texture_archive;

  // Admit newly meaningful implicit-default identity before decoding any
  // retained appearance/remap, including dormant and before-state owners.
  auto appearance_feature=[&](const nlohmann::json &a){if(detail::minimum_appearance_scene_version(a)>version)throw std::invalid_argument("Implicit default material authority requires Scene17");};
  auto appearance_table=[&](const nlohmann::json &table,size_t maximum){
    if(!table.is_array()||table.size()>maximum)throw std::length_error("Retained appearance table exceeds its archive bound");
    for(const auto &owner:table)appearance_feature(owner.at("appearance"));
  };
  for(const auto &o:objects)if(o.contains("appearance"))appearance_feature(o.at("appearance"));
  if(j.contains("polygon_sources"))appearance_table(j.at("polygon_sources").at("origins"),512);
  if(j.contains("source_materials")){
    const auto &materials=j.at("source_materials");
    for(const char *table:{"origins","color_preparations","normal_preparations","key_derivations"})if(materials.contains(table))appearance_table(materials.at(table),256);
  }
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
    detail::require_material_emission_version(o.at("material"),unsigned(version));
    if(version>=16){if(!o.contains("shape_source"))throw std::invalid_argument("Incomplete shape source scene fields");}
    else if(o.contains("shape_source"))throw std::invalid_argument("Shape source requires Scene16");
    if(version==25||rig_pool){if(!o.contains("rig_source"))throw std::invalid_argument("Incomplete rig source scene fields");}
    else if(version<25&&o.contains("rig_source")&&!o.at("rig_source").is_null())throw std::invalid_argument("Rig source requires Scene25");
  }
  if(j.contains("polygon_sources"))for(const auto &origin:j.at("polygon_sources").at("origins"))detail::require_material_emission_version(origin.at("material"),unsigned(version));
  if(j.contains("polygon_sources") && j.at("polygon_sources").at("version")==4 && version<16)
    throw std::invalid_argument("Direct shape source archive requires Scene16");
  const bool polygon_fields = version >= 12;
  if (polygon_fields) {
    if (!j.contains("polygon_sources"))
      throw std::invalid_argument("Incomplete polygon source scene fields");
    if(version<13 && j.at("polygon_sources").at("version")!=1)throw std::invalid_argument("Exact polygon placement requires Scene13");
    if(version<14 && j.at("polygon_sources").at("version")==3)
      throw std::invalid_argument("Current polygon corner colors require Scene14");
    detail::PolygonSceneArchive::preflight(j.at("polygon_sources"),objects,(shared_corner_context||canonical_paint_context||polygon_surface_context)?&j.at("source_materials"):nullptr,canonical_paint_context?&j.at("canonical_paint"):nullptr,polygon_surface_context?&j.at("polygon_surfaces"):nullptr,version==40?&j.at("texture_set_geometry"):nullptr);
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
    detail::PolygonSceneBudget budget;budget.owning_paint_domain=paint_context;
    budget.paint_bytes=layered_normal_bytes;
    if(layered_context)for(const auto &c:j.at("baked_normal_fields").at("origins"))budget.deformation(c.at("deformation"),unsigned(version));
    budget.independent_source_paint=source_paint_context.independent_source;budget.owning_physical_emission=version>=27;
    const bool shared_texture_geometry=version==40&&j.at("texture_set_geometry").at("version")==2;
    if(version==40){
      const auto&geometry=j.at("texture_set_geometry");
      for(const auto*name:{"meshes","topologies"}){const auto&table=geometry.at(name);if(!table.is_array()||table.empty()||table.size()>256)throw std::length_error("Invalid texture-set geometry count");for(const auto&record:table)for(const auto&field:record.items())if(field.value().is_binary())detail::PolygonSceneBudget::add(budget.paint_bytes,field.value().get_binary().size(),256*1024*1024,"Shared geometry/identity exceeds whole Scene storage");}
      if(shared_texture_geometry){
        for(const auto&assignment:geometry.at("assignments")){if(!assignment.is_binary()||assignment.get_binary().size()%4||assignment.get_binary().size()>8'000'000)throw std::invalid_argument("Invalid shared texture assignment buffer");detail::PolygonSceneBudget::add(budget.paint_bytes,assignment.get_binary().size(),256*1024*1024,"Shared assignment budget exceeded");}
        for(const auto&record:geometry.at("paints")){const auto&paint=record.at("painted");const auto*seed=paint.contains("field_source")?detail::texture_source_seed(paint,geometry,j.at("source_materials"),j.at("polygon_surfaces")):nullptr;if(!paint.is_object()||paint.at("version")!=7||(!seed&&paint.size()!=3))throw std::invalid_argument("Shared texture Paint7 envelope differs");budget.paint(paint.at("payload"),false,unsigned(version),true,seed?std::optional<bool>(paint.at("physical_paint").get<bool>()):std::nullopt);}
        for(const auto&seed:geometry.at("field_sources"))budget.paint(seed.at("original"),true,unsigned(version),true,seed.at("physical_paint").get<bool>());
      }else for(const auto&o:objects)if(!o.at("texture_sets").is_null()){const auto&slots=o.at("texture_sets").at("slots");if(!slots.is_binary()||slots.get_binary().size()%4||slots.get_binary().size()>8'000'000)throw std::invalid_argument("Invalid texture assignment storage");detail::PolygonSceneBudget::add(budget.paint_bytes,slots.get_binary().size(),256*1024*1024,"Texture assignment budget exceeded");}
    }
    for (const auto &o : objects) {
      budget.volume(o.at("volume"));
      budget.paint(o.at("painted"),false,unsigned(version));
      if(version==40&&!shared_texture_geometry&&!o.at("texture_sets").is_null()){
        const auto &sets=o.at("texture_sets");
        auto member=[&](const nlohmann::json &paint){
          if(!paint.is_object()||paint.size()!=3||!paint.at("version").is_number_integer()||paint.at("version")!=7||!paint.at("payload").is_object())throw std::invalid_argument("Scene40 needs actual membership-required Paint7");
          budget.paint(paint.at("payload"),false,unsigned(version),true);
        };
        if(!sets.at("additional").is_array()||sets.at("additional").size()>63)throw std::length_error("Too many texture sets");
        member(sets.at("primary"));for(const auto &set:sets.at("additional"))member(set.at("painted"));
      }
      budget.deformation(o.at("deformation"), unsigned(version));
      budget.appearance(o.at("appearance"));
      budget.retopology(o.at("retopology"));
    }
    const auto &poly = j.at("polygon_sources");
    for (const auto &v : poly.at("volumes"))
      budget.volume(v);
    for (const auto &p : poly.at("paints"))
      budget.paint(p,true,unsigned(version));
    for (const auto &o : poly.at("origins")) {
      budget.deformation(o.at("deformation"), unsigned(version));
      budget.appearance(o.at("appearance"));
      budget.retopology(o.at("retopology"));
    }
    const auto &materials = j.at("source_materials");
    for (const auto &p : materials.at("paints"))
      budget.paint(p,true,unsigned(version));
    for (const auto &o : materials.at("origins")) {
      if (o.contains("deformation"))
        budget.deformation(o.at("deformation"), unsigned(version));
      budget.appearance(o.at("appearance"));
    }
    if(materials.contains("key_derivations"))for(const auto &k:materials.at("key_derivations")){budget.deformation(k.at("deformation"),unsigned(version));budget.appearance(k.at("appearance"));}
    if(materials.contains("color_preparations"))for(const auto &p:materials.at("color_preparations")){budget.deformation(p.at("deformation"),unsigned(version));budget.appearance(p.at("appearance"));for(const char *key:{"uv_reprojection","paint_sampling"})if(p.contains(key)){const auto &u=p.at(key);budget.deformation(u.at("deformation"),unsigned(version));budget.appearance(u.at("appearance"));}}
    if(materials.contains("normal_preparations"))for(const auto &p:materials.at("normal_preparations")){budget.deformation(p.at("deformation"),unsigned(version));budget.appearance(p.at("appearance"));if(p.contains("uv_reprojection")){const auto &u=p.at("uv_reprojection");budget.deformation(u.at("deformation"),unsigned(version));budget.appearance(u.at("appearance"));}if(p.contains("sampling_domain")){const auto &u=p.at("sampling_domain");budget.deformation(u.at("deformation"),unsigned(version));budget.appearance(u.at("appearance"));}}
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
  for(const auto &o:objects)if(o.contains("painted")&&!o.at("painted").is_null()&&detail::polygon_paint_policy(detail::paint_archive_payload(o.at("painted"),paint_context,source_paint_context))){
    if(!polygon_fields||!o.contains("polygon")||o.at("polygon").is_null()||o.at("mesh_active")!=true)
      throw std::invalid_argument("Polygon-authored paint requires authenticated Scene12 polygon authority");
    auto index=detail::polygon_archive_index(o.at("polygon"),j.at("polygon_sources").at("authorities").size());
    if(j.at("polygon_sources").at("authorities")[index].at("active")!=true||j.at("polygon_sources").at("authorities")[index].at("topology").at("faces").empty())
      throw std::invalid_argument("Polygon-authored paint has an inactive current authority");
  }
  if(j.contains("source_materials")&&j.at("source_materials").is_object()&&j.at("source_materials").contains("paints"))
    for(const auto &paint:j.at("source_materials").at("paints"))if(detail::polygon_paint_policy(detail::paint_archive_payload(paint,paint_context,source_paint_context)))
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
    size_t pixels=0,storage=layered_normal_bytes;
    std::function<void(const nlohmann::json&,std::optional<bool>)> preflight_grid;
    const auto preflight=[&](const nlohmann::json&record){preflight_grid(record,{});};
    preflight_grid=[&](const nlohmann::json &record,std::optional<bool> source_physical) {
      if(record.is_null())return;const auto &paint=detail::paint_archive_payload(record,paint_context,source_paint_context);
      if(source_physical&&(version!=40||record.contains("version")||record.contains("mesh")))throw std::invalid_argument("Source grid requires its actual inspected shared field envelope");
      if(record.contains("version")&&record.at("version")==4){
        storage+=record.at("vertices").get_binary().size();preflight(record.at("before"));
      }
      if(paint.contains("source_maps")){auto coordinates=detail::source_uv_archive_measure(paint.at("source_maps"),unsigned(version));storage+=coordinates.bytes;}
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
        if(layer.contains("physical_emission_tiles")){
          const auto &tiles=layer.at("physical_emission_tiles");
          if(version<27||!(source_physical.value_or(false)||detail::physical_paint_authority(record,source_paint_context))||!tiles.is_array()||tiles.empty()||tiles.size()>4096)
            throw std::invalid_argument("Physical paint requires actual Scene27/Paint3 capability");
          storage+=tiles.size()*sizeof(PaintEmissionTile);
        }
      }
      if(paint.contains("baked"))for(const auto *key:{"color","orm","emission","normal","coverage"}) {
        const auto &bytes=paint.at("baked").at(key);
        if(!bytes.is_binary())throw std::runtime_error("Invalid retained baked buffer");
        storage+=bytes.get_binary().size();
      }
      if(paint.contains("color_projection") && version<10)throw std::runtime_error("Prepared color projection requires scene10");
      if(paint.contains("normal_projection")&&paint.at("normal_projection").at("version")==2&&version<21)throw std::invalid_argument("Retained normal field requires Scene21");
      if(paint.contains("normal_projection")&&version<15)throw std::invalid_argument("Prepared normal projection requires Scene15");
      if(paint.contains("baked")&&paint.at("baked").at("version")!=1){
        const auto &baked=paint.at("baked");const auto bv=baked.at("version").get<int>();
        if(bv==2||((bv==3||bv==5)&&(baked.contains("pbr_color")||baked.contains("color_interpretation")))){
          if(version<10||!baked.at("pbr_color").is_binary())throw std::runtime_error("Resolved baked PBR requires scene10");
          storage+=baked.at("pbr_color").get_binary().size();
        }
        if(bv==4&&version<21)throw std::invalid_argument("Retained normal detail requires Scene21");
        if(bv==3||bv==4){if(version<15||!baked.at("mtoon_normal").is_binary())throw std::invalid_argument("Prepared baked normals require Scene15");storage+=baked.at("mtoon_normal").get_binary().size();}
        else if(bv==5){if(!layered_context||!baked.contains("normal_field"))throw std::invalid_argument("Independent baked payload5 requires actual Scene33");}
        else if(bv!=2)throw std::invalid_argument("Unsupported retained baked payload");
      }
      if(pixels>16*1024*1024 || storage>256*1024*1024)
        throw std::length_error("Scene retained painting allocations exceed budget");
    };
    if(material_fields) {
      const auto &materials=j.at("source_materials");
      if(!materials.is_object() || !detail::material_archive_base_version(materials).is_number_integer() || (detail::material_archive_base_version(materials)!=1 && (version<9 || detail::material_archive_base_version(materials)!=2) && (version<10 || detail::material_archive_base_version(materials)!=3) && (version<15 || detail::material_archive_base_version(materials)!=4) && (version<19 || detail::material_archive_base_version(materials)!=5) && (version<20 || detail::material_archive_base_version(materials)!=6) && (version<21 || detail::material_archive_base_version(materials)!=7) && (version<23 || detail::material_archive_base_version(materials)!=8) && (version<24 || detail::material_archive_base_version(materials)!=9) && (version<22 || detail::material_archive_base_version(materials)!=10)) ||
         !materials.at("paints").is_array() || materials.at("paints").size()>256)
        throw std::runtime_error("Invalid retained source-material archive");
      for(const auto &paint:materials.at("paints"))preflight(paint);
    }
    if(!j.at("objects").is_array() || j.at("objects").empty() || j.at("objects").size()>256)
      throw std::runtime_error("Invalid scene object count");
    for(const auto &object:j.at("objects")){
      preflight(object.at("painted"));
      if(version==40&&j.at("texture_set_geometry").at("version")==1&&!object.at("texture_sets").is_null())for(const auto*paint:texture_holder_paints(object.at("texture_sets"),j.at("texture_set_geometry")))preflight(paint->at("payload"));
    }
    if(version==40&&j.at("texture_set_geometry").at("version")==2){const auto&geometry=j.at("texture_set_geometry");for(const auto&p:geometry.at("paints")){const auto&paint=p.at("painted");if(paint.contains("field_source")){(void)detail::texture_source_seed(paint,geometry,j.at("source_materials"),j.at("polygon_surfaces"));preflight_grid(paint.at("payload"),paint.at("physical_paint").get<bool>());}else preflight(paint.at("payload"));}for(const auto&seed:geometry.at("field_sources"))preflight_grid(seed.at("original"),seed.at("physical_paint").get<bool>());}
  }
  // Aggregate paint/deformation/sculpt preflight precedes all current geometry
  // reconstruction as well as every SurfaceMap allocation.
  if(version==40)texture_archive=detail::TextureSetArchive::decode_geometry(j.at("texture_set_geometry"));
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
      if(object.at("painted").is_null()&&!(version==40&&!object.at("texture_sets").is_null()))throw std::runtime_error("Retopology has no retained display mesh");
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
  detail::BakedNormalArchive layered_normals(layered_context?&j.at("baked_normal_fields"):nullptr,source_images);
  CreatorAssetArchive source_models;
  if (archives) {
    if(version<11)for(const auto &model:j.at("source_models"))if(model.contains("original_source"))
      throw std::runtime_error("Original format source receipts require scene version eleven");
    source_models = CreatorAssetArchive::decode(j.at("source_models"), source_images);
  }
  if(material_fields)for(const auto&origin:j.at("source_materials").at("origins"))if(origin.contains("deformation")&&detail::minimum_deformation_scene_version(origin.at("deformation"))>version)throw std::runtime_error("Retained explicit inverse binds require scene version eleven");
  if(material_fields&&j.at("source_materials").contains("color_preparations"))for(const auto &preparation:j.at("source_materials").at("color_preparations")){
    detail::require_deformation_scene_version(preparation.at("deformation"),version,"Preconversion explicit inverse binds require scene version eleven");
    for(const char *key:{"uv_reprojection","paint_sampling"})if(preparation.contains(key))
      detail::require_deformation_scene_version(preparation.at(key).at("deformation"),version,"Preconversion replay exceeds its actual scene version");
  }
  if(material_fields&&j.at("source_materials").contains("key_derivations"))for(const auto &key:j.at("source_materials").at("key_derivations"))detail::require_deformation_scene_version(key.at("deformation"),version,"Key source deformation exceeds its actual Scene feature");
  if(version==15&&(!material_fields||detail::material_archive_base_version(j.at("source_materials"))!=4))throw std::invalid_argument("Scene15 requires its explicit normal preparation feature");
  if(material_fields&&j.at("source_materials").contains("normal_preparations"))for(const auto &preparation:j.at("source_materials").at("normal_preparations"))
    if(version<15||detail::minimum_deformation_scene_version(preparation.at("deformation"))>version)throw std::invalid_argument("Pre-normal-preparation source features exceed scene version");
  size_t preflight_deformation=material_fields?SourceMaterialArchive::deformation_entries(j.at("source_materials")):0;
  if(version>=4)for(const auto &o:objects)if(!o.at("deformation").is_null()) {
    const auto &d=o.at("deformation");
    detail::require_deformation_scene_version(d,version,"Explicit inverse binds require scene version eleven");
    preflight_deformation+=d.at("pose").size()+d.at("morph_weights").size()+detail::encoded_pose_matrix_entries(d);
    if(!d.at("skin").is_null())for(const auto &v:d.at("skin").at("vertices"))preflight_deformation+=v.size();
    if(!d.at("morphs").is_null())for(const auto &t:d.at("morphs").at("targets"))preflight_deformation+=t.at("delta").size();
    if(preflight_deformation>8'000'000)throw std::length_error("Scene original and current deformation allocations exceed budget");
  }
  detail::PolygonColorArchive color_archive;
  if(shared_corner_context)color_archive=detail::PolygonColorArchive::decode(j.at("polygon_sources").at("color_bases"),j.at("polygon_sources").at("color_fields"));
  NormalSamplingArchive normal_domains;
  if(normal_context)normal_domains=NormalSamplingArchive::decode(j.at("normal_sampling"),source_images);
  PaintSamplingArchive writable_domains;if(paint_context)writable_domains=PaintSamplingArchive::decode(j.at("paint_sampling"));
  SourceMaterialArchive source_materials;
  if(material_fields&&!canonical_paint_context&&!polygon_surface_context)source_materials=SourceMaterialArchive::decode(j.at("source_materials"),source_images,source_models,shared_corner_context?&color_archive:nullptr,normal_context?&normal_domains:nullptr,paint_context?&writable_domains:nullptr,source_paint_context);
  detail::SourcePlacementArchive placements;
  if(j.contains("affine_state"))placements=detail::SourcePlacementArchive::decode(j.at("affine_state"),source_models);
  detail::PolygonSceneArchive polygons;
  std::unique_ptr<detail::PolygonSceneReader> polygon_reader;
  std::unique_ptr<detail::PolygonSurfaceArchive> polygon_surfaces;
  std::unique_ptr<detail::CanonicalPaintArchive> canonical_paints;
  std::unique_ptr<detail::TextureSetFieldCodec>texture_field_codec;
  if(texture_archive&&texture_archive->uses_shared_records()){
    texture_field_codec=std::make_unique<detail::TextureSetFieldCodec>(texture_archive->field_source_records(),[&](uint32_t geometry){return texture_archive->geometry_at(geometry);},j.at("texture_set_geometry").at("meshes").size(),polygon_surface_context?j.at("polygon_surfaces").at("nodes").size():0);
    texture_archive->field_decoder([&](const nlohmann::json&paint,const auto&domain,uint32_t geometry,const Material&material,const PaintedMesh*raster,bool polygon){auto field=texture_field_codec->decode(paint,domain,geometry,material,source_images,normal_context?&normal_domains:nullptr,paint_context?&writable_domains:nullptr,[&](uint32_t id){return polygon_reader?polygon_reader->material_at(id):source_materials.at(id);},polygon,raster,source_paint_context);return detail::TextureSetArchiveField{std::move(field.paint),std::move(field.source),std::move(field.binding),std::move(field.appearance),std::move(field.recipe)};},[&]{texture_field_codec->require_complete();});
  }
  if(canonical_paint_context||polygon_surface_context){
    polygon_reader=std::make_unique<detail::PolygonSceneReader>(j.at("polygon_sources"),source_images,source_models,[&source_materials](uint32_t id){return source_materials.at(id);},&placements,shared_corner_context?&color_archive:nullptr,normal_context?&normal_domains:nullptr,&writable_domains,source_paint_context,std::function<bool()>{},texture_archive?&*texture_archive:nullptr);
    if(polygon_surface_context)polygon_surfaces=std::make_unique<detail::PolygonSurfaceArchive>(j.at("polygon_surfaces"),*polygon_reader,j.at("polygon_sources").at("origins").size(),texture_field_codec?detail::PolygonSurfaceGeometryReader([&](uint32_t node){return texture_field_codec->geometry_for_node(node);}):detail::PolygonSurfaceGeometryReader{});
    if(canonical_paint_context)canonical_paints=std::make_unique<detail::CanonicalPaintArchive>(j.at("canonical_paint"),*polygon_reader,layered_normals,writable_domains);
    source_materials=SourceMaterialArchive::decode(j.at("source_materials"),source_images,source_models,shared_corner_context?&color_archive:nullptr,normal_context?&normal_domains:nullptr,&writable_domains,source_paint_context);
    polygons=polygon_reader->finish();
  }else if(polygon_fields)polygons=detail::PolygonSceneArchive::decode(j.at("polygon_sources"),source_images,source_models,source_materials,&placements,shared_corner_context?&color_archive:nullptr,normal_context?&normal_domains:nullptr,paint_context?&writable_domains:nullptr,source_paint_context,texture_archive?&*texture_archive:nullptr);
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
  detail::NativeSceneGeometryIntern decoded_geometry;
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
        const auto &paint_payload=detail::paint_archive_payload(v.at("painted"),paint_context,source_paint_context);const auto &size = paint_payload.at("resolution");
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
        o.painted = PaintedMesh::decode(v.at("painted"), archives ? &source_images : nullptr, polygon_fields && !v.at("polygon").is_null() && o.mesh_active,normal_context?&normal_domains:nullptr,paint_context?&writable_domains:nullptr,source_paint_context);
        paint_bytes += o.painted->storage_bytes();
        if (paint_bytes > 256 * 1024 * 1024) throw std::length_error("Scene paint storage budget exceeded");
      }
    }
    if(version==40&&!v.at("texture_sets").is_null()){
      // Actual holder context is available before Paint7. The shared decoder
      // strong-matches the primary's real source/material/recipe identities.
      if(!v.at("material_bindings").is_null())o.material_bindings=source_materials.at(uint32_t(detail::polygon_archive_index(v.at("material_bindings"),source_materials.bindings().size())));
      if(!v.at("appearance").is_null())o.appearance=decode_creator_appearance(v.at("appearance"),source_models);
      if(!v.at("bake_recipe").is_null())o.bake_recipe=ObjectBakeRecipe::decode(v.at("bake_recipe"));
      if(polygon_fields&&!v.at("polygon").is_null())o.polygon=polygons.at(detail::polygon_archive_index(v.at("polygon"),polygons.authorities().size()));
      texture_archive->decode(o,v.at("texture_sets"),source_images,normal_context?&normal_domains:nullptr,paint_context?&writable_domains:nullptr,source_paint_context);
      texels+=size_t(o.painted->resolution())*o.painted->resolution();paint_bytes+=o.painted->storage_bytes();
      for(const auto &set:o.texture_sets->additional()){texels+=size_t(set.painted->resolution())*set.painted->resolution();paint_bytes+=set.painted->storage_bytes();}
      if(texels>16*1024*1024||paint_bytes>256*1024*1024)throw std::length_error("Scene texture-set paint budget exceeded");
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
      deformation_entries += d.at("pose").size() + d.at("morph_weights").size()+detail::encoded_pose_matrix_entries(d);
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
    if (archives && !o.texture_sets && !v.at("appearance").is_null()) {
      remap_vertices += v.at("appearance").at("source_vertices").get_binary().size() / 4;
      if (remap_vertices > 8'000'000) throw std::length_error("Creator source remap budget exceeded");
      o.appearance = decode_creator_appearance(v.at("appearance"), source_models);
    }
    if(material_fields && !o.texture_sets && !v.at("material_bindings").is_null()) {
      const auto &binding=v.at("material_bindings");
      if((!binding.is_number_unsigned() && (!binding.is_number_integer() || binding.get<int64_t>()<0)) ||
         binding.get<uint64_t>()>UINT32_MAX)throw std::runtime_error("Invalid resolved material identity");
      o.material_bindings=source_materials.at(binding.get<uint32_t>());
    }
    if(o.painted&&!o.texture_sets&&o.painted->paint_sampling_domain()){
      const auto attachment=detail::material_paint_attachment(o.material_bindings.get());
      if(!attachment)throw std::invalid_argument("Current writable paint lacks its own exact material witness");
      o.painted=rebind_paint_sampling_geometry(*o.painted,attachment);
    }
    if(o.painted&&!o.texture_sets&&o.material_bindings&&o.material_bindings->key_derivation)o.painted=detail::adopt_key_paint_geometry(*o.painted,*o.material_bindings,o.appearance.get(),o.deformation.get());
    if(polygon_fields && !v.at("polygon").is_null())o.polygon=polygons.at(detail::polygon_archive_index(v.at("polygon"),polygons.authorities().size()));
    if(version>=30&&!o.texture_sets&&!v.at("bake_recipe").is_null())o.bake_recipe=ObjectBakeRecipe::decode(v.at("bake_recipe"));
    if(version>=16 && !v.at("shape_source").is_null())o.shape_source=polygons.origins().at(detail::polygon_archive_index(v.at("shape_source"),polygons.origins().size()));
    if(version>=25 && v.contains("rig_source") && !v.at("rig_source").is_null())o.rig_source=polygons.origins().at(detail::polygon_archive_index(v.at("rig_source"),polygons.origins().size()));
    result.objects_.push_back(std::move(o));
  }
  placements.require_reachable();
  if(normal_context)normal_domains.require_reachable();if(paint_context)writable_domains.require_reachable();
  layered_normals.require_reachable();
  if(texture_archive)texture_archive->require_reachable();
  if(canonical_paints)canonical_paints->require_complete();
  unsigned actual_feature=0;
  for(const auto &o:result.objects_)actual_feature=std::max({actual_feature,o.bake_recipe?30u:0u,detail::hdr_emission(o.material.emissive)?27u:0u,detail::minimum_scene_version(o.painted.get()),detail::minimum_scene_version(o.appearance.get()),detail::minimum_scene_version(o.deformation.get()),detail::minimum_scene_version(o.material_bindings.get()),detail::minimum_scene_version(o.polygon.get()),detail::minimum_scene_version(o.shape_source.get()),o.rig_source?25u:0u,detail::minimum_scene_version(o.rig_source.get())});
  if(detail::validate_texture_set_graph(result).collections)actual_feature=std::max(actual_feature,40u);
  if(version==40&&actual_feature!=40)throw std::invalid_argument("Scene40 lacks actual decoded texture-set membership");
  actual_feature=std::max(actual_feature,detail::composed_material_scene_version(source_materials));
  if(version==39&&actual_feature!=39)throw std::invalid_argument("Scene39 lacks genuine decoded current polygon field ownership");
  if(version==37&&actual_feature!=37)throw std::invalid_argument("Scene37 lacks genuine decoded canonical paint ownership");
  if(version==36&&actual_feature!=36)throw std::invalid_argument("Scene36 lacks genuine current native paint/frame source");
  if(version==35&&actual_feature!=35)throw std::invalid_argument("Scene35 lacks genuine decoded native/canonical composition");
  if(actual_feature>unsigned(version))throw std::invalid_argument("Retained data requires its actual Scene feature version");
  if(version==29&&actual_feature!=29)throw std::invalid_argument("Scene29 requires genuine decoded explicit Mirror authority");
  if(version==32&&actual_feature!=32)throw std::invalid_argument("Scene32 requires genuine decoded native paint authority");
  if(version==30&&actual_feature!=30)throw std::invalid_argument("Scene30 requires an actual bake recipe");
  if(version==31&&actual_feature!=31)throw std::invalid_argument("Scene31 requires an actual decoded canonical conversion lease");
  if(version==34&&actual_feature!=34)throw std::invalid_argument("Scene34 requires actual converted normal UV correspondence");
  if(version==33&&actual_feature!=33)throw std::invalid_argument("Scene33 requires an actual separate canonical attribute origin");
  if(version==28&&actual_feature!=28)throw std::invalid_argument("Scene28 requires actual independent source paint ownership");
  if(version==27&&actual_feature<27)throw std::invalid_argument("Scene27 requires genuine physical emission state");
  if(material_fields&&(polygon_surface_context||detail::physical_material_archive(j.at("source_materials"))||detail::explicit_mirror_material_archive(j.at("source_materials"))||detail::canonical_material_archive(j.at("source_materials"))||native_paint_context||detail::layered_material_archive(j.at("source_materials")))){
    SourceMaterialArchive reachable;
    for(const auto&o:result.objects_){if(o.material_bindings)reachable.add(o.material_bindings,false);if(o.texture_sets)for(const auto&field:o.texture_sets->additional())if(field.material_bindings)reachable.add(field.material_bindings,false);}
    for(const auto&o:polygons.origins()){if(o->material_bindings)reachable.add(o->material_bindings,false);if(o->texture_sets)for(const auto&field:o->texture_sets->additional())if(field.material_bindings)reachable.add(field.material_bindings,false);}
    if(reachable.bindings().size()!=source_materials.bindings().size()||reachable.origins().size()!=source_materials.origins().size())
      throw std::invalid_argument("Scene HDR material archive has unreachable owning roots");
  }
  if(version==25&&actual_feature!=25)throw std::invalid_argument("Scene25 requires actual exact pose or rig source authority");
  for(const auto &g:result.groups_)if(g.source_placement && detail::minimum_scene_version(g.source_placement->model.get())>unsigned(version))throw std::invalid_argument("Original source coordinates require their actual scene feature version");
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
  return load_scene_file(path, cancelled).scene;
}
std::vector<ObjectSurface>
Scene::surfaces(bool atlas, const std::function<bool()> &cancelled) const {
  require_exportable_sources();
  return surfaces_impl(atlas, false, cancelled);
}
std::vector<ObjectSurface> Scene::preview_surfaces(const std::function<bool()> &cancelled) const {
  return surfaces_impl(false, true, cancelled);
}
std::vector<ObjectSurface> Scene::export_surfaces(const std::function<bool()> &cancelled) const {
  bool sets=false;for(const auto&o:objects_)sets|=o.visible&&bool(o.texture_sets);
  auto result=sets?preview_surface_bundles(cancelled,{}):preview_surfaces(cancelled);
  for(auto &surface:result){
    if(cancelled&&cancelled())throw std::runtime_error("Source paint export cancelled");
    const auto &o=object(surface.id);
    if(o.mesh_active&&o.painted->source_sampling_prepared())
      detail::source_paint_export(*o.painted,surface,cancelled);
  }
  return result;
}
std::vector<ObjectSurface> Scene::preview_surfaces(const std::function<bool()> &cancelled, const SculptMesher &mesher) const {
  return surfaces_impl(false, true, cancelled, mesher);
}
std::vector<ObjectSurface> Scene::preview_surface_bundles(const std::function<bool()> &cancelled,const SculptMesher &mesher)const {
  return surfaces_impl(false,true,cancelled,mesher,true);
}
std::vector<ObjectSurface> Scene::surfaces_impl(bool atlas, bool preview, const std::function<bool()> &cancelled, const SculptMesher &mesher,bool texture_bundles,
    const std::function<bool(const SculptObject &)> *capture_sculpt,bool render_only) const {
  bool has_sets=false;for(const auto &o:objects_)if(o.texture_sets){has_sets=true;if(!texture_bundles)throw std::logic_error("Texture-set surfaces require their owning subset render/export adapter");}
  if(has_sets)detail::validate_texture_set_graph(*this);
  std::vector<ObjectSurface> result;
  for (auto &o : objects_)
    if (o.visible) {
      if(o.texture_sets){
        if(!preview||atlas)throw std::logic_error("Texture-set bundles cannot substitute the multi-primitive export compiler");
        TextureSetLimits limits;limits.cancelled=cancelled;
        auto plan=detail::prepare_texture_set_surface(o,limits);
        if(plan->indices().empty())continue;
        ObjectSurface surface;surface.id=o.id;surface.name=o.name;surface.transform=o.transform;
        surface.material=o.material;surface.deformation=o.deformation;surface.groups=group_path(o.group);
        surface.affine_edit=o.affine_edit;surface.rig_source=o.rig_source;surface.texture_set_surface=std::move(plan);
        result.push_back(std::move(surface));continue;
      }
      if(o.mesh_active&&o.painted&&o.painted->paint_sampling_domain()){
        if(atlas)throw std::invalid_argument("Retained writable paint needs its explicit compact/current-layout compilation adapter");
        if(!o.painted->valid_paint_sampling_attachment())throw std::invalid_argument("Retained paint lost its exact REST geometry attachment");
        if(!o.material_bindings||o.material_bindings->has_mtoon)throw std::invalid_argument("Retained paint requires its own supported current-field material");
        o.material_bindings->validate(false);detail::source_paint_pairing(o.painted.get(),o.material_bindings.get(),o.appearance.get(),false);
      }
      if(o.polygon && o.polygon->active && o.polygon->topology.faces().empty())continue;
      if(!o.mesh_active&&capture_sculpt&&(*capture_sculpt)(o))continue;
      auto mesh = o.mesh_active ? o.painted->mesh()
                                : mesher ? mesher(o.id, *o.volume) : surface_mesh(*o.volume, cancelled);
      if (mesh.indices.empty())
        continue;
      if (atlas && !o.mesh_active)
        unwrap(mesh, 1024, 4, cancelled);
      const bool polygon_field=o.mesh_active&&o.material_bindings&&bool(o.material_bindings->polygon_surface_transfer);
      // Only the actual render publisher may select a GPU-only marker. Public
      // preview/export/material-sampling callers still receive their real Images.
      const auto current_grid=render_only?detail::CurrentGridPaintPreview::for_render(o,cancelled):nullptr;
      if(polygon_field&&!preview)throw std::invalid_argument("Polygon surface standard export requires its actual portable field compiler");
      result.push_back(
          {o.id, o.name, o.transform, o.material, std::move(mesh),
           current_grid?current_grid:o.mesh_active ? (polygon_field ? detail::PolygonSurfacePaintCodec::preview(*o.painted,o.material_bindings,o.material,cancelled) : (preview||o.painted->source_sampling_prepared()) ? (detail::EmissionExportAccess::requires_preview(*o.painted,o.material,o.material_bindings.get()) ? detail::EmissionExportAccess::preview(*o.painted,o.material,cancelled) : o.painted->preview(o.material,cancelled)) : o.painted->composite(o.material,cancelled)) : nullptr,
           o.mesh_active ? o.deformation : nullptr, group_path(o.group),
           o.mesh_active ? o.appearance : nullptr,
           o.mesh_active ? o.material_bindings : nullptr,
           source_is_dormant(o)});
      result.back().affine_edit=o.affine_edit;
      result.back().rig_source=o.mesh_active?o.rig_source:nullptr;
      if(!preview&&o.mesh_active&&o.painted->source_sampling_prepared())
        detail::source_paint_export(*o.painted,result.back(),cancelled);
      if(o.mesh_active&&o.painted->paint_sampling_domain()){
        result.back().paint_sampling_attachment=o.painted->paint_sampling_attachment();
        if(!polygon_field)result.back().normal_preview_proof=prepare_normal_preview(*o.painted,o.material_bindings,o.material,cancelled);
        const auto domain=o.painted->paint_sampling_domain();const auto &basis=*domain->basis();
        auto &uv=result.back().paint_corner_coordinates;uv.reserve(domain->source_corners().size());size_t c=0;
        // The factory seal already proves this exact REST mesh. Read the private
        // immutable correspondence; no repeated mesh/plane replay on publishers.
        for(auto corner:domain->source_corners()){if((c++&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Retained paint coordinate packing cancelled");uv.push_back(basis.uv.at(basis.indices.at(corner)));}
      }
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
