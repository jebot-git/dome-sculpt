// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
namespace dome {
AssetStatus Document::asset_status() const {
  auto work=asset_work_.load();
  if(!work)return {};
  bool cancelling=work->cancelled.load();
  return {!cancelling,cancelling,work->progress.load()};
}
void Document::cancel_asset_work_locked(){
  if(auto work=asset_work_.load();work && state_.asset_busy){
    work->cancelled.store(true);state_.message="Cancelling background work...";
  }
}
void Document::cancel_asset_work(){
  std::lock_guard lock(model_mutex_);cancel_asset_work_locked();publish_locked();
}
std::vector<uint64_t> Document::bake_sources_locked() const {
  std::vector<uint64_t> ids;
  for(const auto &o:scene_.objects())if(o.id!=scene_.active().id &&
      (o.mesh_active ? o.painted && !o.painted->mesh().indices.empty() : o.volume->stored_samples()!=0))ids.push_back(o.id);
  return ids;
}
bool Document::bake_target_ready_locked() const {
  const auto &o=scene_.active();
  return !state_.asset_busy && !state_.recovery_busy && !action_active_locked() &&
    o.visible && !o.locked && o.mesh_active && o.painted;
}
void Document::queue_bake_locked(uint64_t source,ObjectBakeSettings settings,AssetSource expected){
  auto ids=bake_sources_locked();
  if(!bake_target_ready_locked() || std::find(ids.begin(),ids.end(),source)==ids.end())
    throw std::logic_error("Choose a prepared, visible, unlocked target and a different nonempty source");
  auto report=std::make_shared<std::string>();auto work=std::make_shared<AssetWork>();
  queue_scene_operation_locked("Bake surface",[source,settings,report,work](Scene &scene,const auto &cancelled){
    auto result=bake_object(scene,source,settings,cancelled,[work](float p){work->progress.store(p);});
    *report=std::to_string(result.projected)+" texels, "+std::to_string(result.padded)+" padded, "+std::to_string(result.rays)+" rays";
  },expected,report,{},work);
  state_.bake_source=source;state_.bake_settings=settings;publish_locked();
}
void Document::bake_from_object(uint64_t source,ObjectBakeSettings settings,AssetSource expected){
  std::lock_guard lock(model_mutex_);queue_bake_locked(source,settings,expected);
}
void Document::queue_clear_bake_locked(AssetSource expected){
  if(!bake_target_ready_locked() || !scene_.active().painted->baked())throw std::logic_error("Select an editable baked target");
  queue_scene_operation_locked("Clear baked surface",[](Scene &scene,const auto &cancelled){
    if(cancelled())throw std::runtime_error("Clear bake cancelled");
    scene.active().painted->clear_bake();
  },expected,{});
}
void Document::clear_bake(AssetSource expected){
  std::lock_guard lock(model_mutex_);queue_clear_bake_locked(expected);
}
} // namespace dome
