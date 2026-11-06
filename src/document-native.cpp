// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "dome/paint-sampling.hpp"
#include "dome/normal-preview.hpp"
#include "affine-surface.hpp"
#include "source-skin-world.hpp"
#include "document-held-affine.hpp"
#include "dome/polygon-scene.hpp"
#include "dome/mesh-processing.hpp"
#include "dome/material-visibility.hpp"
#include "document-transaction.hpp"
#include "dome/shapekey-sculpt.hpp"
#include "shapekey-preview.hpp"
#include "current-colors.hpp"
#include "texture-set-stroke.hpp"
#include "texture-set-worker.hpp"
#include <pthread.h>
namespace dome {
namespace {
Vec3 local(CanvasTransform t,Vec3 p){auto q=ob::transform(ob::inverse(t),{p.x,p.y,p.z});return {q.x,q.y,q.z};}
Vec3 point(CanvasTransform t,Vec3 p){auto q=ob::transform(t,{p.x,p.y,p.z});return {q.x,q.y,q.z};}
Vec3 rotate(ob::Quat q,Vec3 p){auto v=ob::rotate(q,{p.x,p.y,p.z});return {v.x,v.y,v.z};}
bool same(CanvasTransform a,CanvasTransform b){return a.position.x==b.position.x&&a.position.y==b.position.y&&a.position.z==b.position.z&&a.scale==b.scale&&a.orientation.x==b.orientation.x&&a.orientation.y==b.orientation.y&&a.orientation.z==b.orientation.z&&a.orientation.w==b.orientation.w;}
// Detached worker metadata only; no source parsing or live model lock is used.
std::optional<std::vector<Vec3>> source_world_positions(const Scene &scene,
    const SculptObject &object,const ShapeSculptDraft *shape,
    const std::function<bool()> &cancelled) {
  if(!object.painted||!object.deformation||!object.deformation->skin||!object.appearance)return {};
  const auto &rest=object.painted->mesh();constexpr size_t limit=256u*1024u*1024u;
  size_t bytes=0;auto charge=[&](size_t count,size_t stride){if(count>(limit-bytes)/stride)throw std::length_error("Native source world snapshot budget exceeded");bytes+=count*stride;};
  charge(rest.positions.size(),sizeof(Vec3));charge(rest.normals.size(),sizeof(Vec3));charge(rest.indices.size(),sizeof(uint32_t));charge(rest.uv.size(),sizeof(std::array<float,2>));charge(rest.uv_pins.size(),1);charge(rest.uv_seams.size(),1);
  ObjectSurface surface;surface.id=object.id;surface.transform=object.transform;
  surface.mesh=rest;surface.deformation=object.deformation;surface.groups=scene.group_path(object.group);
  surface.appearance=object.appearance;surface.affine_edit=object.affine_edit;surface.rig_source=object.rig_source;
  if(shape){
    auto deformation=std::make_shared<DeformationState>(*object.deformation);
    MorphSet draft(rest);draft.set_target(rest,"Sculpt draft",shape->positions());
    deformation->morphs=std::move(draft);deformation->morph_weights={1};surface.deformation=std::move(deformation);
  }
  return detail::source_skin_world_positions(surface,cancelled,limit-bytes);
}
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
  if(state_.rig_fit)return false;
  if(o.mesh_active&&!state_.shape_sculpt&&state_.projection&&state_.projection->loading)return false;
  if(native_stroke_){state_.message="Finish or cancel the current desktop stroke before starting another";publish_locked();return false;}
  if((o.polygon&&o.polygon->active&&!o.mesh_active)||desktop_||state_.polygon||state_.cut||state_.retopology||state_.weights.enabled||uv_||control_before_||state_.asset_busy||state_.recovery_busy||action_active_locked()||input_.menu_open()||!object_editable_locked(o.id)||(!o.mesh_active&&!volume().editable_layer()&&!volume().sculpt_settings().mask_edit))return false;
  if(!state_.shape_sculpt && reject_texture_paint_locked())return false;
  if(scene_.has_affine_placement(o.id)&&state_.object_grabbing&&(space!=NativeStrokeSpace::EvaluatedWorldBrush||!held_affine_||held_affine_->base_edit!=o.affine_edit))return false;
  if(scene_.has_affine_placement(o.id)&&!o.mesh_active){state_.message="Affine sculpt strokes require the full field metric path";publish_locked();return false;}
  auto preview=mesh_.load();
  if(!native_source_scene_||preview->generation!=state_.generation||preview->revision!=state_.revision||preview->pbr_view!=state_.pbr_view||preview->view_epoch!=state_.view_epoch){state_.message="Wait for the current surface before painting";publish_locked();return false;}
  const bool projected=o.mesh_active&&!state_.shape_sculpt&&state_.projection&&state_.projection->settings.enabled&&state_.projection->image;
  if(projected&&(!o.mesh_active||state_.shape_sculpt||state_.projection->placing))return false;
  auto field_lease=o.texture_sets?detail::capture_texture_set_stroke(scene_,o.id):nullptr;
  auto session=std::make_shared<NativeStroke>();session->texture_set=std::move(field_lease);session->epoch=++native_epoch_;
  if(projected){
    session->projection=state_.projection;
    session->projection_excluded.push_back(o.id);
    for(const auto &other:state_.objects)if(!other.visible||other.ghost)
      session->projection_excluded.push_back(other.id);
  }
  session->source={state_.generation,state_.revision,o.id,state_.edit_authority};if(projected||o.texture_sets||scene_.has_affine_placement(o.id)||space==NativeStrokeSpace::EvaluatedWorldBrush)session->affine_world=scene_.world_matrix(o.id);else session->transform=scene_.world_transform(o.id);session->deformation=o.deformation;
  session->initial=native_source_scene_;session->original_preview=preview;session->paint=state_.paint;session->spatial_owner=spatial_owner;session->space=space;session->menu_page=space==NativeStrokeSpace::Ray?24:3;
  if(projected)session->menu_page=25;
  session->pbr_view=state_.pbr_view;
  session->view_epoch=state_.view_epoch;
  if(state_.object_grabbing&&space==NativeStrokeSpace::EvaluatedWorldBrush)session->held_basis=held_affine_;
  if(state_.shape_sculpt&&space!=NativeStrokeSpace::Ray)return false;
  if(space==NativeStrokeSpace::EvaluatedWorldBrush&&!o.mesh_active)return false;
  if(state_.shape_sculpt&&o.texture_sets)throw std::logic_error("Texture-set shape editing requires its complete field/frame remap");
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
  CanvasTransform world_from_reference{};
  if(native_stroke_->held_basis){
    if(!held_affine_||held_affine_->base_edit!=native_stroke_->held_basis->base_edit)
      throw std::logic_error("Held paint reference changed during the stroke");
    world_from_reference=ob::compose(held_affine_->preview_pivot,ob::inverse(held_affine_->base_pivot));
  }
  bool overflow=false;
  {std::lock_guard queue(native_mutex_);auto &s=*native_stroke_;if(!s.accepting)return false;
    if(s.submitted>=4096)overflow=true;else{s.samples.push_back({std::move(sample),world_from_reference});++s.submitted;}}
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
        current->generation!=session->source.generation||current->revision!=session->source.revision||current->active_object!=session->source.object||current->edit_authority!=session->source.edit_authority||current->pbr_view!=session->pbr_view||current->view_epoch!=session->view_epoch;
    };
    std::exception_ptr failure;
    try{
      if(cancelled())throw std::runtime_error("Desktop stroke cancelled");
      Scene working=session->initial->snapshot();working.select(session->source.object);auto &o=working.active();
      if(session->texture_set){session->texture_set->authenticate_owner(session->texture_set);session->texture_set->authenticate_scene(working);}
      // The selected mutable draft is deliberately outside every published const
      // collection. Only sealed COW snapshots of this field are ever adopted.
      auto paint_target=session->texture_set?session->texture_set->paint()->snapshot():o.painted;
      size_t budget=4'000'000;if(!session->shape_target)for(const auto &other:working.objects())if(other.id!=o.id){auto used=std::max(other.volume->stored_samples(),other.volume->voxels().size());if(used>budget)throw std::length_error("Scene sculpt storage budget exceeded");budget-=used;}
      std::unique_ptr<MaterialSurfaceQuery> query;
      std::optional<SurfaceRange> query_range;
      std::vector<std::array<float,4>> query_colors;
      std::vector<std::array<float,2>> query_paint;
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
          if(auto positions=source_world_positions(working,o,shape.get(),cancelled))placed->positions=std::move(*positions);
          reflected=frame.orientation()==AffineOrientation::Reversing;geometry=std::move(placed);
        } else geometry=shape?std::make_shared<const Mesh>(shape->preview()):o.deformation?std::make_shared<const Mesh>(o.deformation->preview(o.painted->mesh())):std::shared_ptr<const Mesh>(o.painted,&o.painted->mesh());
        if(cancelled())throw std::runtime_error("Desktop stroke cancelled");
        if(!shape&&(o.deformation||session->affine_world))evaluation=paint_target->evaluate(geometry,cancelled);
        if(session->texture_set){
          TextureSetLimits limits;limits.cancelled=cancelled;ObjectSurface bundle;bundle.id=o.id;bundle.name=o.name;bundle.transform=o.transform;bundle.material=o.material;bundle.deformation=o.deformation;bundle.groups=working.group_path(o.group);bundle.affine_edit=o.affine_edit;bundle.rig_source=o.rig_source;bundle.texture_set_surface=detail::prepare_texture_set_surface(o,limits);
          auto layout=detail::prepare_texture_set_packed(bundle.texture_set_surface,limits);MeshSnapshot query_snapshot;detail::TextureSetWorkerView view;view.pbr=session->pbr_view;detail::append_texture_set_surface(query_snapshot,bundle,layout,view,limits);
          query=std::make_unique<MaterialSurfaceQuery>(query_snapshot.mesh,query_snapshot.ranges,cancelled,query_snapshot.source_colors,PaintVertexCoordinates{query_snapshot.paint_coordinates});
        }else{
        query_range.emplace(SurfaceRange{o.id,0,geometry->indices.size(),o.material,o.painted->preview(o.material,cancelled),o.appearance,0,o.material_bindings});
        auto &range=*query_range;
        range.reflected=reflected;
        // This query keeps logical REST triangle order. place_mesh transforms
        // positions/normals without reindexing; only the packed render worker
        // reverses corners for reflected placement.
        range.paint_winding_reversed=false;
        if(o.painted->paint_sampling_domain()) {
          range.paint_sampling_attachment=o.painted->paint_sampling_attachment();
          range.normal_preview_proof=prepare_normal_preview(*o.painted,o.material_bindings,o.material,cancelled);
          PaintSamplingLimits limits;limits.cancelled=cancelled;
          query_paint=o.painted->paint_sampling_domain()->corner_coordinates(o.painted->mesh(),limits);
        }
        range.pbr_view=session->pbr_view;
        PolygonColorSurface current_colors;
        if(o.polygon && o.polygon->active && o.polygon->colors) {
          current_colors.topology=std::shared_ptr<const PolygonMesh>(o.polygon,&o.polygon->topology);
          current_colors.field=o.polygon->colors;
        }
        query_colors=detail::current_vertex_colors(o.painted->mesh(),
            current_colors.field?&current_colors:nullptr,o.appearance.get(),
            o.material_bindings.get(),range.textures.get(),cancelled);
        query=std::make_unique<MaterialSurfaceQuery>(*geometry,std::span<const SurfaceRange>(&range,1),cancelled,query_colors,PaintCornerCoordinates{query_paint});
        }
        if(!shape)paint_target->begin_stroke();
      }else o.volume->begin_stroke();
      std::shared_ptr<const MaterialSurfaceQuery> scene_query;
      std::optional<PaintProjection> world_projector;
      if(session->projection){
        auto options=session->projection->settings.image;options.visibility=false;options.cancelled=cancelled;
        world_projector=PaintProjection::from_shared_image(session->projection->image->pixels,
            session->projection->settings.camera,Mesh{},std::move(options));
        if(!evaluation||!query)throw std::logic_error("Projection requires evaluated native geometry");
        if(session->projection->settings.image.visibility&&session->projection->settings.occlusion==ProjectionOcclusion::VisibleScene&&
           std::any_of(session->original_preview->ranges.begin(),session->original_preview->ranges.end(),[&](const auto &range){return range.count&&std::find(session->projection_excluded.begin(),session->projection_excluded.end(),range.object)==session->projection_excluded.end();})){
          scene_query=session->original_preview->picking;
          if(!scene_query)scene_query=std::make_shared<MaterialSurfaceQuery>(session->original_preview->mesh,
              session->original_preview->ranges,cancelled,session->original_preview->source_colors,PaintVertexCoordinates{session->original_preview->paint_coordinates});
        }
      }
      std::optional<Vec3> previous;size_t processed=0;auto shown=std::chrono::steady_clock::now();
      for(;;){
        std::optional<NativeStrokeSample> sample;CanvasTransform world_from_reference{};bool accept=false;
        {std::unique_lock lock(native_mutex_);native_changed_.wait(lock,[&]{return cancelled()||!session->samples.empty()||!session->accepting;});
          if(cancelled())throw std::runtime_error("Desktop stroke cancelled");
          if(!session->samples.empty()){sample=std::move(session->samples.front().input);world_from_reference=session->samples.front().world_from_reference;session->samples.pop_front();}else accept=session->accepted.load();}
        if(!sample){if(accept)break;continue;}
        auto paint=[&](const Brush &brush){
          if(!world_projector){
            if(evaluation)paint_target->stamp_evaluated(brush,session->paint,*evaluation,cancelled);
            else paint_target->stamp(brush,session->paint,cancelled);
            return;
          }
          auto camera=session->projection->settings.camera;
          camera.pose=ob::compose(ob::inverse(world_from_reference),camera.pose);
          auto projector=world_projector->with_camera(camera);
          const auto &settings=session->projection->settings;
          const float target_epsilon=settings.image.visibility_epsilon/world_from_reference.scale;
          uint64_t samples=0;
          paint_target->stamp_sampled(brush,session->paint,[&](Vec3 p,Vec3 n)->std::optional<PaintedMesh::PaintSample>{
            if(++samples>projector.max_samples_per_stamp())throw std::length_error("Projection stamp sample budget exceeded");
            auto projected=projector.sample(p,n,session->paint);if(!projected)return {};
            if(settings.image.visibility){
              const auto ray=projector.camera_ray(p);
              auto hit=query->raycast(ray.origin,ray.direction,ray.distance+target_epsilon,sample->material_time);
              // Transparent target texels remain paintable. Only a nearer,
              // material-visible target face can occlude this candidate.
              if(hit&&hit->distance+target_epsilon<ray.distance)return {};
              if(scene_query){
                const auto world=point(world_from_reference,p);const auto scene_ray=world_projector->camera_ray(world);
                auto occluder=scene_query->raycast_excluding(scene_ray.origin,scene_ray.direction,
                    scene_ray.distance+settings.image.visibility_epsilon,sample->material_time,session->projection_excluded);
                if(occluder&&occluder->distance+settings.image.visibility_epsilon<scene_ray.distance)return {};
              }
            }
            return PaintedMesh::PaintSample{projected->settings,projected->opacity};
          },cancelled,evaluation.get());
        };
        if(sample->space==NativeStrokeSpace::EvaluatedWorldBrush){
          if(!evaluation||shape)throw std::logic_error("Evaluated-world contact painting requires an immutable evaluated mesh");
          paint(sample->brush);
        }else {
        auto origin=session->affine_world?sample->origin:local(session->transform,sample->origin);auto direction=session->affine_world?sample->direction:rotate(ob::conjugate(session->transform.orientation),sample->direction);
        std::optional<Vec3> hit;Vec3 normal;
        if(query){auto picked=query->raycast(origin,direction,session->affine_world?10:10/session->transform.scale,sample->material_time);if(picked&&(!session->texture_set||query->material_identity(picked->triangle)==SurfaceMaterialIdentity{o.id,session->texture_set->set()})){hit=picked->position;normal=picked->normal;}}
        else {hit=o.volume->raycast(origin,direction,10/session->transform.scale,cancelled);if(hit)normal=o.volume->gradient(*hit);}
        if(hit){auto brush=sample->brush;brush.from=previous.value_or(*hit);brush.to=*hit;brush.normal=normal;if(!session->affine_world){brush.radius/=session->transform.scale;brush.blend/=session->transform.scale;}
          if(shape){auto world=sample->brush;world.from=session->affine_world?previous.value_or(*hit):point(session->transform,previous.value_or(*hit));world.to=session->affine_world?*hit:point(session->transform,*hit);world.normal=session->affine_world?normal:rotate(session->transform.orientation,normal);shape->stamp(world,cancelled);}
          else if(o.mesh_active){paint(brush);}else o.volume->stamp(brush,budget,cancelled);previous=*hit;
        }else previous.reset();
        }
        ++processed;
        if(cancelled())throw std::runtime_error("Desktop stroke cancelled");
        auto now=std::chrono::steady_clock::now();
        if(now-shown>=std::chrono::milliseconds(30)){
          auto preview=working.snapshot();
          if(session->texture_set){TextureSetLimits limits;limits.cancelled=cancelled;detail::adopt_texture_set_stroke(preview,session->texture_set,*paint_target,limits);}
          if(shape){ShapeKeyEdit edit;edit.action=ShapeKeyAction::Replace;edit.target=*session->shape_target;edit.positions.assign(shape->positions().begin(),shape->positions().end());author_shapekey(preview,o.id,edit,cancelled);detail::shape_sculpt_preview(preview,o.id,*session->shape_target);
            auto geometry=shape->preview();auto frame=detail::placement_frame(working,o.id);if(session->affine_world){detail::place_mesh(geometry,frame,cancelled);if(auto positions=source_world_positions(working,o,shape.get(),cancelled))geometry.positions=std::move(*positions);}
            // Only evaluated geometry changes during this gesture. Preserve
            // the original immutable material, sampling attachment/proof and
            // its explicitly logical-corner paint stream for every refresh.
            if(!query_range||geometry.indices.size()!=query_range->count)throw std::logic_error("Shape preview changed its visibility triangle correspondence");
            query=std::make_unique<MaterialSurfaceQuery>(geometry,std::span<const SurfaceRange>(&*query_range,1),cancelled,query_colors,PaintCornerCoordinates{query_paint});
          }
          std::lock_guard lock(model_mutex_);
          if(native_stroke_!=session||session->source.generation!=state_.generation||session->source.revision!=state_.revision||scene_.active().id!=session->source.object||scene_.active().deformation!=session->deformation||!same_placement(scene_,session->source.object,session->transform,session->affine_world))throw std::runtime_error("Desktop source or pose changed");
          if(session->texture_set)session->texture_set->authenticate_scene(scene_);
          state_.native_samples=processed;publish_locked();
          {std::lock_guard mesh(mesh_mutex_);pending_mesh_.emplace(MeshJob{state_.revision,state_.generation,std::move(preview),false,true,0,0,{}, {}});pending_mesh_->sculpt_mask_view=state_.sculpt_mask_visible;pending_mesh_->native_gesture=session->epoch;pending_mesh_->native_samples=processed;pending_mesh_->pbr_view=session->pbr_view;pending_mesh_->view_epoch=session->view_epoch;}
          mesh_changed_.notify_all();shown=now;
        }
      }
      if(cancelled())throw std::runtime_error("Desktop stroke cancelled");
      bool changed;
      if(shape){ShapeKeyEdit edit;edit.action=ShapeKeyAction::Replace;edit.target=*session->shape_target;edit.positions.assign(shape->positions().begin(),shape->positions().end());changed=author_shapekey(working,o.id,edit,cancelled).changed;}
      else changed=o.mesh_active?paint_target->end_stroke():o.volume->end_stroke();
      if(changed&&session->texture_set){TextureSetLimits limits;limits.cancelled=cancelled;detail::adopt_texture_set_stroke(working,session->texture_set,*paint_target,limits);}
      std::deque<History> previous_redo;
      {std::lock_guard lock(model_mutex_);
        if(cancelled()||native_stroke_!=session||session->source.generation!=state_.generation||session->source.revision!=state_.revision||scene_.active().id!=session->source.object||scene_.active().deformation!=session->deformation||!same_placement(scene_,session->source.object,session->transform,session->affine_world))throw std::runtime_error("Desktop source or pose changed");
        if(session->texture_set)session->texture_set->authenticate_scene(scene_);
        const bool release_held=session->release_held.load();
        if(release_held){
          if(!state_.object_grabbing||state_.held_object!=session->source.object||!held_affine_||!session->held_basis||held_affine_->base_edit!=session->held_basis->base_edit)throw std::runtime_error("Held contact source placement changed");
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
