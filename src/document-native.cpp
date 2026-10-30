// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "affine-surface.hpp"
#include "document-held-affine.hpp"
#include "dome/polygon-scene.hpp"
#include "dome/mesh-processing.hpp"
#include "dome/material-visibility.hpp"
#include "document-transaction.hpp"
#include "dome/shapekey-sculpt.hpp"
#include "shapekey-preview.hpp"
#include "current-colors.hpp"
#include <pthread.h>
namespace dome {
namespace {
Vec3 local(CanvasTransform t,Vec3 p){auto q=ob::transform(ob::inverse(t),{p.x,p.y,p.z});return {q.x,q.y,q.z};}
Vec3 point(CanvasTransform t,Vec3 p){auto q=ob::transform(t,{p.x,p.y,p.z});return {q.x,q.y,q.z};}
Vec3 rotate(ob::Quat q,Vec3 p){auto v=ob::rotate(q,{p.x,p.y,p.z});return {v.x,v.y,v.z};}
bool same(CanvasTransform a,CanvasTransform b){return a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z&&a.scale==b.scale&&a.orientation.x==b.orientation.x&&a.orientation.y==b.orientation.y&&a.orientation.z==b.orientation.z&&a.orientation.w==b.orientation.w;}
bool same_placement(const Scene &scene,uint64_t id,CanvasTransform legacy,const std::optional<AffineTransform>&affine) {
  if(affine)return scene.world_matrix(id).matrix()==affine->matrix();
  return !scene.has_affine_placement(id)&&same(scene.world_transform(id),legacy);
}
}
bool Document::begin_native_stroke(){
  std::lock_guard lock(model_mutex_);return begin_native_locked();
}
bool Document::begin_native_locked(uint32_t spatial_owner,NativeStrokeSpace space){
  auto &o=scene_.active();
  if(native_stroke_){state_.message="Finish or cancel the current desktop stroke before starting another";publish_locked();return false;}
  if((o.polygon&&o.polygon->active&&!o.mesh_active)||desktop_||state_.polygon||state_.cut||state_.retopology||state_.weights.enabled||uv_||control_before_||state_.asset_busy||state_.recovery_busy||action_active_locked()||input_.menu_open()||!object_editable_locked(o.id)||(!o.mesh_active&&!volume().editable_layer()&&!volume().sculpt_settings().mask_edit))return false;
  if(scene_.has_affine_placement(o.id)&&state_.object_grabbing&&(space!=NativeStrokeSpace::EvaluatedWorldBrush||!held_affine_||held_affine_->base_edit!=o.affine_edit))return false;
  if(scene_.has_affine_placement(o.id)&&!o.mesh_active){state_.message="Affine sculpt strokes require the full field metric path";publish_locked();return false;}
  auto preview=mesh_.load();
  if(!native_source_scene_||preview->generation!=state_.generation||preview->revision!=state_.revision){state_.message="Wait for the current surface before painting";publish_locked();return false;}
  auto session=std::make_shared<NativeStroke>();session->epoch=++native_epoch_;
  session->source={state_.generation,state_.revision,o.id,state_.edit_authority};if(scene_.has_affine_placement(o.id)||space==NativeStrokeSpace::EvaluatedWorldBrush)session->affine_world=scene_.world_matrix(o.id);else session->transform=scene_.world_transform(o.id);session->deformation=o.deformation;
  session->initial=native_source_scene_;session->original_preview=preview;session->paint=state_.paint;session->spatial_owner=spatial_owner;session->space=space;session->menu_page=space==NativeStrokeSpace::Ray?24:3;
  if(state_.object_grabbing&&space==NativeStrokeSpace::EvaluatedWorldBrush)session->held_basis=held_affine_;
  if(state_.shape_sculpt&&space!=NativeStrokeSpace::Ray)return false;
  if(space==NativeStrokeSpace::EvaluatedWorldBrush&&!o.mesh_active)return false;
  if(state_.shape_sculpt){if(!o.deformation||!o.deformation->morphs||state_.morph_target>=o.deformation->morphs->targets.size()||state_.object_grabbing)return false;session->shape_target=state_.morph_target;}
  std::lock_guard queue(native_mutex_);
  if(native_jobs_.size()>=8){state_.error="Desktop gesture queue is full; retry after pending cancellation finishes";publish_locked();return false;}
  Transaction transaction(*this);native_jobs_.push_back(session);
  try{
    input_.cancel();native_stroke_=std::move(session);desktop_=spatial_owner==0;state_.error.clear();state_.message="Desktop stroke";state_.native_samples=0;
    publish_locked(false,false);transaction.commit();
  }catch(...){native_jobs_.pop_back();throw;}
  native_changed_.notify_all();return true;

}
bool Document::sample_native_stroke(NativeStrokeSample sample){
  std::lock_guard lock(model_mutex_);return sample_native_locked(std::move(sample));
}
bool Document::sample_native_locked(NativeStrokeSample sample){
  if(!std::isfinite(sample.material_time)||!finite(sample.origin)||!finite(sample.direction)||length(sample.direction)<1e-8f||!std::isfinite(sample.brush.radius)||sample.brush.radius<=0||sample.brush.radius>5||!std::isfinite(sample.brush.pressure)||sample.brush.pressure<0||sample.brush.pressure>1)throw std::invalid_argument("Invalid native stroke sample");
  if(!native_stroke_)return false;
  if(sample.space!=native_stroke_->space)throw std::invalid_argument("Native sample coordinate space changed during gesture");
  if(sample.space==NativeStrokeSpace::EvaluatedWorldBrush&&(!finite(sample.brush.from)||!finite(sample.brush.to)||!finite(sample.brush.normal)||length(sample.brush.normal)<1e-8f))throw std::invalid_argument("Invalid evaluated-world brush capsule");
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
    if(session->failure)std::rethrow_exception(session->failure);
    job.revision=session->revision;job.generation=session->source.generation;job.content=session->content;job.recovery_source=session->recovery_source;}
  // This immutable completion is the exact accepted endpoint, even if another
  // gesture or object change has already started before the ordered file job.
  job.scene=session->completed->snapshot();job.native_capture.reset();
}
void Document::native_loop(){
  pthread_setname_np(pthread_self(),"dome-native");
  for(;;){
    if(stopping_.load()&&native_notice_pending_.load()){std::lock_guard lock(model_mutex_);worker_notices_[2].reset();signal_worker_notices_locked();}
    if(native_notice_pending_.load()){
      {std::lock_guard lock(model_mutex_);retry_worker_notices_locked(false,true);}
      if(stopping_.load()&&native_notice_pending_.load()){std::lock_guard lock(model_mutex_);worker_notices_[2].reset();signal_worker_notices_locked();}
      if(native_notice_pending_.load()){
        std::unique_lock lock(native_mutex_);native_changed_.wait_for(lock,std::chrono::milliseconds(25),[&]{return stopping_.load()||!native_notice_pending_.load();});
        if(stopping_)continue;
        continue;
      }
    }
    std::shared_ptr<NativeStroke> session;
    {std::unique_lock lock(native_mutex_);native_changed_.wait(lock,[&]{return stopping_||!native_jobs_.empty()||native_notice_pending_.load();});if(stopping_&&native_jobs_.empty())return;if(native_notice_pending_)continue;session=std::move(native_jobs_.front());native_jobs_.pop_front();native_processing_=true;}
    auto cancelled=[&]{
      auto current=published_.load();
      return stopping_.load()||session->cancelled.load()||native_epoch_.load()!=session->epoch||
        current->generation!=session->source.generation||current->revision!=session->source.revision||current->active_object!=session->source.object||current->edit_authority!=session->source.edit_authority;
    };
    std::exception_ptr failure;
    try{
      if(cancelled())throw std::runtime_error("Desktop stroke cancelled");
      Scene working=session->initial->snapshot();working.select(session->source.object);auto &o=working.active();
      size_t budget=4'000'000;if(!session->shape_target)for(const auto &other:working.objects())if(other.id!=o.id){auto used=std::max(other.volume->stored_samples(),other.volume->voxels().size());if(used>budget)throw std::length_error("Scene sculpt storage budget exceeded");budget-=used;}
      std::unique_ptr<MaterialSurfaceQuery> query;
      std::vector<std::array<float,4>> query_colors;
      std::unique_ptr<ShapeSculptDraft> shape;
      std::shared_ptr<const PaintedMesh::Evaluation> evaluation;
      if(session->shape_target){
        const auto &t=session->transform;RigMatrix world=Quat{t.orientation.w,t.orientation.x,t.orientation.y,t.orientation.z}.matrix();
        for(size_t c=0;c<3;++c)for(size_t r=0;r<3;++r)world[c*4+r]*=t.scale;
        world[12]=t.position.x;world[13]=t.position.y;world[14]=t.position.z;
        if(session->affine_world)world=session->affine_world->float_matrix();
        shape=std::make_unique<ShapeSculptDraft>(o.painted,o.deformation,*session->shape_target,world,ShapeSculptOptions{},cancelled);
      }
      if(o.mesh_active){
        std::shared_ptr<const Mesh> geometry;
        bool reflected=false;
        if(session->affine_world) {
          auto placed=std::make_shared<Mesh>(shape?shape->preview():o.deformation?o.deformation->preview(o.painted->mesh()):o.painted->mesh());
          auto frame=detail::placement_frame(working,o.id);detail::place_mesh(*placed,frame,cancelled);
          reflected=frame.orientation()==AffineOrientation::Reversing;geometry=std::move(placed);
        } else geometry=shape?std::make_shared<const Mesh>(shape->preview()):o.deformation?std::make_shared<const Mesh>(o.deformation->preview(o.painted->mesh())):std::shared_ptr<const Mesh>(o.painted,&o.painted->mesh());
        if(cancelled())throw std::runtime_error("Desktop stroke cancelled");
        if(!shape&&(o.deformation||session->affine_world))evaluation=o.painted->evaluate(geometry,cancelled);
        SurfaceRange range{o.id,0,geometry->indices.size(),o.material,o.painted->preview(o.material),o.appearance,0,o.material_bindings};
        range.reflected=reflected;
        PolygonColorSurface current_colors;
        if(o.polygon && o.polygon->active && o.polygon->colors) {
          current_colors.topology=std::shared_ptr<const PolygonMesh>(o.polygon,&o.polygon->topology);
          current_colors.field=o.polygon->colors;
        }
        query_colors=detail::current_vertex_colors(o.painted->mesh(),
            current_colors.field?&current_colors:nullptr,o.appearance.get(),
            o.material_bindings.get(),range.textures.get(),cancelled);
        query=std::make_unique<MaterialSurfaceQuery>(*geometry,std::span<const SurfaceRange>(&range,1),cancelled,query_colors);if(!shape)o.painted->begin_stroke();
      }else o.volume->begin_stroke();
      std::optional<Vec3> previous;size_t processed=0;auto shown=std::chrono::steady_clock::now();
      for(;;){
        std::optional<NativeStrokeSample> sample;bool accept=false;
        {std::unique_lock lock(native_mutex_);native_changed_.wait(lock,[&]{return cancelled()||!session->samples.empty()||!session->accepting;});
          if(cancelled())throw std::runtime_error("Desktop stroke cancelled");
          if(!session->samples.empty()){sample=std::move(session->samples.front());session->samples.pop_front();}else accept=session->accepted.load();}
        if(!sample){if(accept)break;continue;}
        if(sample->space==NativeStrokeSpace::EvaluatedWorldBrush){
          if(!evaluation||shape)throw std::logic_error("Evaluated-world contact painting requires an immutable evaluated mesh");
          o.painted->stamp_evaluated(sample->brush,session->paint,*evaluation,cancelled);
        }else {
        auto origin=session->affine_world?sample->origin:local(session->transform,sample->origin);auto direction=session->affine_world?sample->direction:rotate(ob::conjugate(session->transform.orientation),sample->direction);
        std::optional<Vec3> hit;Vec3 normal;
        if(query){auto picked=query->raycast(origin,direction,session->affine_world?10:10/session->transform.scale,sample->material_time);if(picked){hit=picked->position;normal=picked->normal;}}
        else {hit=o.volume->raycast(origin,direction,10/session->transform.scale,cancelled);if(hit)normal=o.volume->gradient(*hit);}
        if(hit){auto brush=sample->brush;brush.from=previous.value_or(*hit);brush.to=*hit;brush.normal=normal;if(!session->affine_world){brush.radius/=session->transform.scale;brush.blend/=session->transform.scale;}
          if(shape){auto world=sample->brush;world.from=session->affine_world?previous.value_or(*hit):point(session->transform,previous.value_or(*hit));world.to=session->affine_world?*hit:point(session->transform,*hit);world.normal=session->affine_world?normal:rotate(session->transform.orientation,normal);shape->stamp(world,cancelled);}
          else if(o.mesh_active){if(evaluation)o.painted->stamp_evaluated(brush,session->paint,*evaluation,cancelled);else o.painted->stamp(brush,session->paint,cancelled);}else o.volume->stamp(brush,budget,cancelled);previous=*hit;
        }else previous.reset();
        }
        ++processed;
        if(cancelled())throw std::runtime_error("Desktop stroke cancelled");
        auto now=std::chrono::steady_clock::now();
        if(now-shown>=std::chrono::milliseconds(30)){
          auto preview=working.snapshot();
          if(shape){ShapeKeyEdit edit;edit.action=ShapeKeyAction::Replace;edit.target=*session->shape_target;edit.positions.assign(shape->positions().begin(),shape->positions().end());author_shapekey(preview,o.id,edit,cancelled);detail::shape_sculpt_preview(preview,o.id,*session->shape_target);
            auto geometry=shape->preview();auto frame=detail::placement_frame(working,o.id);if(session->affine_world)detail::place_mesh(geometry,frame,cancelled);SurfaceRange range{o.id,0,geometry.indices.size(),o.material,o.painted->preview(o.material),o.appearance,0,o.material_bindings};range.reflected=session->affine_world&&frame.orientation()==AffineOrientation::Reversing;query=std::make_unique<MaterialSurfaceQuery>(geometry,std::span<const SurfaceRange>(&range,1),cancelled,query_colors);
          }
          std::lock_guard lock(model_mutex_);
          if(native_stroke_!=session||session->source.generation!=state_.generation||session->source.revision!=state_.revision||scene_.active().id!=session->source.object||scene_.active().deformation!=session->deformation||!same_placement(scene_,session->source.object,session->transform,session->affine_world))throw std::runtime_error("Desktop source or pose changed");
          state_.native_samples=processed;publish_locked();
          {std::lock_guard mesh(mesh_mutex_);pending_mesh_.emplace(MeshJob{state_.revision,state_.generation,std::move(preview),false,true,0,0,{}, {}});pending_mesh_->sculpt_mask_view=state_.sculpt_mask_visible;pending_mesh_->native_gesture=session->epoch;pending_mesh_->native_samples=processed;}
          mesh_changed_.notify_all();shown=now;
        }
      }
      if(cancelled())throw std::runtime_error("Desktop stroke cancelled");
      bool changed;
      if(shape){ShapeKeyEdit edit;edit.action=ShapeKeyAction::Replace;edit.target=*session->shape_target;edit.positions.assign(shape->positions().begin(),shape->positions().end());changed=author_shapekey(working,o.id,edit,cancelled).changed;}
      else changed=o.mesh_active?o.painted->end_stroke():o.volume->end_stroke();
      std::deque<History> previous_redo;
      {std::lock_guard lock(model_mutex_);
        if(cancelled()||native_stroke_!=session||session->source.generation!=state_.generation||session->source.revision!=state_.revision||scene_.active().id!=session->source.object||scene_.active().deformation!=session->deformation||!same_placement(scene_,session->source.object,session->transform,session->affine_world))throw std::runtime_error("Desktop source or pose changed");
        const bool release_held=session->release_held.load();
        if(release_held){
          if(!state_.object_grabbing||state_.held_object!=o.id||!held_affine_||!session->held_basis||held_affine_->base_edit!=session->held_basis->base_edit)throw std::runtime_error("Held contact source placement changed");
          if(state_.edit_authority==UINT64_MAX)throw std::overflow_error("Object editing authority exhausted");
          working.active().transform=held_preview_transform_;working.active().affine_edit=held_affine_->preview_edit;
          changed=changed||working.active().affine_edit!=scene_.active().affine_edit;
        }
        auto immutable=std::make_shared<const Scene>(working.snapshot());
        const auto previous_held=held_affine_;const auto previous_navigation=object_navigation_;
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
          if(release_held){state_.object_grabbing=false;state_.held_object=0;++state_.edit_authority;held_affine_.reset();object_navigation_.reset();}
          if(changed){scene_=std::move(working);content_=next_content_++;bind_locked();}
          else {std::lock_guard mesh(mesh_mutex_);mesh_.store(session->original_preview);mesh_changed_.notify_all();}
          if(session->menu_after.load()&&session->spatial_owner&&state_.hands[session->spatial_owner-1].tracked.valid)input_.retain_page(session->menu_page,session->spatial_owner,session->menu_page==24?11:0);
          state_.error.clear();state_.message=changed?"Stroke complete":"Stroke unchanged";
          // Recovery cannot observe a candidate before all publication/model
          // allocations succeed. It is enqueued after the commit below.
          publish_locked(changed,false);
        } catch(...) {
          if(changed){scene_=std::move(before);undo_.pop_back();redo_.swap(previous_redo);}
          content_=previous_content;next_content_=previous_next;state_=std::move(previous_state);
          native_stroke_=session;desktop_=session->spatial_owner==0;held_affine_=previous_held;object_navigation_=previous_navigation;native_source_scene_=std::move(previous_source);
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
    }catch(...){failure=std::current_exception();std::lock_guard lock(model_mutex_);native_worker_error_locked(session,failure);}
    {std::lock_guard lock(native_mutex_);session->failure=std::move(failure);session->done=true;native_processing_=false;}
    native_changed_.notify_all();
  }
}
} // namespace dome
