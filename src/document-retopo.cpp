// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "document-retopo-input.hpp"
#include <type_traits>
namespace dome {
namespace {
// Count actual queued operations, not batches. A frame cannot multiply the
// historical 254 ordinary-operation budget by hiding many steps in each job.
template<class Jobs> size_t queued_retopo_cost(const Jobs &jobs) {
  size_t cost = 0;
  for (const auto &job : jobs)
    cost += job.acceptance && job.acceptance->input_batch
              ? job.acceptance->steps.size() + bool(job.initial) : 1;
  return cost;
}
}
struct Document::RetopoInputScope::State {
  EditorState view;
  SpatialInput input;
  CanvasNavigation navigation, object_navigation;
  std::optional<Scene> control_before;
  AssetSource source, control_source;
  uint64_t control_object;
  size_t pending;
  float snap, control_snap;
  bool stroking, sampled, accepting, desktop, anchored, control_changed;
  bool retire_inputs = false;
  std::shared_ptr<RetopoAcceptance> admission;
  explicit State(Document &d)
      : view(d.state_), input(d.input_), navigation(d.navigation_), object_navigation(d.object_navigation_),
        control_before(d.control_before_), source(d.retopo_source_), control_source(d.control_source_),
        control_object(d.control_object_), pending(d.retopo_pending_), snap(d.retopo_snap_mm_),
        control_snap(d.control_retopo_mm_), stroking(d.retopo_stroking_), sampled(d.retopo_sampled_),
        accepting(d.retopo_accepting_), desktop(d.desktop_), anchored(d.anchored_),
        control_changed(d.control_changed_), admission(std::make_shared<RetopoAcceptance>()) {}
};
Document::RetopoInputScope::RetopoInputScope(Document &d, bool active) : d_(d) {
  if (!active) return;
  auto state = std::make_unique<State>(d);
  state->admission->input_batch = true;
  RetopoJob job{d.retopo_session_.load(), {}, {}, {}, d.retopo_snap_mm_};job.acceptance = state->admission;
  {
    std::lock_guard lock(d.retopo_mutex_);
    if (d.retopo_jobs_.size() >= 256) throw std::length_error("Retopology input queue is full");
    if (std::any_of(d.retopo_jobs_.begin(),d.retopo_jobs_.end(),[](const auto &j) {
          return j.acceptance && j.acceptance->input_batch &&
                 j.acceptance->admission == RetopoAcceptance::Admission::Pending;
        })) throw std::logic_error("Nested retopology input transaction");
    d.retopo_jobs_.push_back(std::move(job));
  }
  state_ = std::move(state);
}
void Document::RetopoInputScope::retire_inputs() noexcept {
  if(state_)state_->retire_inputs = true;
}
void Document::RetopoInputScope::commit() noexcept {
  if (!state_) return;
  auto &a = state_->admission;
  {
    std::lock_guard lock(d_.retopo_mutex_);
    if (a->next_session) d_.retopo_session_ = *a->next_session;
    if (a->cancel_workspace) {
      ++d_.retopo_session_;
      d_.retopo_jobs_.clear();d_.retopo_pending_ = 0;
    } else if (a->counted) a->admission = RetopoAcceptance::Admission::Accepted;
    else std::erase_if(d_.retopo_jobs_,[&](const auto &j){return j.acceptance == a;});
    if (a->cancel_work) a->cancel_work->cancelled = true;
  }
  if(state_->retire_inputs) {
    std::lock_guard lock(d_.input_mutex_);
    const auto epoch = d_.state_.input_epoch;
    const bool reset = d_.reset_input_ && (d_.inputs_.empty() || d_.inputs_.front().first == epoch);
    std::erase_if(d_.inputs_,[epoch](const auto &sample){return sample.first != epoch;});
    d_.reset_input_ = reset;d_.input_changed_.notify_all();
  }
  state_.reset();d_.retopo_changed_.notify_all();
}
Document::RetopoInputScope::~RetopoInputScope() noexcept {
  if (!state_) return;
  {
    std::lock_guard lock(d_.retopo_mutex_);
    state_->admission->admission = RetopoAcceptance::Admission::Rejected;
    std::erase_if(d_.retopo_jobs_,[&](const auto &j){return j.acceptance == state_->admission;});
  }
  static_assert(std::is_nothrow_move_assignable_v<EditorState>);
  static_assert(std::is_nothrow_move_assignable_v<SpatialInput>);
  static_assert(std::is_nothrow_move_assignable_v<CanvasNavigation>);
  static_assert(std::is_nothrow_move_assignable_v<std::optional<Scene>>);
  d_.state_ = std::move(state_->view);d_.input_ = std::move(state_->input);
  d_.navigation_ = std::move(state_->navigation);d_.object_navigation_ = std::move(state_->object_navigation);d_.control_before_ = std::move(state_->control_before);
  d_.retopo_source_ = state_->source;d_.control_source_ = state_->control_source;
  d_.control_object_ = state_->control_object;d_.retopo_pending_ = state_->pending;
  d_.retopo_snap_mm_ = state_->snap;d_.control_retopo_mm_ = state_->control_snap;
  d_.retopo_stroking_ = state_->stroking;d_.retopo_sampled_ = state_->sampled;
  d_.retopo_accepting_ = state_->accepting;d_.desktop_ = state_->desktop;
  d_.anchored_ = state_->anchored;d_.control_changed_ = state_->control_changed;
  d_.retopo_changed_.notify_all();
}
void Document::begin_retopology() {
  std::lock_guard lock(model_mutex_);
  finish_locked();
  RetopoInputScope admission(*this,true);
  begin_retopology_locked();
  admission.commit();
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
  if (retopo_session_.load() == UINT64_MAX) throw std::overflow_error("Retopology session exhausted");
  auto initial = std::make_shared<const Scene>(scene_.snapshot());
  auto view = std::make_shared<RetopoView>();view->busy = true;view->snap_mm = retopo_snap_mm_;
  {
    std::lock_guard lock(retopo_mutex_);
    if (retopo_jobs_.empty() || !retopo_jobs_.back().acceptance ||
        !retopo_jobs_.back().acceptance->input_batch ||
        retopo_jobs_.back().acceptance->admission != RetopoAcceptance::Admission::Pending)
      throw std::logic_error("Retopology opening needs its caller transaction");
    if (queued_retopo_cost(retopo_jobs_) >= 254) throw std::length_error("Retopology input queue is full");
    auto &job = retopo_jobs_.back();job.initial = std::move(initial);
    job.session = retopo_session_.load() + 1;job.snap_mm = retopo_snap_mm_;
    job.acceptance->next_session = job.session;job.acceptance->counted = true;
    ++retopo_pending_;
  }
  retopo_source_ = {state_.generation, state_.revision, o.id,state_.edit_authority};
  retopo_stroking_ = retopo_sampled_ = retopo_accepting_ = false;
  state_.weights.enabled = false;state_.retopology = std::move(view);
  state_.error.clear();state_.message.clear();
  publish_locked(true); // Turn off an existing weight heatmap, if any.
  retopo_source_.revision = state_.revision;

}
void Document::queue_retopology_locked(RetopoAction action, Vec3 point, bool coalescible) {
  if (!state_.retopology || !state_.retopology->source || retopo_accepting_)
    throw std::logic_error("Wait for the retopology source before editing");
  std::lock_guard lock(retopo_mutex_);
  if (retopo_jobs_.empty() || !retopo_jobs_.back().acceptance ||
      !retopo_jobs_.back().acceptance->input_batch ||
      retopo_jobs_.back().acceptance->admission != RetopoAcceptance::Admission::Pending)
    throw std::logic_error("Retopology input needs its caller transaction");
  auto &a = *retopo_jobs_.back().acceptance;
  auto view = std::make_shared<RetopoView>(*state_.retopology);view->busy = true;
  const bool replace = coalescible && !a.steps.empty() && a.steps.back().coalescible;
  const size_t limit = action == RetopoAction::EndGesture || action == RetopoAction::CancelGesture ? 256 : 254;
  if (!replace && queued_retopo_cost(retopo_jobs_) >= limit)
    throw std::length_error("Retopology input queue is full");
  RetopoStep step{action,point,retopo_snap_mm_,coalescible};
  if (replace) a.steps.back() = std::move(step);else a.steps.push_back(std::move(step));
  if (!a.counted) {++retopo_pending_;a.counted = true;}
  state_.retopology = std::move(view);

}
void Document::edit_retopology(RetopoAction action) {
  if (action == RetopoAction::BeginGesture || action == RetopoAction::Sample ||
      action == RetopoAction::EndGesture || action == RetopoAction::CancelGesture)
    throw std::invalid_argument("Use the input gesture interface for retopology points");
  std::lock_guard lock(model_mutex_);
  RetopoInputScope admission(*this,true);
  if (retopo_stroking_) {cancel_action_locked();desktop_ = false;input_.cancel();}
  queue_retopology_locked(action);
  state_.error.clear();publish_locked();admission.commit();

}
void Document::retopology_distance(float mm) {
  if (!std::isfinite(mm) || mm < 1 || mm > 200) throw std::invalid_argument("Projection distance must be 1 to 200 mm");
  std::lock_guard lock(model_mutex_);
  if (retopo_accepting_) throw std::logic_error("Wait for the accepted patch");
  RetopoInputScope admission(*this,true);
  retopo_snap_mm_ = mm;
  if (state_.retopology) {
    auto view = std::make_shared<RetopoView>(*state_.retopology);
    view->snap_mm = mm; state_.retopology = std::move(view);
  }
  publish_locked();admission.commit();
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
  RetopoInputScope admission(*this,bool(state_.retopology));
  end_retopology_locked(accept);
  admission.commit();
}
void Document::end_retopology_locked(bool accept) {
  if (!state_.retopology) return;
  if (retopo_accepting_ && accept)
    throw std::logic_error("Cancel background work or wait for the accepted patch");
  if (!accept && retopo_accepting_ && state_.asset_busy) {
    // The ordered asset stage owns publication now. Request its cancellation;
    // on completion it restores the editable draft rather than losing work.
    auto work = asset_work_.load();
    state_.message = "Cancelling background work...";
    {std::lock_guard lock(retopo_mutex_);retopo_jobs_.back().acceptance->cancel_work = std::move(work);}
    publish_locked();return;
  }
  if (!accept) {
    {std::lock_guard lock(retopo_mutex_);
     if(retopo_session_.load()==UINT64_MAX)throw std::overflow_error("Retopology session exhausted");
     retopo_jobs_.back().acceptance->cancel_workspace = true;}
    retopo_stroking_ = retopo_sampled_ = retopo_accepting_ = false;
    state_.retopology.reset();desktop_ = false;input_.cancel();state_.error.clear();
    publish_locked(true);return;
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
        return stopping_.load() || (workspace && session != retopo_session_.load()) ||
               (!retopo_jobs_.empty() && (!retopo_jobs_.front().acceptance ||
                 retopo_jobs_.front().acceptance->admission != RetopoAcceptance::Admission::Pending));
      });
      if (stopping_) return;
      if (workspace && session != retopo_session_.load()) {
        // Destruction of a large BVH/history also stays off model/input locks.
        lock.unlock(); workspace.reset(); session = 0; continue;
      }
      job = std::move(retopo_jobs_.front()); retopo_jobs_.pop_front();
    }
    const auto obsolete = [&] { return stopping_.load() || job.session != retopo_session_.load(); };
    auto cancelled = [&] { return obsolete() || (job.accept && job.acceptance &&
                                                job.acceptance->work->cancelled.load()); };
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
        if (job.accept) throw std::runtime_error("Retopology acceptance cancelled");
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
      if (job.acceptance && job.acceptance->input_batch)
        for (const auto &step : job.acceptance->steps)
          candidate->edit(step.action,step.point,step.snap_mm,cancelled);
      if (job.action) candidate->edit(*job.action, job.point, job.snap_mm, cancelled);
      if (job.accept) {
        if (!job.acceptance) throw std::logic_error("Retopology acceptance lost its ordered asset owner");
        candidate->edit(RetopoAction::EndGesture, {}, job.snap_mm, cancelled);
        if (candidate->view().topology->faces().empty()) throw std::logic_error("Make at least one face before applying the patch");
      }
      if (!job.accept && cancelled()) continue;
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
      if (job.accept) {
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
      if (job.accept) {
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
