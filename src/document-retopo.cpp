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
  // Prepare the view before accepting an input job. A failed view allocation
  // must not leave a hidden Sample/EndGesture in the worker queue.
  auto view = std::make_shared<RetopoView>(*state_.retopology);
  view->busy = true;
  if (coalescible && !retopo_jobs_.empty() && retopo_jobs_.back().coalescible)
    retopo_jobs_.back() = std::move(job);
  else {
    if (retopo_jobs_.size() >= 254 && action != RetopoAction::EndGesture && action != RetopoAction::CancelGesture)
      throw std::length_error("Retopology input queue is full");
    retopo_jobs_.push_back(std::move(job)); ++retopo_pending_;
  }
  state_.retopology = std::move(view);
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
  // Successful acceptance already published its ordered asset owner. Do not
  // introduce another throwing allocation after that transaction commits.
  if (!accept) publish_locked();
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
  // The accepted input job already finalizes the live gesture. Do not enqueue
  // a separate EndGesture before acceptance allocations can still fail.
  auto acceptance = std::make_shared<RetopoAcceptance>();
  acceptance->work = std::make_shared<AssetWork>();
  acceptance->changed = std::make_shared<bool>(true);
  auto view = std::make_shared<RetopoView>(*state_.retopology);
  view->busy = view->committing = true;
  const auto session = retopo_session_.load();
  RetopoJob job{session, {}, {}, {}, retopo_snap_mm_, true};
  job.acceptance = acceptance;
  {
    std::lock_guard lock(retopo_mutex_);
    if (retopo_jobs_.size() >= 256) throw std::length_error("Retopology input queue is full");
    retopo_jobs_.push_back(std::move(job));
  }
  const auto previous = state_.retopology;
  const auto pending = retopo_pending_;
  const auto stroking = retopo_stroking_, sampled = retopo_sampled_, desktop = desktop_;
  ++retopo_pending_; retopo_accepting_ = true;
  state_.retopology = std::move(view);
  retopo_stroking_ = retopo_sampled_ = false; desktop_ = false;
  try {
    queue_scene_operation_locked("Apply retopology", [this, acceptance](Scene &scene, const auto &cancelled) {
      std::unique_lock lock(retopo_mutex_);
      retopo_changed_.wait(lock, [&] { return acceptance->ready || cancelled(); });
      if (cancelled()) throw std::runtime_error("Retopology acceptance cancelled");
      if (acceptance->error) std::rethrow_exception(acceptance->error);
      if (!*acceptance->changed) return;
      auto topology = acceptance->topology;
      const auto source = acceptance->source;
      const auto transform = acceptance->transform;
      if (!topology) throw std::logic_error("Accepted retopology has no finalized topology");
      lock.unlock();
      publish_retopology(scene, *topology, source, transform, cancelled);
    }, retopo_source_, {}, acceptance->work, acceptance->changed, {}, session);
  } catch (...) {
    {
      std::lock_guard lock(retopo_mutex_);
      acceptance->admission = RetopoAcceptance::Admission::Rejected;
      std::erase_if(retopo_jobs_, [&](const auto &queued) { return queued.acceptance == acceptance; });
    }
    retopo_pending_ = pending;retopo_accepting_ = false;
    state_.retopology = previous;
    retopo_stroking_ = stroking; retopo_sampled_ = sampled; desktop_ = desktop;
    retopo_changed_.notify_all();
    throw;
  }
  {
    std::lock_guard lock(retopo_mutex_);
    acceptance->admission = RetopoAcceptance::Admission::Accepted;
  }
  if (stroking) input_.cancel();
  // Both the input queue and accepted-result waiter use this condition.
  retopo_changed_.notify_all();
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
    const auto obsolete = [&] { return stopping_.load() || job.session != retopo_session_.load(); };
    auto cancelled = [&] { return obsolete() || (job.acceptance && job.acceptance->work->cancelled.load()); };
    bool retired = false;
    try {
      if (job.acceptance) {
        std::unique_lock lock(retopo_mutex_);
        retopo_changed_.wait(lock, [&] {
          return job.acceptance->admission != RetopoAcceptance::Admission::Pending || obsolete();
        });
        if (job.acceptance->admission != RetopoAcceptance::Admission::Accepted) continue;
      }
      if (cancelled()) {
        if (job.acceptance) throw std::runtime_error("Retopology acceptance cancelled");
        continue;
      }
      // Draft geometry and local history cannot advance before the immutable
      // model publication succeeds, including failures after a real edit.
      std::unique_ptr<RetopoWorkspace> candidate;
      if (job.initial) candidate = std::make_unique<RetopoWorkspace>(*job.initial, cancelled);
      else {
        if (!workspace || session != job.session) throw std::logic_error("Retopology workspace is unavailable");
        candidate = std::make_unique<RetopoWorkspace>(*workspace);
      }
      if (job.action) candidate->edit(*job.action, job.point, job.snap_mm, cancelled);
      if (job.accept) {
        if (!job.acceptance) throw std::logic_error("Retopology acceptance lost its ordered asset owner");
        candidate->edit(RetopoAction::EndGesture, {}, job.snap_mm, cancelled);
        if (candidate->view().topology->faces().empty()) throw std::logic_error("Make at least one face before applying the patch");
      }
      if (!job.acceptance && cancelled()) continue;
      std::lock_guard lock(model_mutex_);
      if (obsolete() || !state_.retopology) continue;
      if (retopo_source_.generation != state_.generation || retopo_source_.revision != state_.revision ||
          retopo_source_.object != scene_.active().id) {
        cancel_retopology_locked(); state_.error = "Retopology source changed; reopen the workspace"; publish_locked(true); continue;
      }
      --retopo_pending_;
      retired = true;
      if (cancelled()) continue;
      auto view = std::make_shared<RetopoView>(candidate->view());
      view->busy = retopo_pending_ != 0; view->committing = retopo_accepting_; view->snap_mm = retopo_snap_mm_;
      const auto previous = state_.retopology;
      auto previous_error = state_.error, previous_message = state_.message;
      try {
        state_.retopology = std::move(view);
        state_.error.clear(); state_.message.clear();
        publish_locked();
      } catch (...) {
        state_.retopology = previous;
        state_.error = std::move(previous_error); state_.message = std::move(previous_message);
        throw;
      }
      // Swap without destroying the old BVH/history under the model lock.
      // candidate is destroyed after this lock guard leaves scope.
      workspace.swap(candidate); session = job.session;
      if (job.acceptance) {
        // Only hand the file owner a draft whose immutable view succeeded.
        // A failed preview must fail the dependent Save, retaining the exact
        // pre-Apply workspace (including any unfinished held gesture).
        std::lock_guard input_lock(retopo_mutex_);
        job.acceptance->topology = workspace->view().topology;
        job.acceptance->source = workspace->source_object();
        job.acceptance->transform = workspace->view().transform;
        *job.acceptance->changed = workspace->view().changed;
        job.acceptance->ready = true;
      }
    } catch (const std::exception &e) {
      if (job.acceptance) {
        {
          std::lock_guard lock(retopo_mutex_);
          if (!job.acceptance->ready) {
            job.acceptance->error = std::current_exception();
            job.acceptance->ready = true;
          }
        }
        retopo_changed_.notify_all();
        std::lock_guard lock(model_mutex_);
        if (!obsolete() && state_.retopology && !retired && retopo_pending_) --retopo_pending_;
        // The already-ordered file owner publishes failure and retires dependent
        // Saves. Do not race it with a second independently allocated error view.
        continue;
      }
      std::lock_guard lock(model_mutex_);
      if (cancelled() || !state_.retopology) continue;
      if (!retired && retopo_pending_) --retopo_pending_;
      if (job.accept) retopo_accepting_ = false;
      try {
        auto view = workspace && session == job.session ? std::make_shared<RetopoView>(workspace->view())
                                                       : std::make_shared<RetopoView>(*state_.retopology);
        view->error = e.what(); view->busy = retopo_pending_ != 0; view->committing = retopo_accepting_;
        state_.retopology = std::move(view); state_.error = e.what(); publish_locked();
      } catch (...) {
        // Exhausting the error reporter must not terminate the worker. Keep
        // the immutable draft; a healthy subsequent input can publish again.
        try { state_.error = "Out of memory"; } catch (...) {}
      }
    }
    retopo_changed_.notify_all();
  }
}
} // namespace dome
