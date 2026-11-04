// SPDX-License-Identifier: GPL-3.0-or-later
#include "cut-color.hpp"
#include "dome/document.hpp"
#include "dome/bake-recipe.hpp"
#include "dome/polygon-scene.hpp"
#include "polygon-color-target.hpp"
#include "dome/normal-preparation.hpp"
namespace dome {
AssetStatus Document::asset_status() const {
  auto work=asset_work_.load();
  if(!work)return {};
  bool cancelling=work->cancelled.load();
  return {!cancelling,cancelling,work->progress.load()};
}
void Document::cancel_asset_work_locked(){
  if(projection_load_&&!projection_load_->done.load()){
    projection_load_->cancelled=true;state_.message="Cancelling image load...";
  }
  if(auto work=asset_work_.load();work && state_.asset_busy){
    work->cancelled.store(true);state_.message="Cancelling background work...";
  }
}
void Document::cancel_background_work_locked(){
  cancel_asset_work_locked();
  for(const auto &work:exports_)if(work && work->cancel())
    state_.message="Cancelling background work...";
}
void Document::cancel_asset_work(){
  std::lock_guard lock(model_mutex_);cancel_asset_work_locked();publish_locked();
}
void Document::cancel_background_work(){
  std::lock_guard lock(model_mutex_);cancel_background_work_locked();publish_locked();
}
bool Document::bake_source_eligible_locked(uint64_t id) const {
  const auto at=std::find_if(scene_.objects().begin(),scene_.objects().end(),[id](const auto &o){return o.id==id;});
  return at!=scene_.objects().end()&&id!=scene_.active().id&&!scene_.has_affine_placement(id)&&
    detail::cut_color_supported(*at)&&(at->mesh_active?at->painted&&!at->painted->mesh().indices.empty():!at->volume->voxels().empty());
}
std::vector<uint64_t> Document::bake_sources_locked() const {
  std::vector<uint64_t> ids;
  const auto &target=scene_.active();
  if(!detail::corner_bake_target_supported(target))return ids;
  for(const auto &o:scene_.objects())if(o.id!=scene_.active().id && !scene_.has_affine_placement(o.id) &&
      detail::cut_color_supported(o) &&
      (o.mesh_active ? o.painted && !o.painted->mesh().indices.empty() : !o.volume->voxels().empty()))ids.push_back(o.id);
  if(state_.bake_source==unavailable_bake_object||
      (target.bake_recipe&&target.bake_recipe->source.object==unavailable_bake_object))
    ids.push_back(unavailable_bake_object);
  if(target.bake_recipe&&std::find(ids.begin(),ids.end(),target.bake_recipe->source.object)==ids.end())
    ids.push_back(target.bake_recipe->source.object);
  return ids;
}
std::vector<uint64_t> Document::bake_cages_locked() const {
  // Eligibility is deliberately cheap on the input thread. Actual ordered
  // topology, evaluated geometry and outward bounds are checked on the worker.
  std::vector<uint64_t> ids{0};
  for(const auto &o:scene_.objects())
    if(o.id!=scene_.active().id && o.mesh_active && o.painted &&
       !o.painted->mesh().indices.empty())ids.push_back(o.id);
  // Preserve a missing/ineligible explicit choice until the user reselects it.
  // Deletion must never silently change a custom cage into automatic projection.
  if(state_.bake_settings.cage_object &&
      std::find(ids.begin(),ids.end(),state_.bake_settings.cage_object)==ids.end())
    ids.push_back(state_.bake_settings.cage_object);
  if(scene_.active().bake_recipe&&scene_.active().bake_recipe->cage&&
      scene_.active().bake_recipe->cage->object==unavailable_bake_object&&
      std::find(ids.begin(),ids.end(),unavailable_bake_object)==ids.end())
    ids.push_back(unavailable_bake_object);
  return ids;
}
bool Document::bake_target_ready_locked() const {
  const auto &o=scene_.active();
  return detail::corner_bake_target_supported(o) && !state_.shape_sculpt && !scene_.has_affine_placement(o.id) && !state_.polygon && !state_.cut && !state_.retopology && !uv_ &&
    !state_.asset_busy && !state_.recovery_busy && !action_active_locked() &&
    object_editable_locked(o.id) && o.mesh_active && o.painted;
}
void Document::queue_bake_locked(uint64_t source,ObjectBakeSettings settings,AssetSource expected){
  if(source==unavailable_bake_object||settings.cage_object==unavailable_bake_object)
    throw std::logic_error("The saved bake source or cage is unavailable; select its replacement explicitly");
  auto ids=bake_sources_locked();
  if(!bake_target_ready_locked() || !bake_source_eligible_locked(source) || std::find(ids.begin(),ids.end(),source)==ids.end())
    throw std::logic_error("Choose a prepared, visible, unlocked target and a different nonempty source");
  auto report=std::make_shared<std::string>();auto work=std::make_shared<AssetWork>();
  queue_scene_operation_locked("Bake surface",[source,settings,report,work](Scene &scene,const auto &cancelled){
    auto result=bake_object(scene,source,settings,cancelled,[work](float p){work->progress.store(p);});
    *report=std::to_string(result.projected)+" projected, "+std::to_string(result.missed)+" missed ("+std::to_string(result.preserved_misses)+" kept from prior bake)";
    if(result.color_prepared){char quality[96];snprintf(quality,sizeof(quality),"; color error bound %.4f",double(*std::max_element(result.color_error_bound.begin(),result.color_error_bound.end())));*report+=quality;}
    if(result.canonical_captured)*report+="; verified normal frame captured";
    if(result.normal_prepared){char quality[128];snprintf(quality,sizeof(quality),"; portable normal attributes authored (rest quantization %.3f deg)",result.normal_initial_error*57.29577951308232);*report+=quality;}
  },expected,report,work,{}, {},0,0,{},BakeAdmission{source,settings});
}
void Document::bake_from_object(uint64_t source,ObjectBakeSettings settings,AssetSource expected){
  std::lock_guard lock(model_mutex_);queue_bake_locked(source,settings,expected);
}
void Document::queue_clear_bake_locked(AssetSource expected){
  if(!bake_target_ready_locked() || !scene_.active().painted->baked())throw std::logic_error("Select an editable baked target");
  queue_scene_operation_locked("Clear baked surface",[](Scene &scene,const auto &cancelled){
    if(cancelled())throw std::runtime_error("Clear bake cancelled");
    auto &o=scene.active();
    if(o.material_bindings&&o.material_bindings->normal_preparation&&o.material_bindings->normal_preparation->canonical_conversion){
      NormalPreparationOptions options;options.cancelled=cancelled;
      auto result=convert_canonical_normals({o.painted,o.appearance,o.material_bindings,o.deformation,o.material,{}},options);
      o.painted=std::move(result.paint);o.material_bindings=std::move(result.material_bindings);
    }
    scene.active().painted->clear_bake();
  },expected,{});
}
void Document::clear_bake(AssetSource expected){
  std::lock_guard lock(model_mutex_);queue_clear_bake_locked(expected);
}
bool Document::canonical_conversion_ready_locked() const {
  const auto &o=scene_.active();
  if(!state_.object_grabbing&&bake_target_ready_locked()&&o.material_bindings&&o.material_bindings->normal_preparation){
    const auto &p=*o.material_bindings->normal_preparation;
    return p.canonical_conversion&&!p.uv_reprojection&&!p.sampling_domain&&!p.key_derivation;
  }
  return !state_.object_grabbing&&bake_target_ready_locked()&&!o.appearance&&!o.material_bindings&&
    o.painted&&!o.painted->normal_projection()&&!o.painted->paint_sampling_domain()&&!o.painted->source_sampling_prepared()&&
    o.painted->baked()&&!o.painted->baked()->maps.normal.rgba.empty()&&
    o.painted->baked()->normal_interpretation==BakedNormalInterpretation::CanonicalMikk;
}
void Document::queue_convert_bake_locked(AssetSource expected){
  if(control_before_||!canonical_conversion_ready_locked())throw std::logic_error("Select an editable native mesh with historical normal detail for explicit conversion");
  const float error=state_.bake_settings.normal_error;
  auto report=std::make_shared<std::string>();auto work=std::make_shared<AssetWork>();
  queue_scene_operation_locked("Convert to portable normals",[error,report,work](Scene &scene,const auto &cancelled){
    auto &o=scene.active();NormalPreparationOptions options;options.max_angular_error=error;options.cancelled=cancelled;
    work->progress.store(.05f);
    auto result=convert_canonical_normals({o.painted,o.appearance,o.material_bindings,o.deformation,o.material,{}},options);
    o.painted=std::move(result.paint);o.material_bindings=std::move(result.material_bindings);
    scene.validate();if(cancelled())throw std::runtime_error("Canonical normal conversion cancelled");
    *report="Portable normal/tangent targets authored; original source retained. Pose and morph shading follow the new model.";work->progress.store(.95f);
  },expected,report,work);
}
void Document::convert_baked_normals(AssetSource expected){
  std::lock_guard lock(model_mutex_);queue_convert_bake_locked(expected);
}
} // namespace dome
