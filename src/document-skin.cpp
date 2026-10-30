// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
namespace dome {
bool Document::skin_target_ready_locked() const {
  const auto &o=scene_.active();
  return !state_.polygon && !state_.cut && !state_.retopology && !uv_ &&
    !state_.asset_busy && !state_.recovery_busy && !action_active_locked() &&
    object_editable_locked(o.id) && o.mesh_active && o.painted;
}
std::vector<uint64_t> Document::skin_sources_locked(SkinAuthoringMode mode) const {
  std::vector<uint64_t> ids;
  for(const auto &o:scene_.objects())
    if((mode!=SkinAuthoringMode::Transfer || o.id!=scene_.active().id) &&
       o.mesh_active && o.painted && !o.painted->mesh().indices.empty() &&
       o.deformation && o.deformation->skeleton && (mode!=SkinAuthoringMode::Transfer || o.deformation->skin)) ids.push_back(o.id);
  return ids;
}
void Document::queue_bind_skin_locked(SkinAuthoringOptions options,AssetSource expected) {
  options.validate();
  auto ids=skin_sources_locked(options.mode);
  if(!skin_target_ready_locked() || std::find(ids.begin(),ids.end(),options.reference)==ids.end())
    throw std::logic_error("Select an editable mesh and an eligible skeleton/skin reference");
  auto object=scene_.active().id;
  auto changed=std::make_shared<bool>(false);
  auto report=std::make_shared<std::string>();
  queue_scene_operation_locked(options.mode==SkinAuthoringMode::Transfer?"Transfer skin weights":"Automatic skin bind",
    [object,options,changed,report](Scene &scene,const auto &cancelled) {
      *changed=author_skin(scene,object,options,cancelled);
      *report=*changed?"Skin updated; rest geometry, paint and morphs retained":"No skin changes; original weights retained";
    },expected,report,{},changed);
  state_.skin_use_locks=!options.locks.empty();
  if(!options.locks.empty()) state_.weights.locks=options.locks;
  options.locks.clear();state_.skin_options=std::move(options);
  publish_locked();
}
void Document::bind_skin(SkinAuthoringOptions options,AssetSource expected) {
  std::lock_guard lock(model_mutex_);queue_bind_skin_locked(std::move(options),expected);
}
} // namespace dome
