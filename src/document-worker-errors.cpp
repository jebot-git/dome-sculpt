// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "text-utf8.hpp"
namespace dome {
namespace {
std::string worker_message(const std::exception_ptr &error) {
  try{if(error)std::rethrow_exception(error);}
  catch(const std::bad_alloc&){return "Insufficient memory; retry when memory is available.";}
  catch(const std::exception&e){const std::string_view value=e.what();return text::valid_utf8(value)?text::utf8_prefix(value,512):"Authoring worker failed; invalid error text";}
  catch(...){return "Authoring worker failed with an unknown exception";}
  return "Authoring worker failed";
}
}
void Document::signal_worker_notices_locked() noexcept {
  const bool pending=bool(worker_notices_[1]),input=bool(worker_notices_[0]);
  const bool completion=pending&&bool(worker_notices_[1]->completion);
  // Both consumers use their predicate's mutex, preventing lost wakeups.
  {std::lock_guard lock(file_mutex_);worker_notices_pending_.store(pending);worker_completion_pending_.store(completion);}
  {std::lock_guard lock(input_mutex_);input_notice_pending_.store(input);}
  {std::lock_guard lock(native_mutex_);native_notice_pending_.store(bool(worker_notices_[2]));}
  file_changed_.notify_all();input_changed_.notify_all();native_changed_.notify_all();
}
void Document::input_worker_error_locked(std::exception_ptr error) noexcept {
  WorkerNotice notice;notice.error=std::move(error);notice.generation=state_.generation;
  notice.publication=published_.load();notice.input=true;
  worker_notices_[0]=std::move(notice);retry_worker_notices_locked(true);
}
void Document::file_worker_error_locked(FileJob &job,std::exception_ptr error) noexcept {
  WorkerNotice notice;notice.error=std::move(error);notice.generation=job.generation;
  notice.publication=published_.load();notice.completion=std::move(job.completion);
  notice.label=std::move(job.operation_label);notice.discover_import=job.discover_import;notice.import_request=job.import_request;
  notice.discover_recovery=job.discover_recovery;notice.cut_session=job.cut_session;notice.retopo_session=job.retopo_session;
  notice.cancelled=(job.export_work&&job.export_work->cancelled())||(job.work&&job.work->cancelled.load());
  notice.held_weight=job.work&&job.work->held_weight;notice.export_job=bool(job.export_work);
  retire_export_locked(job.export_work);
  // Nonallocating bookkeeping happens before any diagnostic/view allocation.
  if(job.generation==state_.generation) {
    if(job.open_project||job.prepare_mesh||job.uv_mesh||job.restore_recovery||job.scene_operation)state_.asset_busy=false;
    if(job.discover_recovery||job.restore_recovery)state_.recovery_busy=false;
    if(job.uv_mesh&&uv_&&uv_accept_pending_&&uv_session_==job.uv_session){
      uv_accept_pending_=false;invalidate_uv_locked();if(!job.uv_desktop)input_.resume_control(VrParameter::UvEdit);
    }
    if(job.scene_operation){
      if(asset_work_.load()==job.work)asset_work_.store({});
      restore_weight_view_locked(job.rollback_preview);
      if(job.retopo_session&&job.retopo_session==retopo_session_.load())retopo_accepting_=false;
    }
    if((job.open_project||job.prepare_mesh)&&asset_work_.load()==job.work)asset_work_.store({});
  }
  worker_notices_[1]=std::move(notice);retry_worker_notices_locked();
}
void Document::native_worker_error_locked(const std::shared_ptr<NativeStroke>&session,std::exception_ptr error) noexcept {
  // The detached draft never replaces authority on failure. Preserve the exact
  // exception without allocating strings, and never cancel a newer gesture.
  if(native_stroke_==session){
    native_stroke_.reset();desktop_=false;session->cancelled=true;++native_epoch_;
    // begin_native_locked cancelled SpatialInput before publication, so there
    // is no synchronous edit callback left to run during failure cleanup.
    input_.cancel();state_.native_samples=0;
    if(session->source.generation==state_.generation&&session->source.revision==state_.revision){
      std::lock_guard mesh(mesh_mutex_);pending_mesh_.reset();mesh_.store(session->original_preview);mesh_changed_.notify_all();
    }
    if(stopping_.load())return;
    WorkerNotice notice;notice.error=std::move(error);notice.generation=session->source.generation;notice.publication=published_.load();
    worker_notices_[2]=std::move(notice);retry_worker_notices_locked(false,true);
  }
}
void Document::retry_worker_notices_locked(bool input,bool native) noexcept {
  // Each worker retries its own slot. A long authoring/export job must never
  // delay controller recovery, and idle workers keep their normal indefinite wait.
  for(const size_t slot:{native?size_t(2):input?size_t(0):size_t(1)}) {
    auto &pending=worker_notices_[slot];if(!pending)continue;
    auto &notice=*pending;
    bool handled=false;
    try {
      const bool same_generation=notice.generation==state_.generation;
      const bool same_publication=notice.publication==published_.load();
      const bool same_actor=same_generation&&state_.object_grabbing&&
        notice.publication->held_object==state_.held_object&&notice.publication->edit_authority==state_.edit_authority;
      const bool same_held=notice.input&&same_actor;
      if(notice.input&&same_generation) {
        if(same_held)finish_object_grab_locked(false,true);
        if(same_publication){finish_control_locked(false);if(!desktop_)input_.cancel();}
      }
      if(notice.held_weight&&same_actor&&!desktop_&&!object_navigation_.grabbing())finish_object_grab_locked(false,true);
      if(notice.discover_import) {
        if(same_generation&&!stopping_.load()&&import_view_&&import_request_.load()==notice.import_request) {
          auto view=std::make_shared<ImportView>(*import_view_);view->busy=false;view->error=worker_message(notice.error);
          auto previous=import_view_;import_view_=std::move(view);
          try{publish_locked(false,false);}catch(...){import_view_=std::move(previous);throw;}
        }
      }else if(same_generation) {
        // Do not overwrite status from a newer accepted operation/document.
        // Its successful publication already includes nonallocating cleanup.
        bool view_changed=false;
        if(!asset_work_.load()) {
          if(notice.cut_session&&notice.cut_session==cut_session_.load()&&state_.cut&&state_.cut->committing){
            auto view=std::make_shared<CutView>(*state_.cut);view->committing=false;view->error=worker_message(notice.error);state_.cut=std::move(view);view_changed=true;
          }
          if(notice.retopo_session&&notice.retopo_session==retopo_session_.load()&&state_.retopology&&state_.retopology->committing){
            auto view=std::make_shared<RetopoView>(*state_.retopology);view->busy=view->committing=false;view->error=worker_message(notice.error);state_.retopology=std::move(view);view_changed=true;
          }
        }
        if(same_publication||same_held) {
          if(notice.discover_recovery)recovery_discovery_error_=worker_message(notice.error);
          if(notice.cancelled){state_.error.clear();state_.message=notice.label.empty()?"Export cancelled":notice.label+" cancelled";}
          else state_.error=worker_message(notice.error);
          publish_locked(false,false);
        }else if(view_changed)publish_locked(false,false);
      }else if(notice.export_job&&same_publication) {
        // Accepted exports outlive document replacement. Retire their status
        // counter without applying the old document's failure to the new one.
        if(notice.cancelled&&state_.message=="Cancelling background work...")state_.message="Export cancelled";
        publish_locked(false,false);
      }
      handled=true;
    }catch(...){/* memory pressure: original record and worker remain live */}
    // Preserve the normal ready-future/native-status ordering. Under memory
    // pressure a failed publication must not strand the original exception.
    if(notice.completion) {
      try{notice.completion->set_exception(notice.error);notice.completion.reset();}
      catch(const std::future_error&){notice.completion.reset();} // already satisfied/no-state
      catch(...){/* preserve this promise and original exception for retry */}
    }
    if(handled&&!notice.completion)pending.reset();
  }
  signal_worker_notices_locked();
}
} // namespace dome
