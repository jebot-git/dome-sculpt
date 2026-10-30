// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
namespace dome {
void Document::begin_retopology() {
  std::lock_guard lock(model_mutex_);
  finish_locked();
  begin_retopology_locked();
}
void Document::begin_retopology_locked() {
  auto &o = scene_.active();
  if (state_.retopology || state_.asset_busy || state_.recovery_busy || control_before_ ||
      action_active_locked() || !object_editable_locked(o.id) || scene_.objects().size() >= 256)
    throw std::logic_error("Finish the current action; show and unlock a source with room for a new patch");
  if (o.retopology) {
    const auto source = o.retopology->source;
    if (std::none_of(scene_.objects().begin(), scene_.objects().end(),
                     [source](const auto &object) { return object.id == source; }))
      throw std::logic_error("Retopology reference deleted or unavailable; restore or choose a source");
  }
  retopo_source_ = {state_.generation, state_.revision, o.id,state_.edit_authority};
  retopo_stroking_ = retopo_sampled_ = retopo_accepting_ = false;
  state_.weights.enabled = false;
  auto view = std::make_shared<RetopoView>();
  view->busy = true; view->snap_mm = retopo_snap_mm_;
  state_.retopology = view;
  {
    std::lock_guard lock(retopo_mutex_);
    auto session = ++retopo_session_;
    retopo_jobs_.clear();
    retopo_jobs_.push_back({session, std::make_shared<const Scene>(scene_.snapshot()), {}, {}, retopo_snap_mm_});
    retopo_pending_ = 1;
  }
  state_.error.clear(); state_.message.clear();
  publish_locked(true); // Turn off an existing weight heatmap, if any.
  // A view-only geometry refresh must not invalidate the source guard.
  retopo_source_.revision = state_.revision;
  retopo_changed_.notify_one();
}
void Document::queue_retopology_locked(RetopoAction action, Vec3 point, bool coalescible) {
  if (!state_.retopology || !state_.retopology->source || retopo_accepting_)
    throw std::logic_error("Wait for the retopology source before editing");
  std::lock_guard lock(retopo_mutex_);
  RetopoJob job{retopo_session_.load(), {}, action, point, retopo_snap_mm_, false, coalescible};
  if (coalescible && !retopo_jobs_.empty() && retopo_jobs_.back().coalescible)
    retopo_jobs_.back() = std::move(job);
  else {
    if (retopo_jobs_.size() >= 254 && action != RetopoAction::EndGesture && action != RetopoAction::CancelGesture)
      throw std::length_error("Retopology input queue is full");
    retopo_jobs_.push_back(std::move(job)); ++retopo_pending_;
  }
  auto view = std::make_shared<RetopoView>(*state_.retopology);
  view->busy = true; state_.retopology = std::move(view);
  retopo_changed_.notify_one();
}
void Document::edit_retopology(RetopoAction action) {
  if (action == RetopoAction::BeginGesture || action == RetopoAction::Sample ||
      action == RetopoAction::EndGesture || action == RetopoAction::CancelGesture)
    throw std::invalid_argument("Use the input gesture interface for retopology points");
  std::lock_guard lock(model_mutex_);
  if (retopo_stroking_) { cancel_action_locked(); desktop_ = false; input_.cancel(); }
  queue_retopology_locked(action);
  state_.error.clear(); publish_locked();
}
void Document::retopology_distance(float mm) {
  if (!std::isfinite(mm) || mm < 1 || mm > 200) throw std::invalid_argument("Projection distance must be 1 to 200 mm");
  std::lock_guard lock(model_mutex_);
  if (retopo_accepting_) throw std::logic_error("Wait for the accepted patch");
  retopo_snap_mm_ = mm;
  if (state_.retopology) {
    auto view = std::make_shared<RetopoView>(*state_.retopology);
    view->snap_mm = mm; state_.retopology = std::move(view);
  }
  publish_locked();
}
void Document::cancel_retopology_locked() {
  ++retopo_session_;
  retopo_stroking_ = retopo_sampled_ = retopo_accepting_ = false;
  state_.retopology.reset(); retopo_pending_ = 0;
  std::lock_guard lock(retopo_mutex_);
  retopo_jobs_.clear(); retopo_changed_.notify_all();
}
void Document::end_retopology(bool accept) {
  std::lock_guard lock(model_mutex_);
  end_retopology_locked(accept);
  publish_locked();
}
void Document::end_retopology_locked(bool accept) {
  if (!state_.retopology) return;
  if (retopo_accepting_ && accept)
    throw std::logic_error("Cancel background work or wait for the accepted patch");
  if (!accept && retopo_accepting_ && state_.asset_busy) {
    // The ordered asset stage owns publication now. Request its cancellation;
    // on completion it restores the editable draft rather than losing work.
    cancel_asset_work_locked(); return;
  }
  if (!accept) {
    cancel_retopology_locked(); desktop_ = false; input_.cancel(); state_.error.clear();
    publish_locked(true); return;
  }
  if (!state_.retopology->source) throw std::logic_error("Wait for the source surface");
  if (retopo_stroking_) { end_action_locked(); desktop_ = false; input_.cancel(); }
  std::lock_guard lock(retopo_mutex_);
  if (retopo_jobs_.size() >= 256) throw std::length_error("Retopology input queue is full");
  retopo_jobs_.push_back({retopo_session_.load(), {}, {}, {}, retopo_snap_mm_, true});
  ++retopo_pending_; retopo_accepting_ = true;
  auto view = std::make_shared<RetopoView>(*state_.retopology);
  view->busy = view->committing = true; state_.retopology = std::move(view);
  retopo_changed_.notify_one();
}
bool Document::wait_retopology(std::chrono::milliseconds timeout) const {
  std::unique_lock lock(retopo_mutex_);
  return retopo_changed_.wait_for(lock, timeout, [&] {
    auto view = published_.load()->retopology;
    return stopping_.load() || !view || (!view->busy && !view->committing);
  });
}
void Document::retopo_loop() {
  std::unique_ptr<RetopoWorkspace> workspace;
  uint64_t session = 0;
  for (;;) {
    RetopoJob job;
    {
      std::unique_lock lock(retopo_mutex_);
      retopo_changed_.wait(lock, [&] {
        return stopping_.load() || !retopo_jobs_.empty() || (workspace && session != retopo_session_.load());
      });
      if (stopping_) return;
      if (workspace && session != retopo_session_.load()) {
        // Destruction of a large BVH/history also stays off model/input locks.
        lock.unlock(); workspace.reset(); session = 0; continue;
      }
      job = std::move(retopo_jobs_.front()); retopo_jobs_.pop_front();
    }
    auto cancelled = [&] { return stopping_.load() || job.session != retopo_session_.load(); };
    try {
      if (cancelled()) continue;
      if (job.initial) { workspace = std::make_unique<RetopoWorkspace>(*job.initial, cancelled); session = job.session; }
      if (!workspace || session != job.session) throw std::logic_error("Retopology workspace is unavailable");
      if (job.action) workspace->edit(*job.action, job.point, job.snap_mm, cancelled);
      if (job.accept) {
        workspace->edit(RetopoAction::EndGesture, {}, job.snap_mm, cancelled);
        if (workspace->view().topology->faces().empty()) throw std::logic_error("Make at least one face before applying the patch");
      }
      if (cancelled()) continue;
      std::lock_guard lock(model_mutex_);
      if (cancelled() || !state_.retopology) continue;
      if (retopo_source_.generation != state_.generation || retopo_source_.revision != state_.revision ||
          retopo_source_.object != scene_.active().id) {
        cancel_retopology_locked(); state_.error = "Retopology source changed; reopen the workspace"; publish_locked(true); continue;
      }
      --retopo_pending_;
      auto view = std::make_shared<RetopoView>(workspace->view());
      view->busy = retopo_pending_ != 0; view->committing = retopo_accepting_; view->snap_mm = retopo_snap_mm_;
      state_.retopology = view;
      state_.error.clear(); state_.message.clear();
      if (job.accept) {
        if (!view->changed) { cancel_retopology_locked(); publish_locked(true); continue; }
        auto topology = view->topology; auto source = workspace->source_object(); auto transform = view->transform;
        queue_scene_operation_locked("Apply retopology", [topology, source, transform](Scene &scene, const auto &cancel) {
          publish_retopology(scene, *topology, source, transform, cancel);
        }, retopo_source_, {}, {}, {}, {}, job.session);
      }
      publish_locked();
    } catch (const std::exception &e) {
      std::lock_guard lock(model_mutex_);
      if (cancelled() || !state_.retopology) continue;
      if (retopo_pending_) --retopo_pending_;
      if (job.accept) retopo_accepting_ = false;
      auto view = workspace && session == job.session ? std::make_shared<RetopoView>(workspace->view())
                                                     : std::make_shared<RetopoView>(*state_.retopology);
      view->error = e.what(); view->busy = retopo_pending_ != 0; view->committing = retopo_accepting_;
      state_.retopology = view; state_.error = e.what(); publish_locked();
    }
    retopo_changed_.notify_all();
  }
}
} // namespace dome
