// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "polygon-conversion.hpp"
#include "dome/humanoid-creator.hpp"
#include "dome/source-uv-bake.hpp"
#include "dome/color-paint-sampling.hpp"
#include "dome/normal-sampling.hpp"
#include "dome/normal-preparation.hpp"
#include "normal-sampling-access.hpp"
#include "normal-frame-runtime.hpp"
#include "paint-sampling-access.hpp"
#include <new>
#include "document-transaction.hpp"
namespace dome {
namespace {
bool heavy_uv(UvAction action) {
  return action == UvAction::Conformal || action == UvAction::Stitch ||
         action == UvAction::Relax || action == UvAction::Pack || action == UvAction::Seam;
}
}
bool Document::uv_retention_requested_locked() const {
  // The UV workspace has no numeric fourth component. Keep this draft-only
  // option in the existing transactional control snapshot, never in Scene.
  return state_.controls.parameter==VrParameter::UvEdit&&state_.controls.value[3]>.5f;
}
bool Document::uv_paint_retention_requested_locked() const {
  return state_.controls.parameter==VrParameter::UvEdit&&state_.controls.value[3]>1.5f;
}
bool Document::uv_paint_retention_available_locked() const {
  if(!control_before_||!control_object_)return false;
  const auto &o=scene_.object(control_object_);const auto b=o.material_bindings;
  return o.mesh_active&&o.painted&&!o.appearance&&b&&!b->has_mtoon&&b->normal_preparation&&b->color_preparation&&
    o.painted->normal_projection()&&o.painted->color_projection()&&o.polygon&&o.polygon->active&&o.polygon->colors&&
    b->color_preparation->receipt.source_field==SourceColorField::PolygonCorners&&
    b->color_preparation->current_descriptor()&&b->color_preparation->current_descriptor()->field==o.polygon->colors&&
    (!o.painted->source_maps()||!o.painted->source_maps()->uv1);
}
bool Document::uv_retention_changes_locked() const {
  if(!uv_retention_requested_locked()||!control_before_)return false;
  const auto &o=scene_.object(control_object_);
  return o.painted&&((uv_paint_retention_requested_locked()&&!o.painted->paint_sampling_domain())||(o.painted->normal_projection()&&o.painted->normal_projection()->version==1));
}
void Document::edit_uv(UvAction action, float x, float y) {
  std::lock_guard lock(model_mutex_);
  if(action==UvAction::RetainNormals||action==UvAction::RetainPaint){
    Transaction transaction(*this);apply_uv_action_locked(action,x,y);publish_locked(false,false);transaction.commit(false);return;
  }
  try { apply_uv_action_locked(action, x, y); }
  catch (...) { publish_locked(); throw; }
  publish_locked();
}
void Document::apply_uv_action_locked(UvAction action, float x, float y) {
  if (!uv_) throw std::logic_error("Open the UV editor first");
  if (uv_accept_pending_) throw std::logic_error("Wait for the accepted UV edit to finish");
  if (uv_solving_) throw std::logic_error("Wait for UV processing, or cancel the UV editor");
  try {
    if(action==UvAction::RetainPaint) {
      const auto &o=scene_.object(control_object_);
      if(!uv_paint_retention_available_locked())throw std::logic_error("Keep paint detail requires this mesh's own supported prepared corner colours and normal detail");
      if(o.painted->paint_sampling_domain())throw std::logic_error("This project uses retained paint coordinates; restore its source before choosing a different transfer");
      if(!uv_->view().topology||uv_->view().validating)throw std::logic_error("Wait for UV validation before changing paint detail");
      state_.controls.value[3]=uv_paint_retention_requested_locked()?(o.painted->normal_projection()->version==2?1.f:0.f):2.f;
      state_.message=uv_paint_retention_requested_locked()?"Paint and normal detail keep their existing coordinates. The UV layout remains editable.":"Paint detail will be transferred into the edited UV layout";
    }
    else if(action==UvAction::RetainNormals){
      if(uv_paint_retention_requested_locked())throw std::logic_error("Turn off Keep paint detail before changing normal sampling");
      const auto &o=scene_.object(control_object_);
      if(!o.painted||!o.painted->normal_projection()||!o.material_bindings||!o.material_bindings->normal_preparation)
        throw std::logic_error("Prepare normal detail in the Bake controls first");
      if(o.painted->normal_projection()->version==2)
        throw std::logic_error("This mesh already keeps its original normal detail; restore an earlier source to resample it");
      if(!uv_->view().topology||uv_->view().validating)
        throw std::logic_error("Wait for UV validation before changing normal detail");
      state_.controls.value[3]=uv_retention_requested_locked()?0.f:1.f;
      state_.message=uv_retention_requested_locked()
        ?"Original normal detail will be kept. Save the native project; glTF/VRM export still needs a sampling conversion."
        :"Normal detail will be resampled into the edited UVs";
    }
    else if (heavy_uv(action)) queue_uv_operation_locked(action);
    else uv_->edit(action, x, y);
    state_.error.clear();
  } catch (const std::exception &error) {
    state_.error = error.what();
    throw;
  }
}
void Document::queue_uv_operation_locked(UvAction operation) {
  uv_->edit(UvAction::EndGesture);
  invalidate_uv_locked();
  auto mesh = uv_->view().mesh;
  UvValidationJob job{++uv_request_, uv_session_, state_.generation, mesh,
                      operation, std::make_shared<UvWorkspace>(*uv_)};
  uv_solving_ = true;
  uv_scheduled_ = mesh;
  state_.message = operation == UvAction::Conformal ? "Unwrapping selected UV chart..."
      : operation == UvAction::Stitch ? "Stitching selected UV seam..." : "Processing UV charts...";
  std::lock_guard lock(uv_mutex_);
  uv_validation_ = std::move(job);
  uv_changed_.notify_all();
}
bool Document::wait_uv(std::chrono::milliseconds timeout) const {
  std::unique_lock lock(uv_mutex_);
  return uv_changed_.wait_for(lock, timeout, [&] {
    auto state = published_.load();
    return !state->uv || (!state->uv->validating && !state->uv->committing && !state->uv->solving);
  });
}
void Document::invalidate_uv_locked() {
  ++uv_request_;
  uv_solving_ = false;
  uv_scheduled_.reset();
  std::lock_guard lock(uv_mutex_);
  uv_validation_.reset();
  uv_changed_.notify_all();
}
std::optional<Document::UvValidationJob> Document::prepare_uv_schedule_locked() {
  if (!uv_ || uv_accept_pending_ || uv_solving_ || !uv_->view().validating ||
      uv_scheduled_.lock() == uv_->view().mesh) return {};
  auto mesh=uv_->view().mesh;
  return UvValidationJob{0,uv_session_,state_.generation,mesh,{},
      !uv_->view().topology?std::make_shared<UvWorkspace>(*uv_):nullptr};
}
void Document::uv_loop() {
  for (;;) {
    UvValidationJob job;
    {
      std::unique_lock lock(uv_mutex_);
      uv_changed_.wait(lock, [&] { return stopping_ || uv_validation_; });
      if (stopping_) return;
      job = std::move(*uv_validation_);
      uv_validation_.reset();
    }
    auto cancelled = [&] { return stopping_ || uv_request_ != job.request; };
    std::string error;
    bool completed = false;
    const bool workspace_job = bool(job.workspace);
    const bool initializing = workspace_job && !job.operation;
    try {
      if (workspace_job) {
        if (job.operation) job.workspace->edit(*job.operation, 0, 0, cancelled);
        else job.workspace->initialize(cancelled);
        auto result = job.workspace->view().mesh;
        error = validate_uv(*result, cancelled);
        job.workspace->validated(result, error);
        completed = true;
      } else error = validate_uv(*job.mesh, cancelled);
    } catch (const std::bad_alloc &) {
      if (cancelled()) continue;
      error = "Not enough memory to prepare UV charts; cancel and reopen the editor";
    } catch (const std::exception &failure) {
      if (cancelled()) continue;
      error = failure.what();
    }
    std::lock_guard lock(model_mutex_);
    if (cancelled() || !uv_ || uv_session_ != job.session ||
        state_.generation != job.generation) continue;
    if (initializing) {
      const auto &source = scene_.active();
      bool same_owner = source.painted && !job.mesh.owner_before(source.painted) &&
                        !source.painted.owner_before(job.mesh);
      if (!same_owner || source.id != control_object_ ||
          job.mesh.get() != &source.painted->mesh() ||
          state_.revision != control_source_.revision) {
        finish_control_locked(false);
        state_.error = "The UV source changed while loading; reopen the editor";
        state_.message.clear();
        publish_locked();
        continue;
      }
    }
    if (uv_->view().mesh != job.mesh) {
      if (workspace_job) {
        uv_solving_ = false;
        uv_scheduled_.reset();
        state_.error = "The UV draft changed during processing; retry";
        publish_locked();
      }
      continue;
    }
    if (workspace_job) {
      uv_solving_ = false;
      uv_scheduled_.reset();
      if (completed) {
        uv_ = std::move(*job.workspace);
        state_.error.clear();
        state_.message = initializing ? "UV charts ready" :
            "UV operation complete; Apply publishes UVs and transferred paint";
      } else {
        if (initializing) uv_->validated(job.mesh, error);
        state_.error = std::move(error);
        state_.message.clear();
      }
      publish_locked();
    } else if (uv_->validated(job.mesh, std::move(error))) publish_locked();
  }
}
void Document::process_uv_transfer(FileJob &job) {
  uint64_t request;
  {
    std::lock_guard lock(model_mutex_);
    if (job.edit_authority!=state_.edit_authority || !object_editable_locked(job.object) || job.generation != state_.generation || job.content != content_ ||
        !uv_ || uv_session_ != job.uv_session || !uv_accept_pending_ ||
        scene_.active().id != job.object)
      throw std::runtime_error("The UV session changed before paint transfer");
    const auto &current = scene_.object(job.object);
    const auto &leased=job.scene.object(job.object);
    if (current.painted != job.paint_identity ||
        current.painted->revision() != job.source_revision || current.polygon!=leased.polygon ||
        current.material_bindings!=leased.material_bindings || current.deformation!=leased.deformation ||
        current.appearance!=leased.appearance || current.shape_source!=leased.shape_source)
      throw std::runtime_error("The source mesh changed before UV transfer");
    request = uv_request_.load();
  }
  auto cancelled = [&] { return stopping_.load() || uv_request_.load() != request || (job.work&&job.work->cancelled.load()); };
  const auto &source = job.scene.object(job.object);
  const auto &old = source.painted->mesh();
  auto target_owner=job.uv_mesh;
  auto map=job.uv_source_vertices;
  std::shared_ptr<const PolygonAuthority> polygon=source.polygon;
  if(source.polygon&&source.polygon->active){
    if(!map)throw std::runtime_error("Polygon UV source correspondence is missing");
    auto derivative=prepare_polygon_uv_derivative(source,*target_owner,*map,cancelled);
    polygon=std::move(derivative.authority);
    target_owner=std::make_shared<const Mesh>(std::move(derivative.mesh));
    map=std::make_shared<const std::vector<uint32_t>>(std::move(derivative.source_vertices));
  }
  const auto &target = *target_owner;
  if (!map || map->size() != target.positions.size() ||
      old.indices.size() != target.indices.size())
    throw std::runtime_error("UV source vertex/corner identity is missing; reopen the UV editor");
  bool uv_modified = false;
  for (size_t vertex = 0; vertex < map->size(); ++vertex) {
    if (cancelled()) throw std::runtime_error("UV transfer cancelled");
    auto original = map->at(vertex);
    if (original >= old.positions.size())
      throw std::runtime_error("UV source vertex identity is stale");
    auto p = old.positions[original], q = target.positions[vertex],
         n = old.normals[original], r = target.normals[vertex];
    if (p.x != q.x || p.y != q.y || p.z != q.z ||
        n.x != r.x || n.y != r.y || n.z != r.z)
      throw std::runtime_error("UV remap changed source geometry or normals");
  }
  for (size_t corner = 0; corner < old.indices.size(); ++corner) {
    auto vertex = target.indices[corner];
    if (vertex >= map->size() || map->at(vertex) != old.indices[corner])
      throw std::runtime_error("UV remap changed original triangle/corner identity");
    uv_modified |= old.uv[old.indices[corner]] != target.uv[vertex];
  }
  const bool own_corners=source.polygon&&source.polygon->active&&source.polygon->colors;
  const bool source_aware=(uv_modified||job.uv_keep_normals||job.uv_keep_paint)&&(own_corners||source.appearance||source.painted->source_maps()||source.material_bindings||source.painted->color_projection());
  // Metadata-only edits of unchanged authored UVs retain the original explicit
  // source-authored policy (shared/zero-area charts are not silently repaired).
  if (uv_modified || !source.painted->source_maps()) {
    auto error = validate_uv(target, cancelled);
    if (!error.empty()) throw std::runtime_error(error);
  }
  std::shared_ptr<const DeformationState> deformation;
  if (!source_aware && source.deformation) {
    source.deformation->validate(old);
    auto remapped = *source.deformation;
    remapped.source = morph_topology_id(target);
    size_t entries = remapped.pose.size() + remapped.morph_weights.size();
    if (remapped.skin) {
      SkinWeights skin;
      skin.vertices.reserve(map->size());
      for (auto original : *map) {
        if (cancelled()) throw std::runtime_error("UV skin remap cancelled");
        const auto &row = source.deformation->skin->vertices.at(original);
        if (row.size() > 8'000'000 - std::min(entries, size_t(8'000'000)))
          throw std::runtime_error("UV remap exceeds deformation resource budget");
        entries += row.size(); skin.vertices.push_back(row);
      }
      remapped.skin = std::move(skin);
    }
    if (remapped.morphs) {
      if (map->size() && remapped.morphs->targets.size() >
          (8'000'000 - std::min(entries, size_t(8'000'000))) / map->size())
        throw std::runtime_error("UV morph remap exceeds deformation resource budget");
      MorphSet morphs(target);
      for (const auto &original : source.deformation->morphs->targets) {
        MorphTarget morph{original.name, {}};
        morph.delta.reserve(map->size());
        for (auto vertex : *map) {
          if (cancelled()) throw std::runtime_error("UV morph remap cancelled");
          morph.delta.push_back(original.delta.at(vertex));
        }
        morphs.targets.push_back(std::move(morph));
      }
      remapped.morphs = std::move(morphs);
    }
    remapped.validate(target);
    deformation = std::make_shared<const DeformationState>(std::move(remapped));
  }
  std::shared_ptr<const CreatorAppearance> appearance;
  if (!source_aware && source.appearance) {
    if (source.appearance->source_vertices.size() != old.positions.size())
      throw std::runtime_error("Original avatar source vertex provenance is incomplete");
    auto updated = std::make_shared<CreatorAppearance>(*source.appearance);
    updated->source_vertices.clear(); updated->source_vertices.reserve(map->size());
    for (auto original : *map)
      updated->source_vertices.push_back(source.appearance->source_vertices.at(original));
    appearance = std::move(updated);
  }
  bool changed = uv_changed(target, old) || morph_topology_id(target) != morph_topology_id(old) ||
    (job.uv_keep_normals&&source.painted->normal_projection()&&source.painted->normal_projection()->version==1)||
    (job.uv_keep_paint&&!source.painted->paint_sampling_domain());
  std::shared_ptr<PaintedMesh> painted;
  auto binding=source.material_bindings;
  if(job.uv_keep_paint) {
    if(!job.uv_keep_normals)throw std::invalid_argument("Retained paint lost its independent normal sampling choice");
    if(!own_corners||!polygon||!source.material_bindings||!source.material_bindings->normal_preparation||
        !source.material_bindings->color_preparation||source.appearance||source.material_bindings->has_mtoon||
        !source.painted->normal_projection()||!source.painted->color_projection())
      throw std::invalid_argument("Keep paint detail lost its complete supported current source");
    auto descriptor=std::make_shared<const PolygonColorSurface>(PolygonColorSurface{std::shared_ptr<const PolygonMesh>(polygon,&polygon->topology),polygon->colors});
    PaintSamplingLimits limits;limits.cancelled=cancelled;
    auto retained=retain_source_corner_paint(source.painted,source.material_bindings,source.deformation,descriptor,target,*map,limits);
    SourceUvBakeInput input;input.paint=source.painted;input.current_material=source.material_bindings;input.current_deformation=source.deformation;input.target=target;input.source_vertices=*map;
    SourceUvBakeResult normal_result;SourceUvBakeOptions options;options.cancelled=cancelled;options.normal_sampling=SourceNormalUvMethod::RetainSource;
    auto normal=detail::retain_normal_sampling(input,normal_result,options,retained.paint->paint_sampling_domain());
    painted=adopt_retained_paint_normal(*retained.paint,*normal,limits);
    auto composed=std::make_shared<SourceMaterialBindings>(*source.material_bindings);composed->color_preparation=std::move(retained.colors);
    auto normal_preparation=std::make_shared<SourceNormalPreparation>(*composed->normal_preparation);normal_preparation->projection=normal->projection;normal_preparation->sampling_domain=normal;normal_preparation->uv_reprojection.reset();composed->normal_preparation=std::move(normal_preparation);
    binding=std::move(composed);deformation=detail::normal_frame_remap_deformation(source.deformation,old,target,*map,cancelled);appearance=source.appearance;
  }else if(source_aware){
    SourceUvBakeInput input;input.paint=source.painted;input.appearance=source.appearance;
    input.current_material=source.material_bindings;input.current_deformation=source.deformation;
    input.target=target;input.source_vertices=*map;
    if(own_corners){input.polygon_authority=source.polygon;
      if(source.material_bindings&&source.material_bindings->color_preparation)input.target_polygon=polygon;
    }
    if(!source.appearance&&!source.material_bindings)
      input.native_material=SourceNativeMaterial{source.material.color,source.material.emissive,int(source.material.alpha),source.material.alpha_cutoff,source.material.double_sided};
    SourceUvBakeOptions options;options.cancelled=cancelled;
    options.normal_sampling=job.uv_keep_normals?SourceNormalUvMethod::RetainSource:SourceNormalUvMethod::Rasterize;
    auto adopted=adopt_source_uv_bake(bake_source_uv(input,options),cancelled);
    painted=std::move(adopted.paint);deformation=std::move(adopted.deformation);
    appearance=std::move(adopted.appearance);binding=std::move(adopted.material_bindings);
  }else painted=changed?source.painted->reparameterize(target,cancelled):source.painted;
  if((uv_modified||job.uv_keep_paint)&&own_corners){
    auto retained=std::make_shared<PolygonAuthority>(*polygon);
    retained->origin=capture_polygon_origin(job.scene,job.object);
    polygon=std::move(retained);
  }
  if (cancelled()) throw std::runtime_error("UV transfer cancelled");
  Scene prepared = job.scene;
  auto &candidate = prepared.object(job.object);
  candidate.painted = std::move(painted);
  candidate.polygon = std::move(polygon);
  candidate.deformation = std::move(deformation);
  candidate.appearance = std::move(appearance);
  candidate.material_bindings=std::move(binding);
  prepared.validate();
  if (cancelled()) throw std::runtime_error("UV transfer cancelled");
  std::shared_ptr<AssetAcceptedSnapshot> accepted;
  if(job.work&&job.work->accepted_scene){
    accepted=std::make_shared<AssetAcceptedSnapshot>();
    accepted->scene=std::make_shared<const Scene>((changed?prepared:job.scene).snapshot());
    accepted->generation=job.generation;accepted->object=job.object;
  }
  {
    std::lock_guard lock(model_mutex_);
    if (cancelled() || job.edit_authority!=state_.edit_authority || !object_editable_locked(job.object) || job.generation != state_.generation || job.content != content_ ||
        !uv_ || uv_session_ != job.uv_session || !uv_accept_pending_ ||
        scene_.active().id != job.object)
      throw std::runtime_error("The UV session changed during paint transfer");
    const auto &current = scene_.object(job.object);
    const auto &leased=job.scene.object(job.object);
    if (current.painted != job.paint_identity ||
        current.painted->revision() != job.source_revision || current.polygon!=leased.polygon ||
        current.material_bindings!=leased.material_bindings || current.deformation!=leased.deformation ||
        current.appearance!=leased.appearance || current.shape_source!=leased.shape_source)
      throw std::runtime_error("The source mesh changed during UV transfer");
    if(accepted)accepted->recovery=recovery_source_;
    Transaction transaction(*this);
    transaction.finish_uv();
    if(job.uv_desktop)desktop_=false;
    if(changed){scene_=std::move(prepared);record_locked({{},transaction.before(),scene_});bind_locked();}
    state_.asset_busy=false;if(asset_work_.load()==job.work)asset_work_.store({});state_.message=job.uv_keep_paint?"UV layout updated. Paint and normal detail retained; save the native project to keep editable sources.":job.uv_keep_normals?"UVs updated; original normal detail kept. Save the native project; glTF/VRM export still needs a sampling conversion.":"UVs, paint and deformation identities updated";state_.error.clear();
    publish_locked(changed,false);transaction.commit();
    if(accepted){
      accepted->revision=state_.revision;accepted->content=content_;
      job.work->completed=std::move(accepted);
      {std::lock_guard file_lock(file_mutex_);job.work->done.store(true);}
    }
  }
  std::lock_guard lock(file_mutex_);
  file_changed_.notify_all();
}
} // namespace dome
