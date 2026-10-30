// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/mesh-processing.hpp"
#include "dome/material-visibility.hpp"
#include "document-transaction.hpp"
#include <pthread.h>
namespace dome {
namespace {
Vec3 local(CanvasTransform t,Vec3 p){auto q=ob::transform(ob::inverse(t),{p.x,p.y,p.z});return {q.x,q.y,q.z};}
Vec3 rotate(ob::Quat q,Vec3 p){auto v=ob::rotate(q,{p.x,p.y,p.z});return {v.x,v.y,v.z};}
bool same(CanvasTransform a,CanvasTransform b){return a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z&&a.scale==b.scale&&a.orientation.x==b.orientation.x&&a.orientation.y==b.orientation.y&&a.orientation.z==b.orientation.z&&a.orientation.w==b.orientation.w;}
}
bool Document::begin_native_stroke(){
  std::lock_guard lock(model_mutex_);auto &o=scene_.active();
  if(native_stroke_){state_.message="Finish or cancel the current desktop stroke before starting another";publish_locked();return false;}
  if(desktop_||state_.cut||state_.retopology||state_.weights.enabled||uv_||control_before_||state_.asset_busy||state_.recovery_busy||action_active_locked()||input_.menu_open()||!o.visible||o.locked||(!o.mesh_active&&!volume().editable_layer()&&!volume().sculpt_settings().mask_edit))return false;
  auto preview=mesh_.load();
  if(!native_source_scene_||preview->generation!=state_.generation||preview->revision!=state_.revision){state_.message="Wait for the current surface before painting";publish_locked();return false;}
  auto session=std::make_shared<NativeStroke>();session->epoch=++native_epoch_;
  session->source={state_.generation,state_.revision,o.id};session->transform=scene_.world_transform(o.id);session->deformation=o.deformation;
  session->initial=native_source_scene_;session->original_preview=preview;session->paint=state_.paint;
  std::lock_guard queue(native_mutex_);
  if(native_jobs_.size()>=8){state_.error="Desktop gesture queue is full; retry after pending cancellation finishes";publish_locked();return false;}
  Transaction transaction(*this);native_jobs_.push_back(session);
  try{
    input_.cancel();native_stroke_=std::move(session);desktop_=true;state_.error.clear();state_.message="Desktop stroke";state_.native_samples=0;
    publish_locked(false,false);transaction.commit();
  }catch(...){native_jobs_.pop_back();throw;}
  native_changed_.notify_all();return true;

}
bool Document::sample_native_stroke(NativeStrokeSample sample){
  if(!std::isfinite(sample.material_time)||!finite(sample.origin)||!finite(sample.direction)||length(sample.direction)<1e-8f||!std::isfinite(sample.brush.radius)||sample.brush.radius<=0||sample.brush.radius>5||!std::isfinite(sample.brush.pressure)||sample.brush.pressure<0||sample.brush.pressure>1)throw std::invalid_argument("Invalid native stroke sample");
  std::lock_guard lock(model_mutex_);if(!native_stroke_)return false;
  bool overflow=false;
  {std::lock_guard queue(native_mutex_);auto &s=*native_stroke_;if(!s.accepting)return false;
    if(s.submitted>=4096)overflow=true;else{s.samples.push_back(std::move(sample));++s.submitted;}}
  if(overflow){cancel_native_locked();state_.error="Desktop stroke cancelled: 4096-sample gesture budget reached; no partial stroke was retained";publish_locked();return false;}
  native_changed_.notify_all();return true;
}
void Document::end_native_locked(bool accept){
  if(!native_stroke_)return;
  if(!accept){cancel_native_locked();return;}
  {std::lock_guard queue(native_mutex_);native_stroke_->accepting=false;native_stroke_->accepted=true;}
  state_.message="Finishing desktop stroke...";native_changed_.notify_all();
}
void Document::cancel_native_locked(){
  if(!native_stroke_)return;
  auto old=std::move(native_stroke_);
  {std::lock_guard queue(native_mutex_);old->cancelled=true;++native_epoch_;}
  desktop_=false;input_.cancel();
  if(old->source.generation==state_.generation&&old->source.revision==state_.revision){std::lock_guard mesh(mesh_mutex_);mesh_.store(old->original_preview);mesh_changed_.notify_all();}
  state_.native_samples=0;state_.error.clear();state_.message="Desktop stroke cancelled";native_changed_.notify_all();
}
void Document::end_native_stroke(bool accept){std::lock_guard lock(model_mutex_);end_native_locked(accept);publish_locked();}
bool Document::wait_native_stroke(std::chrono::milliseconds timeout)const{
  std::unique_lock lock(native_mutex_);return native_changed_.wait_for(lock,timeout,[&]{return native_jobs_.empty()&&!native_processing_;});
}
void Document::drain_native_shutdown(){
  {std::lock_guard lock(model_mutex_);if(native_stroke_&&!native_stroke_->accepted.load())cancel_native_locked();}
  // Accepted release is an ordered transaction; finish it while meshing/files
  // are still alive. A held pointer gesture is deliberately cancelled above.
  std::unique_lock lock(native_mutex_);native_changed_.wait(lock,[&]{return native_jobs_.empty()&&!native_processing_;});
}
void Document::capture_native_file(FileJob &job){
  auto session=job.native_capture;
  {std::unique_lock lock(native_mutex_);native_changed_.wait(lock,[&]{return session->done;});
    if(!session->error.empty())throw std::runtime_error("Desktop stroke did not complete; save cancelled: "+session->error);
    job.revision=session->revision;job.generation=session->source.generation;job.content=session->content;job.recovery_source=session->recovery_source;}
  // This immutable completion is the exact accepted endpoint, even if another
  // gesture or object change has already started before the ordered file job.
  job.scene=session->completed->snapshot();job.native_capture.reset();
}
void Document::native_loop(){
  pthread_setname_np(pthread_self(),"dome-native");
  for(;;){
    std::shared_ptr<NativeStroke> session;
    {std::unique_lock lock(native_mutex_);native_changed_.wait(lock,[&]{return stopping_||!native_jobs_.empty();});if(stopping_&&native_jobs_.empty())return;session=std::move(native_jobs_.front());native_jobs_.pop_front();native_processing_=true;}
    auto cancelled=[&]{return session->cancelled.load()||native_epoch_.load()!=session->epoch;};
    std::string error;
    try{
      if(cancelled())throw std::runtime_error("Desktop stroke cancelled");
      Scene working=session->initial->snapshot();working.select(session->source.object);auto &o=working.active();
      size_t budget=4'000'000;for(const auto &other:working.objects())if(other.id!=o.id){auto used=std::max(other.volume->stored_samples(),other.volume->voxels().size());if(used>budget)throw std::length_error("Scene sculpt storage budget exceeded");budget-=used;}
      std::unique_ptr<MaterialSurfaceQuery> query;
      std::shared_ptr<const PaintedMesh::Evaluation> evaluation;
      if(o.mesh_active){
        std::shared_ptr<const Mesh> geometry=o.deformation?std::make_shared<const Mesh>(o.deformation->preview(o.painted->mesh())):std::shared_ptr<const Mesh>(o.painted,&o.painted->mesh());
        if(cancelled())throw std::runtime_error("Desktop stroke cancelled");
        if(o.deformation)evaluation=o.painted->evaluate(geometry,cancelled);
        SurfaceRange range{o.id,0,geometry->indices.size(),o.material,o.painted->preview(o.material),o.appearance,0,o.material_bindings};
        query=std::make_unique<MaterialSurfaceQuery>(*geometry,std::span<const SurfaceRange>(&range,1),cancelled);o.painted->begin_stroke();
      }else o.volume->begin_stroke();
      std::optional<Vec3> previous;size_t processed=0;auto shown=std::chrono::steady_clock::now();
      for(;;){
        std::optional<NativeStrokeSample> sample;bool accept=false;
        {std::unique_lock lock(native_mutex_);native_changed_.wait(lock,[&]{return cancelled()||!session->samples.empty()||!session->accepting;});
          if(cancelled())throw std::runtime_error("Desktop stroke cancelled");
          if(!session->samples.empty()){sample=std::move(session->samples.front());session->samples.pop_front();}else accept=session->accepted.load();}
        if(!sample){if(accept)break;continue;}
        auto origin=local(session->transform,sample->origin);auto direction=rotate(ob::conjugate(session->transform.orientation),sample->direction);
        std::optional<Vec3> hit;Vec3 normal;
        if(query){auto picked=query->raycast(origin,direction,10/session->transform.scale,sample->material_time);if(picked){hit=picked->position;normal=picked->normal;}}
        else {hit=o.volume->raycast(origin,direction,10/session->transform.scale);if(hit)normal=o.volume->gradient(*hit);}
        if(hit){auto brush=sample->brush;brush.from=previous.value_or(*hit);brush.to=*hit;brush.normal=normal;brush.radius/=session->transform.scale;brush.blend/=session->transform.scale;
          if(o.mesh_active){if(evaluation)o.painted->stamp_evaluated(brush,session->paint,*evaluation,cancelled);else o.painted->stamp(brush,session->paint,cancelled);}else o.volume->stamp(brush,budget,cancelled);previous=*hit;
        }else previous.reset();
        ++processed;
        if(cancelled())throw std::runtime_error("Desktop stroke cancelled");
        auto now=std::chrono::steady_clock::now();
        if(now-shown>=std::chrono::milliseconds(30)){
          auto preview=working.snapshot();
          std::lock_guard lock(model_mutex_);
          if(native_stroke_!=session||session->source.generation!=state_.generation||session->source.revision!=state_.revision||scene_.active().id!=session->source.object||scene_.active().deformation!=session->deformation||!same(scene_.world_transform(session->source.object),session->transform))throw std::runtime_error("Desktop source or pose changed");
          state_.native_samples=processed;publish_locked();
          {std::lock_guard mesh(mesh_mutex_);pending_mesh_.emplace(MeshJob{state_.revision,state_.generation,std::move(preview),false,true,0,0,{}, {}});pending_mesh_->sculpt_mask_view=state_.sculpt_mask_visible;pending_mesh_->native_gesture=session->epoch;pending_mesh_->native_samples=processed;}
          mesh_changed_.notify_all();shown=now;
        }
      }
      if(cancelled())throw std::runtime_error("Desktop stroke cancelled");
      bool changed=o.mesh_active?o.painted->end_stroke():o.volume->end_stroke();
      auto immutable=std::make_shared<const Scene>(working.snapshot());
      std::deque<History> previous_redo;
      {std::lock_guard lock(model_mutex_);
        if(cancelled()||native_stroke_!=session||session->source.generation!=state_.generation||session->source.revision!=state_.revision||scene_.active().id!=session->source.object||scene_.active().deformation!=session->deformation||!same(scene_.world_transform(session->source.object),session->transform))throw std::runtime_error("Desktop source or pose changed");
        // Stage every history/state allocation while authority is untouched.
        // The old redo deque is swapped only after its replacement node exists.
        Scene before=scene_;EditorState previous_state=state_;
        auto previous_published=published_.load();auto previous_source=native_source_scene_;
        auto previous_bound=bound_volume_;auto previous_volume=last_volume_,previous_paint=last_paint_;
        auto previous_content=content_,previous_next=next_content_;
        session->recovery_source=recovery_source_;
        if(changed){
          if(next_content_==UINT64_MAX)throw std::overflow_error("Document history identity exhausted");
          History entry;entry.before=before;entry.after=working;entry.from=content_;entry.to=next_content_;
          undo_.push_back(std::move(entry));previous_redo.swap(redo_);
        }
        try {
          native_stroke_.reset();desktop_=false;input_.cancel();++native_epoch_;
          if(changed){scene_=std::move(working);content_=next_content_++;bind_locked();}
          else {std::lock_guard mesh(mesh_mutex_);mesh_.store(session->original_preview);mesh_changed_.notify_all();}
          state_.error.clear();state_.message=changed?"Desktop stroke complete":"Desktop stroke unchanged";
          // Recovery cannot observe a candidate before all publication/model
          // allocations succeed. It is enqueued after the commit below.
          publish_locked(changed,false);
        } catch(...) {
          if(changed){scene_=std::move(before);undo_.pop_back();redo_.swap(previous_redo);}
          content_=previous_content;next_content_=previous_next;state_=std::move(previous_state);
          native_stroke_=session;desktop_=true;native_source_scene_=std::move(previous_source);
          bound_volume_=std::move(previous_bound);input_.refresh_volume(volume());last_volume_=previous_volume;last_paint_=previous_paint;
          published_.store(std::move(previous_published));
          {std::lock_guard mesh(mesh_mutex_);pending_mesh_.reset();mesh_.store(session->original_preview);mesh_changed_.notify_all();}
          throw;
        }
        if(changed)while(undo_.size()>128)undo_.pop_front();
        session->completed=std::move(immutable);session->revision=state_.revision;session->content=content_;
        // Recovery is best-effort persistence of an already committed endpoint,
        // and its failure must not roll back a successful user transaction.
        try{checkpoint_locked();}catch(...){}

      }
    }catch(const std::exception &e){error=e.what();std::lock_guard lock(model_mutex_);if(native_stroke_==session){cancel_native_locked();state_.error=error;publish_locked();}}
    catch(...){error="Unknown desktop stroke failure";std::lock_guard lock(model_mutex_);if(native_stroke_==session){cancel_native_locked();state_.error=error;publish_locked();}}
    {std::lock_guard lock(native_mutex_);session->error=std::move(error);session->done=true;native_processing_=false;}
    native_changed_.notify_all();
  }
}
} // namespace dome
