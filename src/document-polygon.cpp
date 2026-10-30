// SPDX-License-Identifier: GPL-3.0-or-later
#include "document-authority-transition.hpp"
#include "document-authority.hpp"
#include "document-transaction.hpp"
#include "dome/document.hpp"
#include "dome/polygon-scene.hpp"
#include "dome/polygon-source.hpp"
#include "polygon-conversion.hpp"
#include <pthread.h>
namespace dome {
namespace {
struct PredicateGuard {
  bool &flag;
  explicit PredicateGuard(bool &value) : flag(value) {
    if (flag)
      throw std::logic_error("Nested polygon predicate lock");
    flag = true;
  }
  ~PredicateGuard() { flag = false; }
};
Vec3 vector(ob::Vec3 v) { return {v.x, v.y, v.z}; }
Vec3 local_point(CanvasTransform world, Vec3 p) {
  return vector(ob::transform(ob::inverse(world), {p.x, p.y, p.z}));
}
Vec3 local_direction(CanvasTransform world, Vec3 p) {
  return normalized(
      vector(ob::rotate(ob::conjugate(world.orientation), {p.x, p.y, p.z})));
}
std::optional<Vec3> intersect(Vec3 origin, Vec3 direction, Vec3 point,
                              Vec3 normal, float maximum) {
  auto denominator = dot(direction, normal);
  if (std::abs(denominator) < 1e-6f)
    return {};
  auto distance = dot(point - origin, normal) / denominator;
  if (!std::isfinite(distance) || distance < 0 || distance > maximum)
    return {};
  return origin + direction * distance;
}
} // namespace
bool Document::polygon_source_matches_locked() const {
  if (!polygon_initial_ || !state_.polygon)
    return false;
  if (polygon_source_.generation != state_.generation ||
      polygon_source_.edit_authority != state_.edit_authority ||
      polygon_source_.object != scene_.active().id ||
      !object_editable_locked(polygon_source_.object))
    return false;
  const auto &source = polygon_initial_->object(polygon_source_.object),
             &current = scene_.object(polygon_source_.object);
  if (source.mesh_active != current.mesh_active ||
      source.volume->revision() != current.volume->revision() ||
      source.volume->geometry_owner() != current.volume->geometry_owner() ||
      source.volume->voxel_size() != current.volume->voxel_size() ||
      source.volume->band() != current.volume->band() ||
      !Volume::same_settings(source.volume->sculpt_settings(),
                             current.volume->sculpt_settings()) ||
      bool(source.painted) != bool(current.painted) ||
      source.deformation != current.deformation ||
      source.appearance != current.appearance ||
      source.material_bindings != current.material_bindings ||
      source.retopology != current.retopology ||
      source.polygon != current.polygon || source.group != current.group ||
      !detail::same_material(source.material, current.material))
    return false;
  if (source.painted &&
      (source.painted->revision() != current.painted->revision() ||
       source.painted->mesh_owner() != current.painted->mesh_owner()))
    return false;
  return true;
}
void Document::rebase_polygon_transform_locked() {
  if (!state_.polygon)
    return;
  if (!polygon_source_matches_locked())
    throw std::logic_error("Polygon source changed during object motion; "
                           "cancel and reopen the workspace");
  polygon_source_.revision = state_.revision;
}
void Document::begin_polygon(bool convert_source, AssetSource expected) {
  std::lock_guard lock(model_mutex_);
  require_authority_locked(expected);
  if ((expected.generation && expected.generation != state_.generation) ||
      (expected.revision && expected.revision != state_.revision) ||
      (expected.object && expected.object != scene_.active().id))
    throw std::logic_error("The polygon source changed; reopen the editor");
  if(!state_.polygon && !state_.object_grabbing)finish_locked();
  begin_polygon_locked(convert_source);
}
void Document::begin_polygon_locked(bool convert_source) {
  auto &object = scene_.active();
  if (polygon_closing_)
    throw std::logic_error("The polygon worker is closing");
  if (state_.polygon) {
    if (polygon_accepting_)
      throw std::logic_error("Wait for the accepted polygon edit");
    return;
  }
  if (state_.asset_busy || state_.recovery_busy || control_before_ ||
      action_active_locked() || state_.retopology || state_.cut || import_view_ ||
      !object_editable_locked(object.id))
    throw std::logic_error(
        "Finish the current action and select an editable object");
  if (polygon_conversion_required(object) && !convert_source)
    throw std::logic_error("Convert with retained source before editing this "
                           "imported or deformed mesh");
  auto initial = std::make_shared<const Scene>(scene_.snapshot());
  auto view = std::make_shared<PolygonView>();
  view->busy = true;
  auto source = AssetSource{state_.generation, state_.revision, object.id,
                            state_.edit_authority};
  const auto session = polygon_session_.load() + 1;
  if (!session)
    throw std::overflow_error("Polygon workspace identity exhausted");
  PolygonJob job;
  job.session = session;
  job.initial = initial;
  job.convert_source = convert_source;
  {
    std::lock_guard lock(polygon_mutex_);
    PredicateGuard predicate_guard(polygon_predicate_locked_);
    polygon_jobs_.push_back(std::move(job));
    try {
      Transaction transaction(*this);
      polygon_initial_ = std::move(initial);
      polygon_source_ = source;
      polygon_pending_ = 1;
      polygon_accepting_ = polygon_stroking_ = false;
      state_.polygon = view;
      state_.polygon_object = object.id;
      const bool had_weights = state_.weights.enabled;
      state_.weights.enabled = false;
      state_.error.clear();
      state_.message = "Preparing polygon workspace...";
      publish_locked(had_weights, false);
      polygon_source_.revision = state_.revision;
      transaction.commit(false);
      polygon_session_.store(session);
    } catch (...) {
      polygon_jobs_.pop_back();
      polygon_initial_.reset();
      polygon_pending_ = 0;
      throw;
    }
  }
  polygon_changed_.notify_one();
}
void Document::complete_polygon_accept(
    const std::shared_ptr<PolygonAccept> &token, bool success,
    std::shared_ptr<const Scene> scene, uint64_t content, uint64_t revision,
    std::exception_ptr error) {
  if (!token)
    return;
  std::lock_guard lock(token->mutex);
  if (token->done)
    return;
  if (success) {
    token->completed = std::move(scene);
    token->content = content;
    token->revision = revision;
  }
  token->success = success;
  token->error = std::move(error);
  token->done = true;
  token->changed.notify_all();
}
bool Document::held_polygon_pending_locked() const {
  return polygon_accepting_.load() && polygon_accept_ &&
         state_.object_grabbing && state_.held_object == polygon_source_.object;
}
void Document::cancel_polygon_locked(bool cancel_acceptance) {
  if (cancel_acceptance && polygon_accept_) {
    polygon_accept_->work->cancelled.store(true);
    complete_polygon_accept(polygon_accept_, false);
    if (asset_work_.load() == polygon_accept_->work) {
      asset_work_.store({});
      state_.asset_busy = false;
    }
  }
  ++polygon_session_;
  if (polygon_stroking_ && !state_.object_grabbing)
    desktop_ = false;
  polygon_stroking_ = false;
  polygon_accepting_ = false;
  polygon_pending_ = 0;
  state_.polygon.reset();
  state_.polygon_object = 0;
  polygon_initial_.reset();
  {
    std::lock_guard lock(polygon_mutex_);
    polygon_jobs_.clear();
    polygon_changed_.notify_all();
  }
  if (cancel_acceptance)
    polygon_accept_.reset();
}
void Document::restore_polygon_source(AssetSource expected) {
  std::lock_guard lock(model_mutex_);
  restore_polygon_source_locked(expected);
}
void Document::restore_polygon_source_locked(AssetSource expected) {
  if (state_.polygon)
    throw std::logic_error(
        "Apply or cancel the polygon workspace before restoring its source");
  if (!expected.generation)
    expected = {state_.generation, state_.revision, scene_.active().id,
                state_.edit_authority};
  queue_scene_operation_locked(
      "Restore original polygon source",
      [id = scene_.active().id](Scene &scene, const auto &cancelled) {
        restore_polygon_origin(scene, id, cancelled);
      },
      expected, {});
}
void Document::drain_polygon_shutdown() {
  std::shared_ptr<PolygonAccept> token;
  {
    std::lock_guard lock(model_mutex_);
    polygon_closing_ = true;
    if (polygon_accepting_)
      token = polygon_accept_;
    else if (state_.polygon)
      cancel_polygon_locked();
  }
  if (token) {
    std::unique_lock lock(token->mutex);
    token->changed.wait(lock, [&] { return token->done; });
  }
  {
    std::lock_guard lock(model_mutex_);
    if (state_.polygon)
      cancel_polygon_locked();
  }
}
void Document::capture_polygon_file(FileJob &job) {
  auto token = job.polygon_capture;
  {
    std::unique_lock lock(token->mutex);
    token->changed.wait(lock, [&] { return token->done; });
    if (!token->success) {
      if (token->error)
        std::rethrow_exception(token->error);
      throw std::runtime_error(
          "Polygon edit cancelled; dependent save/export cancelled");
    }
    job.revision = token->revision;
    job.generation = token->source.generation;
    job.content = token->content;
    job.recovery_source = token->recovery_source;
    job.capture_scene = token->completed;
    job.object = token->source.object;
  }
  job.polygon_capture.reset();
}
void Document::configure_polygon(PolygonControls controls) {
  if (!std::isfinite(controls.distance_mm) || controls.distance_mm < .001f ||
      controls.distance_mm > 1e6f || controls.cuts < 1 || controls.cuts > 64)
    throw std::invalid_argument(
        "Polygon distance must be 0.001 to 1000000 mm and cuts 1 to 64");
  std::lock_guard lock(model_mutex_);
  if (polygon_accepting_ || polygon_stroking_)
    throw std::logic_error("Finish the polygon gesture before changing controls");
  auto old = state_.polygon_controls;
  state_.polygon_controls = controls;
  try {
    publish_locked(false, false);
  } catch (...) {
    state_.polygon_controls = old;
    throw;
  }
}
void Document::queue_polygon_locked(PolygonJob job, bool coalescible) {
  if (polygon_closing_ || !state_.polygon || !state_.polygon->surface ||
      polygon_accepting_)
    throw std::logic_error("Wait for the polygon source before editing");
  if (polygon_source_.revision != state_.revision ||
      !polygon_source_matches_locked())
    throw std::logic_error(
        "Polygon source changed; cancel and reopen the workspace");
  job.session = polygon_session_.load();
  const bool boundary = job.action == PolygonAction::EndDrag ||
                        job.action == PolygonAction::CancelDrag;
  auto view = std::make_shared<PolygonView>(*state_.polygon);
  view->busy = true;
  view->error.clear();
  {
    std::lock_guard lock(polygon_mutex_);
    PredicateGuard predicate_guard(polygon_predicate_locked_);
    const bool replace = coalescible && !polygon_jobs_.empty() &&
                         polygon_jobs_.back().action == PolygonAction::Drag;
    std::optional<PolygonJob> previous;
    auto old_view = state_.polygon;
    auto old_error = state_.error;
    if (replace) {
      previous = std::move(polygon_jobs_.back());
      polygon_jobs_.back() = std::move(job);
    } else {
      if (polygon_jobs_.size() >= (boundary ? 256u : 254u))
        throw std::length_error(
            "Polygon input queue is full; wait before sending more commands");
      polygon_jobs_.push_back(std::move(job));
      ++polygon_pending_;
    }
    try {
      state_.polygon = std::move(view);
      state_.error.clear();
      publish_locked(false, false);
    } catch (...) {
      state_.polygon = std::move(old_view);
      state_.error = std::move(old_error);
      if (replace)
        polygon_jobs_.back() = std::move(*previous);
      else {
        polygon_jobs_.pop_back();
        --polygon_pending_;
      }
      throw;
    }
  }
  polygon_changed_.notify_one();
}
void Document::edit_polygon(PolygonAction action) {
  if (action >= PolygonAction::Pick)
    throw std::invalid_argument(
        "Use the pointer gesture API for polygon manipulation");
  std::lock_guard lock(model_mutex_);
  if (polygon_stroking_)
    throw std::logic_error(
        "Finish the polygon move before changing its topology");
  PolygonJob job;
  job.action = action;
  auto world = object_world_transform_locked(scene_.active().id);
  job.amount = state_.polygon_controls.distance_mm * .001f / world.scale;
  job.cuts = state_.polygon_controls.cuts;
  queue_polygon_locked(std::move(job));
}
void Document::polygon_pointer_locked(Vec3 origin, Vec3 direction,
                                      PolygonAction action, bool xr,
                                      bool additive, uint32_t owner) {
  if (!finite(origin) || !finite(direction) || length(direction) < 1e-7f)
    throw std::invalid_argument("Invalid polygon pointer ray");
  if (action < PolygonAction::Pick)
    throw std::invalid_argument("Polygon pointer requires a gesture action");
  if (action == PolygonAction::BeginDrag && polygon_stroking_)
    throw std::logic_error(
        "Finish the current polygon move before starting another");
  if ((action == PolygonAction::Drag || action == PolygonAction::EndDrag ||
       action == PolygonAction::CancelDrag) &&
      (!polygon_stroking_ || polygon_owner_ != owner))
    return;
  const auto object = scene_.active().id;
  auto world = object_world_transform_locked(object);
  if (xr)
    world = ob::compose(state_.canvas, world);
  PolygonJob job;
  job.action = action;
  job.origin = local_point(world, origin);
  job.direction = local_direction(world, direction);
  job.radius = .015f / world.scale;
  job.maximum = 100 / world.scale;
  job.additive = additive || state_.polygon_controls.additive;
  const auto old_stroking = polygon_stroking_;
  const auto old_owner = polygon_owner_;
  if (action == PolygonAction::BeginDrag) {
    polygon_stroking_ = true;
    polygon_owner_ = owner;
  }
  if (action == PolygonAction::EndDrag || action == PolygonAction::CancelDrag)
    polygon_stroking_ = false;
  try {
    queue_polygon_locked(std::move(job), action == PolygonAction::Drag);
  } catch (...) {
    polygon_stroking_ = old_stroking;
    polygon_owner_ = old_owner;
    throw;
  }
}
void Document::polygon_pointer(Vec3 origin, Vec3 direction,
                               PolygonAction action, bool additive) {
  std::lock_guard lock(model_mutex_);
  polygon_pointer_locked(origin, direction, action, false, additive);
  if (action == PolygonAction::BeginDrag)
    desktop_ = true;
  if (action == PolygonAction::EndDrag || action == PolygonAction::CancelDrag)
    desktop_ = false;
}
void Document::end_polygon(bool accept) {
  std::lock_guard lock(model_mutex_);
  end_polygon_locked(accept);
}
void Document::end_polygon_locked(bool accept) {
  if (!state_.polygon)
    return;
  if (!accept) {
    cancel_polygon_locked();
    state_.error.clear();
    publish_locked(false, false);
    return;
  }
  if (polygon_closing_)
    throw std::logic_error("The polygon worker is closing");
  if (polygon_accepting_)
    throw std::logic_error("Wait for or cancel the accepted polygon edit");
  if (!state_.polygon->surface)
    throw std::logic_error("Wait for the polygon source");
  if (polygon_source_.revision != state_.revision ||
      !polygon_source_matches_locked())
    throw std::logic_error(
        "Polygon source changed; cancel and reopen the workspace");
  auto token = std::make_shared<PolygonAccept>();
  token->before = std::make_shared<const Scene>(scene_.snapshot());
  token->completed = token->before;
  token->original = polygon_initial_;
  token->work = std::make_shared<AssetWork>();
  token->source = polygon_source_;
  token->content = content_;
  token->isolation_target=state_.isolate_held?state_.held_object:0;
  token->revision = state_.revision;
  token->recovery_source = recovery_source_;
  token->held_pose =
      state_.object_grabbing && state_.held_object == polygon_source_.object;
  token->pose = held_preview_transform_;
  for (size_t i = 0; i < 2; ++i)
    token->grips[i] =
        state_.hands[i].tracked.valid && state_.hands[i].tracked.squeeze > .55f;
  PolygonJob job;
  job.session = polygon_session_.load();
  job.accept = true;
  job.action = PolygonAction::EndDrag;
  job.acceptance = token;
  auto view = std::make_shared<PolygonView>(*state_.polygon);
  view->busy = view->committing = true;
  const auto old_token = polygon_accept_;
  const bool old_stroking = polygon_stroking_;
  {
    std::lock_guard lock(polygon_mutex_);
    PredicateGuard predicate_guard(polygon_predicate_locked_);
    if (polygon_jobs_.size() >= 256)
      throw std::length_error(
          "Polygon input queue is full; wait before applying");
    Transaction transaction(*this);
    polygon_jobs_.push_back(std::move(job));
    ++polygon_pending_;
    try {
      polygon_accept_ = token;
      polygon_accepting_ = true;
      polygon_stroking_ = false;
      asset_work_.store(token->work);
      state_.asset_busy = true;
      state_.polygon = std::move(view);
      state_.error.clear();
      state_.message = "Applying polygon edit...";
      publish_locked(false, false);
      transaction.commit(false);
    } catch (...) {
      polygon_jobs_.pop_back();
      --polygon_pending_;
      polygon_accept_ = old_token;
      polygon_accepting_ = false;
      polygon_stroking_ = old_stroking;
      throw;
    }
  }
  polygon_changed_.notify_one();
}
bool Document::wait_polygon(std::chrono::milliseconds timeout) const {
  std::unique_lock lock(polygon_mutex_);
  return polygon_changed_.wait_for(lock, timeout, [&] {
    auto view = published_.load()->polygon;
    return stopping_.load() || !view || (!view->busy && !view->committing);
  });
}

bool Document::polygon_command_enabled_locked(SpatialCommand command) const {
  if(command==SpatialCommand::PolygonBegin)
    return state_.polygon ? state_.polygon->surface&&!state_.polygon->busy&&!polygon_accepting_&&!control_before_ : state_.can_polygon&&!control_before_;
  if(command==SpatialCommand::PolygonCancel)return bool(state_.polygon);
  if(!state_.polygon||!state_.polygon->surface||state_.polygon->busy||polygon_accepting_||state_.asset_busy||control_before_)return false;
  if(command==SpatialCommand::Undo)return polygon_stroking_||state_.polygon->can_undo;
  if(command==SpatialCommand::Redo)return !polygon_stroking_&&state_.polygon->can_redo;
  if(command==SpatialCommand::Save||command==SpatialCommand::Export)return true;
  return polygon_command(command)&&!polygon_stroking_;
}
void Document::polygon_command_locked(SpatialCommand command) {
  if(!polygon_command_enabled_locked(command))return;
  if(command==SpatialCommand::PolygonBegin){
    if(!state_.polygon){
      if(polygon_conversion_required(scene_.active())||state_.polygon_has_source){
        control_event_locked({VrControlEventType::Begin,VrParameter::PolygonConversion});
        input_.resume_control(VrParameter::PolygonConversion);
      }else begin_polygon_locked(false);
    }
    return;
  }
  if(command==SpatialCommand::PolygonApply||command==SpatialCommand::PolygonCancel){end_polygon_locked(command==SpatialCommand::PolygonApply);return;}
  if(command==SpatialCommand::PolygonMoveMode){state_.polygon_controls.move_mode=!state_.polygon_controls.move_mode;return;}
  if(command==SpatialCommand::PolygonAdditive){state_.polygon_controls.additive=!state_.polygon_controls.additive;return;}
  PolygonJob job;
  switch(command){
  case SpatialCommand::Undo:job.action=polygon_stroking_?PolygonAction::CancelDrag:PolygonAction::Undo;break;
  case SpatialCommand::Redo:job.action=PolygonAction::Redo;break;
  case SpatialCommand::PolygonVertices:job.action=PolygonAction::Vertices;break;
  case SpatialCommand::PolygonEdges:job.action=PolygonAction::Edges;break;
  case SpatialCommand::PolygonFaces:job.action=PolygonAction::Faces;break;
  case SpatialCommand::PolygonSelectAll:{
    const auto &view=*state_.polygon;auto count=view.selection->mode==PolygonSelectionMode::Vertex?view.topology->vertices().size():view.selection->mode==PolygonSelectionMode::Edge?view.topology->edges().size():view.topology->faces().size();
    job.action=view.selection->ids.size()==count?PolygonAction::Clear:PolygonAction::SelectAll;break;}
  case SpatialCommand::PolygonClear:job.action=PolygonAction::Clear;break;
  case SpatialCommand::PolygonExtrude:job.action=PolygonAction::Extrude;break;
  case SpatialCommand::PolygonInset:job.action=PolygonAction::Inset;break;
  case SpatialCommand::PolygonBridge:job.action=PolygonAction::Bridge;break;
  case SpatialCommand::PolygonDissolve:job.action=PolygonAction::Dissolve;break;
  case SpatialCommand::PolygonWeld:job.action=PolygonAction::Weld;break;
  case SpatialCommand::PolygonCutEdges:job.action=PolygonAction::CutEdges;break;
  case SpatialCommand::PolygonConnect:job.action=PolygonAction::ConnectVertices;break;
  case SpatialCommand::PolygonDelete:job.action=PolygonAction::DeleteFaces;break;
  case SpatialCommand::PolygonFlip:job.action=PolygonAction::FlipFaces;break;
  default:return;
  }
  job.amount=state_.polygon_controls.distance_mm*.001f/object_world_transform_locked(scene_.active().id).scale;
  job.cuts=state_.polygon_controls.cuts;
  queue_polygon_locked(std::move(job));
  if(command==SpatialCommand::Undo)polygon_stroking_=false;
}
void Document::process_polygon_pointer_locked(const SpatialFrame &frame) {
  if(!state_.polygon||!state_.polygon->surface||polygon_accepting_||state_.asset_busy)return;
  if(input_.menu_open()||state_.controls.parameter!=VrParameter::None){if(polygon_stroking_)cancel_action_locked();return;}
  for(unsigned i=0;i<2;++i){
    const auto &hand=frame.hands[i].tracked;
    const bool pressed=hand.valid&&hand.aimValid&&hand.trigger>.02f&&hand.squeeze<=.55f;
    const bool previous=state_.hands[i].tracked.valid&&state_.hands[i].tracked.aimValid&&state_.hands[i].tracked.trigger>.02f&&state_.hands[i].tracked.squeeze<=.55f;
    if(polygon_stroking_&&polygon_owner_!=i+1)continue;
    auto origin=vector(hand.aim.position),direction=vector(ob::rotate(hand.aim.orientation,{0,0,-1}));
    if(pressed&&!previous)polygon_pointer_locked(origin,direction,state_.polygon_controls.move_mode?PolygonAction::BeginDrag:PolygonAction::Pick,true,false,i+1);
    else if(pressed&&polygon_stroking_)polygon_pointer_locked(origin,direction,PolygonAction::Drag,true,false,i+1);
    else if(!pressed&&polygon_stroking_)polygon_pointer_locked(origin,direction,(hand.valid&&hand.aimValid)?PolygonAction::EndDrag:PolygonAction::CancelDrag,true,false,i+1);
  }
}

void Document::polygon_loop() {
  pthread_setname_np(pthread_self(),"dome-polygon");
  std::unique_ptr<PolygonWorkspace> workspace;
  uint64_t session = 0;
  std::optional<Vec3> anchor;
  Vec3 plane{}, normal{};
  bool convert = false;
  for (;;) {
    PolygonJob job;
    {
      std::unique_lock lock(polygon_mutex_);
      polygon_changed_.wait(lock, [&] {
        return stopping_ || !polygon_jobs_.empty() ||
               (workspace && session != polygon_session_.load());
      });
      if (stopping_)
        return;
      if (workspace && session != polygon_session_.load()) {
        lock.unlock();
        workspace.reset();
        anchor.reset();
        session = 0;
        continue;
      }
      job = std::move(polygon_jobs_.front());
      polygon_jobs_.pop_front();
    }
    struct Completion {
      Document *document;
      std::shared_ptr<PolygonAccept> token;
      ~Completion() {
        try {
          document->complete_polygon_accept(token, false);
        } catch (...) {
        }
      }
    } completion{this, job.acceptance};
    auto stale = [&] {
      return stopping_.load() || job.session != polygon_session_.load();
    };
    auto cancelled = [&] {
      return stale() ||
             (job.acceptance && job.acceptance->work->cancelled.load());
    };
    auto checkpoint = [&] {
      if (cancelled())
        throw std::runtime_error("Polygon edit cancelled");
    };
    bool consumed = false;
    try {
      checkpoint();
      auto next_anchor = anchor;
      auto next_plane = plane, next_normal = normal;
      std::unique_ptr<PolygonWorkspace> candidate;
      PolygonLimits limits;
      limits.cancelled = cancelled;
      if (job.initial) {
        auto topology = prepare_polygon_workspace_topology(
            job.initial->active(), job.convert_source, cancelled);
        candidate =
            std::make_unique<PolygonWorkspace>(std::move(topology), limits);
        next_anchor.reset();
      } else {
        if (!workspace || session != job.session)
          throw std::logic_error("Polygon workspace is unavailable");
        // Mutation, BVH construction and local-history allocation stay
        // detached. A failed model publication cannot advance hidden worker
        // authority.
        candidate = std::make_unique<PolygonWorkspace>(*workspace);
      }
      if (job.action) {
        switch (*job.action) {
        case PolygonAction::Vertices:
          candidate->mode(PolygonSelectionMode::Vertex, limits);
          break;
        case PolygonAction::Edges:
          candidate->mode(PolygonSelectionMode::Edge, limits);
          break;
        case PolygonAction::Faces:
          candidate->mode(PolygonSelectionMode::Face, limits);
          break;
        case PolygonAction::SelectAll:
          candidate->select_all(limits);
          break;
        case PolygonAction::Clear:
          candidate->clear_selection(limits);
          break;
        case PolygonAction::Undo:
          candidate->undo();
          break;
        case PolygonAction::Redo:
          candidate->redo();
          break;
        case PolygonAction::Extrude:
          candidate->apply(PolygonOperation::Extrude, {}, job.amount, 1,
                           limits);
          break;
        case PolygonAction::Inset:
          candidate->apply(PolygonOperation::Inset, {}, job.amount, 1, limits);
          break;
        case PolygonAction::Bridge:
          candidate->apply(PolygonOperation::Bridge, {}, 0, 1, limits);
          break;
        case PolygonAction::Dissolve:
          candidate->apply(PolygonOperation::Dissolve, {}, 0, 1, limits);
          break;
        case PolygonAction::Weld:
          candidate->apply(PolygonOperation::Weld, {}, 0, 1, limits);
          break;
        case PolygonAction::CutEdges:
          candidate->apply(PolygonOperation::CutEdges, {}, 0, job.cuts, limits);
          break;
        case PolygonAction::ConnectVertices:
          candidate->apply(PolygonOperation::ConnectVertices, {}, 0, 1, limits);
          break;
        case PolygonAction::DeleteFaces:
          candidate->apply(PolygonOperation::DeleteFaces, {}, 0, 1, limits);
          break;
        case PolygonAction::FlipFaces:
          candidate->apply(PolygonOperation::FlipFaces, {}, 0, 1, limits);
          break;
        case PolygonAction::Pick:
        case PolygonAction::BeginDrag:
          if (candidate->select_ray(job.origin, job.direction, job.maximum,
                                    job.radius, job.additive, job.additive,
                                    limits,
                                    *job.action == PolygonAction::BeginDrag) &&
              *job.action == PolygonAction::BeginDrag &&
              candidate->view().display->has_selection) {
            next_plane = candidate->view().display->selection_center;
            next_normal = job.direction;
            next_anchor = intersect(job.origin, job.direction, next_plane,
                                    next_normal, job.maximum);
            if (next_anchor)
              candidate->begin_move();
          }
          break;
        case PolygonAction::Drag:
          if (next_anchor && candidate->view().moving)
            if (auto point = intersect(job.origin, job.direction, next_plane,
                                       next_normal, job.maximum))
              candidate->move_to(*point - *next_anchor, limits);
          break;
        case PolygonAction::EndDrag:
          if (candidate->view().moving)
            candidate->end_move(true, limits);
          next_anchor.reset();
          break;
        case PolygonAction::CancelDrag:
          if (candidate->view().moving)
            candidate->end_move(false, limits);
          next_anchor.reset();
          break;
        }
      }
      checkpoint();
      auto view = std::make_shared<PolygonView>(candidate->view());
      std::optional<Scene> prepared;
      std::shared_ptr<const Scene> endpoint;
      bool global_change = false;
      std::string completion_message;
      if (job.accept) {
        auto token = job.acceptance;
        token->work->progress.store(.1f);
        prepared = token->before->snapshot();
        if (token->held_pose)
          prepared->object(token->source.object).transform = token->pose;
        global_change =
            view->changed ||
            (token->held_pose &&
             !detail::same_transform(
                 token->before->object(token->source.object).transform,
                 token->pose));
        if (view->changed) {
          auto initial = token->original;
          adopt_polygon_edit(*prepared, token->source.object, *view->topology,
                             initial->object(token->source.object),
                             initial->world_transform(token->source.object),
                             convert, cancelled);
        }
        token->work->progress.store(.9f);
        if(token->isolation_target)detail::require_isolated_scene(*token->before,*prepared,token->isolation_target);
        prepared->validate();
        checkpoint();
        endpoint = global_change
                       ? std::make_shared<const Scene>(prepared->snapshot())
                       : token->before;
        token->work->progress.store(.95f);
        completion_message=global_change?"Polygon edit complete":"Polygon edit unchanged";
        const auto &object=prepared->object(token->source.object);
        if(object.mesh_active&&object.painted){auto missing=object.painted->surface_diagnostics().unpaintable_uv_faces.size();
          if(missing)completion_message+=" | "+std::to_string(missing)+" triangles need UVs; use UV editor / Unwrap";
        }
      }
      std::lock_guard lock(model_mutex_);
      if (stale() || !state_.polygon)
        continue;
      checkpoint();
      if (polygon_source_.revision != state_.revision ||
          !polygon_source_matches_locked()) {
        cancel_polygon_locked();
        state_.error = "Polygon source changed; reopen the workspace";
        publish_locked(false, false);
        continue;
      }
      if (polygon_pending_)
        --polygon_pending_;
      consumed = true;
      view->busy = polygon_pending_ != 0;
      view->committing = polygon_accepting_;
      if (job.accept) {
        auto token = job.acceptance;
        if (!token || polygon_accept_ != token ||
            token->source.generation != state_.generation ||
            token->source.revision != state_.revision ||
            token->content != content_ || token->work != asset_work_.load())
          throw std::logic_error(
              "Polygon accepted source changed before publication");
        AuthorityTransition authority(*this);
        Transaction transaction(*this);
        state_.asset_busy = false;
        state_.polygon = view;
        if (global_change) {
          scene_ = std::move(*prepared);
          record_locked({{}, transaction.before(), scene_});
          bind_locked();
        }
        const bool end_held = token->held_pose && state_.object_grabbing &&
                              state_.held_object == token->source.object;
        if (end_held) {
          state_.object_grabbing = false;
          desktop_ = false;
          object_navigation_.reset();
        }
        transaction.finish_polygon(global_change && view->changed);
        state_.error.clear();
        state_.message = std::move(completion_message);
        publish_locked(global_change, false);
        transaction.commit(global_change);
        authority.commit();
        if (end_held)
          polygon_grip_latch_ = token->grips;
        token->work->progress.store(1);
        complete_polygon_accept(token, true, std::move(endpoint), content_,
                                state_.revision);
        if (asset_work_.load() == token->work)
          asset_work_.store({});
        polygon_accept_.reset();
        workspace.reset();
        session = 0;
        anchor.reset();
      } else {
        auto previous = state_.polygon;
        auto old_error = state_.error;
        auto old_message = state_.message;
        state_.polygon = std::move(view);
        state_.error.clear();
        state_.message.clear();
        try {
          publish_locked(false, false);
        } catch (...) {
          state_.polygon = std::move(previous);
          state_.error = std::move(old_error);
          state_.message = std::move(old_message);
          throw;
        }
        workspace = std::move(candidate);
        session = job.session;
        anchor = next_anchor;
        plane = next_plane;
        normal = next_normal;
        if (job.initial)
          convert = job.convert_source;
      }
    } catch (const std::exception &error) {
      std::lock_guard lock(model_mutex_);
      if (stale() || !state_.polygon)
        continue;
      if (!consumed && polygon_pending_)
        --polygon_pending_;
      if (job.accept) {
        polygon_accepting_ = false;
        if (asset_work_.load() == job.acceptance->work) {
          asset_work_.store({});
          state_.asset_busy = false;
        }
        complete_polygon_accept(job.acceptance, false, {}, 0, 0,
                                std::current_exception());
      }
      try {
        AuthorityTransition authority(*this);
        const bool reset_pose =
            job.acceptance && job.acceptance->held_pose &&
            state_.object_grabbing &&
            state_.held_object == job.acceptance->source.object;
        if (reset_pose) {
          state_.object_grabbing = false;
          desktop_ = false;
          object_navigation_.reset();
          held_preview_transform_ = held_start_transform_ =
              scene_.object(state_.held_object).transform;
        }
        if (job.initial && !workspace)
          cancel_polygon_locked();
        else {
          auto view = workspace && session == job.session
                          ? std::make_shared<PolygonView>(workspace->view())
                          : std::make_shared<PolygonView>(*state_.polygon);
          view->busy = polygon_pending_ != 0;
          view->committing = polygon_accepting_;
          view->error = error.what();
          state_.polygon = std::move(view);
        }
        if (job.acceptance && job.acceptance->work->cancelled.load()) {
          state_.error.clear();
          state_.message = "Polygon edit cancelled";
        } else
          state_.error = error.what();
        publish_locked(false, false);
        authority.commit();
        if (reset_pose)
          polygon_grip_latch_ = job.acceptance->grips;
      } catch (...) {
        // Preserve committed/local authority even if reporting is itself out of
        // memory. The next successful input publication can surface status.
        try {
          state_.error = "Out of memory";
        } catch (...) {
        }
      }
    }
    // Progress waiters and worker shutdown both need the predicate-mutex
    // handshake even though the session/progress counters are atomic.
    {
      std::lock_guard lock(polygon_mutex_);
      polygon_changed_.notify_all();
    }
    {
      std::lock_guard lock(file_mutex_);
      file_changed_.notify_all();
    }
  }
}
} // namespace dome
