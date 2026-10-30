// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
namespace dome {
namespace {
bool same(const CutSettings &a,const CutSettings &b){
  return a.yaw==b.yaw&&a.pitch==b.pitch&&a.offset_mm==b.offset_mm&&a.cap==b.cap&&a.flipped==b.flipped&&
         a.cap_material.color==b.cap_material.color&&a.cap_material.metallic==b.cap_material.metallic&&
         a.cap_material.roughness==b.cap_material.roughness&&a.cap_material.emissive==b.cap_material.emissive&&
         a.cap_material.name==b.cap_material.name&&a.cap_material.toon==b.cap_material.toon&&
         a.cap_material.double_sided==b.cap_material.double_sided&&a.cap_material.alpha==b.cap_material.alpha&&
         a.cap_material.alpha_cutoff==b.cap_material.alpha_cutoff&&a.cap_material.unlit==b.cap_material.unlit;
}
std::shared_ptr<const BakeSource> source_mesh(const MeshSnapshot &base,uint64_t object){
  auto range=std::find_if(base.ranges.begin(),base.ranges.end(),[&](auto &r){return r.object==object;});
  if(range==base.ranges.end()||!range->count)throw std::logic_error("The cut source has no displayed surface");
  auto source=std::make_shared<BakeSource>();source->materials[0].material=range->material;source->materials[0].maps=range->textures;
  for(size_t k=range->first;k<range->first+range->count;++k){
    auto vertex=base.mesh.indices.at(k);auto &mesh=source->mesh;
    mesh.indices.push_back(uint32_t(mesh.positions.size()));mesh.positions.push_back(base.mesh.positions.at(vertex));
    mesh.normals.push_back(base.mesh.normals.at(vertex));if(!base.mesh.uv.empty())mesh.uv.push_back(base.mesh.uv.at(vertex));
  }
  source->mesh.validate();return source;
}
std::shared_ptr<const MeshSnapshot> preview_snapshot(const MeshSnapshot &base,const CutPreview &preview,
                                                    uint64_t session,uint64_t request,uint64_t object){
  auto result=std::make_shared<MeshSnapshot>(base);result->cut_session=session;result->cut_request=request;
  result->clipped_object=object;result->clip_plane=preview.plane;
  size_t first=result->mesh.indices.size(),offset=result->mesh.positions.size();auto &cap=preview.cap_mesh;
  if(!cap.indices.empty()){
    // The cap changes packed geometry independently of the source scene key.
    // Own a distinct immutable identity so both eyes reuse this preview, while
    // a later plane/cap request cannot retain the source or an earlier cap VBO.
    result->geometry_identity=std::make_shared<const uint8_t>(0);
    result->mesh.positions.insert(result->mesh.positions.end(),cap.positions.begin(),cap.positions.end());
    result->mesh.normals.insert(result->mesh.normals.end(),cap.normals.begin(),cap.normals.end());
    for(size_t n=0;n<cap.positions.size();++n){result->mesh.uv.push_back(cap.uv.empty()?std::array<float,2>{}:cap.uv[n]);
      if(!result->tangents.empty())result->tangents.push_back({1,0,0,1});
      // Cap ranges use their own generic material, but shared vertex streams
      // still have one entry per packed vertex. Padding is not frame authority.
      if(!result->source_normals.empty())result->source_normals.push_back(cap.normals[n]);
      if(!result->canonical_normals.empty())result->canonical_normals.push_back(cap.normals[n]);
      if(!result->source_tangents.empty())result->source_tangents.push_back({1,0,0,1});
      if(!result->selected_weights.empty())result->selected_weights.push_back(-1);
      if(!result->sculpt_masks.empty())result->sculpt_masks.push_back(-1);}
    for(auto vertex:cap.indices)result->mesh.indices.push_back(uint32_t(offset+vertex));
    result->ranges.push_back({UINT64_MAX,first,cap.indices.size(),preview.settings.cap_material,{}, {},offset});
  }
  return result;
}
}
void Document::begin_cut(){std::lock_guard lock(model_mutex_);finish_locked();begin_cut_locked();}
void Document::begin_cut_locked(){
  const auto &o=scene_.active();
  if(state_.cut||state_.retopology||state_.asset_busy||state_.recovery_busy||control_before_||action_active_locked()||
     !o.visible||o.locked||(!o.mesh_active&&o.volume->voxels().empty())||scene_.objects().size()>254||scene_.groups().size()>=128)
    throw std::logic_error("Finish the current action; show and unlock a source with room for cut outputs");
  cut_settings_.validate();cut_undo_.clear();cut_redo_.clear();cut_base_.reset();cut_bake_source_.reset();
  auto view=std::make_shared<CutView>();view->session=++cut_session_;view->request=++cut_request_;
  view->settings=cut_settings_;state_.cut=view;state_.weights.enabled=false;state_.error.clear();state_.message.clear();
  publish_locked(true);
  cut_source_={state_.generation,state_.revision,o.id};
  {std::lock_guard lock(cut_mutex_);cut_job_=CutJob{view->session,view->request,cut_source_,cut_settings_};}
  cut_changed_.notify_one();
}
void Document::edit_cut(CutSettings settings){std::lock_guard lock(model_mutex_);edit_cut_locked(std::move(settings));publish_locked();}
void Document::edit_cut_locked(CutSettings settings,bool remember){
  settings.validate();if(!state_.cut||state_.cut->committing)throw std::logic_error("Open an editable plane-cut workspace");
  if(same(settings,cut_settings_))return;
  if(remember){cut_undo_.push_back(cut_settings_);if(cut_undo_.size()>128)cut_undo_.erase(cut_undo_.begin());cut_redo_.clear();}
  cut_settings_=std::move(settings);auto view=std::make_shared<CutView>(*state_.cut);
  view->settings=cut_settings_;view->request=++cut_request_;view->busy=true;view->error.clear();
  view->can_undo=!cut_undo_.empty();view->can_redo=!cut_redo_.empty();state_.cut=view;state_.error.clear();
  {std::lock_guard lock(cut_mutex_);cut_job_=CutJob{view->session,view->request,cut_source_,cut_settings_};}
  cut_changed_.notify_one();
}
void Document::cut_history_locked(bool redo){
  if(!state_.cut||state_.cut->committing)return;
  if(control_before_){finish_control_locked(false);input_.cancel();return;}
  auto &from=redo?cut_redo_:cut_undo_,&to=redo?cut_undo_:cut_redo_;
  if(from.empty())return;
  auto settings=from.back();from.pop_back();to.push_back(cut_settings_);
  edit_cut_locked(std::move(settings),false);
}
void Document::finish_cut_parameter_locked(bool accept){
  if(accept&&!same(cut_settings_,control_cut_settings_)){
    cut_undo_.push_back(control_cut_settings_);if(cut_undo_.size()>128)cut_undo_.erase(cut_undo_.begin());cut_redo_.clear();
  }else if(!accept&&state_.cut)edit_cut_locked(control_cut_settings_,false);
  if(state_.cut){auto view=std::make_shared<CutView>(*state_.cut);view->can_undo=!cut_undo_.empty();view->can_redo=!cut_redo_.empty();state_.cut=view;}
  control_before_.reset();control_changed_=false;state_.controls={};
}
void Document::cancel_cut_locked(){
  ++cut_session_;++cut_request_;state_.cut.reset();cut_undo_.clear();cut_redo_.clear();cut_bake_source_.reset();
  {std::lock_guard lock(mesh_mutex_);
    if(cut_base_&&cut_base_->generation==state_.generation&&cut_base_->revision==state_.revision)mesh_.store(cut_base_);
    cut_base_.reset();mesh_changed_.notify_all();}
  {std::lock_guard lock(cut_mutex_);cut_job_.reset();cut_changed_.notify_all();}
}
void Document::end_cut(bool accept){std::lock_guard lock(model_mutex_);end_cut_locked(accept);publish_locked();}
void Document::end_cut_locked(bool accept){
  if(!state_.cut)return;
  if(state_.cut->committing){
    if(!accept&&state_.asset_busy){cancel_asset_work_locked();return;}
    throw std::logic_error("Cancel background work or wait for the accepted cut");
  }
  if(!accept){finish_control_locked(false);cancel_cut_locked();input_.cancel();desktop_=false;state_.error.clear();return;}
  if(control_before_||state_.cut->busy)throw std::logic_error("Finish plane controls and wait for the cut preview");
  auto preview=state_.cut->preview;
  if(!preview||state_.cut->preview_request!=state_.cut->request)throw std::logic_error("Choose a valid cutting plane before Apply");
  if(!preview->changed){cancel_cut_locked();input_.cancel();return;}
  auto source=cut_bake_source_;if(!source)throw std::logic_error("Cut source is unavailable");
  auto view=std::make_shared<CutView>(*state_.cut);view->committing=true;state_.cut=view;
  auto report=std::make_shared<std::string>();auto work=std::make_shared<AssetWork>();
  try{queue_scene_operation_locked("Apply plane cut",[preview,source,report,work](Scene &scene,const auto &cancelled){
    auto result=publish_cut(scene,*preview,*source,512,cancelled,[work](float p){work->progress.store(p);});
    *report=std::to_string(result.projected)+" mapped texels; source retained";
  },cut_source_,report,work,{}, {},0,view->session);}catch(...){view->committing=false;state_.cut=view;throw;}
  input_.cancel();
}
void Document::begin_cut_parameter(VrParameter parameter){
  std::lock_guard lock(model_mutex_);control_event_locked({VrControlEventType::Begin,parameter});desktop_=true;publish_locked();
}
void Document::edit_cut_parameter(int component,float normalized){
  std::lock_guard lock(model_mutex_);control_event_locked({VrControlEventType::Absolute,state_.controls.parameter,component,normalized});publish_locked();
}
void Document::end_cut_parameter(bool accept){
  std::lock_guard lock(model_mutex_);finish_control_locked(accept);desktop_=false;input_.cancel();publish_locked();
}
void Document::cut_from_hand_locked(){
  if(!state_.cut)throw std::logic_error("Start a cut first");
  auto owner=input_.command_device();
  auto hand=owner==1?state_.hands[0].tracked:owner==2?state_.hands[1].tracked:
      state_.hands[1].tracked.valid?state_.hands[1].tracked:state_.hands[0].tracked;
  if(!hand.valid)throw std::logic_error("A tracked hand is required to position the plane");
  auto p=world_to_model(state_.canvas,{hand.grip.position.x,hand.grip.position.y,hand.grip.position.z});
  auto n=ob::rotate(ob::conjugate(state_.canvas.orientation),ob::rotate(hand.grip.orientation,{0,0,1}));
  Vec3 normal=normalized({n.x,n.y,n.z});auto settings=cut_settings_;
  constexpr float degrees=180/3.14159265358979323846f;
  settings.yaw=std::atan2(normal.z,normal.x)*degrees;settings.pitch=std::asin(std::clamp(normal.y,-1.f,1.f))*degrees;
  settings.offset_mm=dot(normal,p-state_.cut->center)*1000;
  edit_cut_locked(std::move(settings));
}
bool Document::wait_cut(std::chrono::milliseconds timeout) const {
  std::unique_lock lock(cut_mutex_);return cut_changed_.wait_for(lock,timeout,[&]{
    auto view=published_.load()->cut;return stopping_.load()||!view||(!view->busy&&!view->committing);
  });
}
void Document::cut_loop(){
  uint64_t session=0;std::shared_ptr<const MeshSnapshot> base;std::shared_ptr<const BakeSource> source;
  for(;;){
    CutJob job;
    {std::unique_lock lock(cut_mutex_);cut_changed_.wait(lock,[&]{return stopping_.load()||cut_job_||(base&&session!=cut_session_.load());});
      if(stopping_)return;
      if(base&&session!=cut_session_.load()){lock.unlock();base.reset();source.reset();session=0;continue;}
      job=std::move(*cut_job_);cut_job_.reset();}
    auto cancelled=[&]{auto state=published_.load();return stopping_.load()||cut_session_.load()!=job.session||
      state->generation!=job.expected.generation||state->revision!=job.expected.revision||state->active_object!=job.expected.object;};
    try{
      if(cancelled())continue;
      if(!base||session!=job.session){
        {std::unique_lock lock(mesh_mutex_);mesh_changed_.wait(lock,[&]{auto m=mesh_.load();return cancelled()||
          (m->generation==job.expected.generation&&m->revision==job.expected.revision&&!m->cut_session)||!published_.load()->error.empty();});
          if(cancelled())continue;
          base=mesh_.load();}
        if(base->generation!=job.expected.generation||base->revision!=job.expected.revision)throw std::runtime_error("Cut source mesh failed to prepare");
        source=source_mesh(*base,job.expected.object);session=job.session;auto [center,radius]=cut_bounds(source->mesh);
        std::lock_guard lock(model_mutex_);if(cancelled()||!state_.cut)continue;
        cut_base_=base;cut_bake_source_=source;auto view=std::make_shared<CutView>(*state_.cut);
        view->center=center;view->radius=radius;view->ready=true;view->offset_limit_mm=std::min(100'000.f,std::max(1.f,radius*1000));state_.cut=view;publish_locked();
      }
      auto preview=std::make_shared<const CutPreview>(preview_cut(source->mesh,job.settings,cancelled));
      auto snapshot=preview_snapshot(*base,*preview,job.session,job.request,job.expected.object);
      std::lock_guard lock(model_mutex_);if(cancelled()||!state_.cut)continue;
      auto view=std::make_shared<CutView>(*state_.cut);view->preview=preview;view->preview_request=job.request;
      view->busy=job.request!=cut_request_.load();if(!view->busy){view->error.clear();state_.error.clear();}
      state_.cut=view;{std::lock_guard mesh_lock(mesh_mutex_);mesh_.store(snapshot);mesh_changed_.notify_all();}publish_locked();
    }catch(const std::exception &e){
      std::lock_guard lock(model_mutex_);if(cancelled()||!state_.cut||job.request!=cut_request_.load())continue;
      auto view=std::make_shared<CutView>(*state_.cut);view->busy=false;view->error=e.what();state_.cut=view;state_.error=e.what();publish_locked();
    }
    cut_changed_.notify_all();
  }
}
} // namespace dome
