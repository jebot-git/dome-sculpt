// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "document-transaction.hpp"
namespace dome {
void Document::enable_recovery(std::filesystem::path root) {
  auto service = std::make_unique<RecoveryService>(root);
  std::lock_guard lock(model_mutex_);
  if (recovery_)
    throw std::logic_error("Recovery is already enabled");
  recovery_root_ = std::move(root);
  recovery_source_ = {new_recovery_id(), state_.generation, path_};
  service->set_source(recovery_source_);
  recovery_ = std::move(service);
  publish_locked();
}
void Document::checkpoint_locked() {
  if (!recovery_) return;
  auto status = recovery_->status();
  state_.recovery_error = status.error.empty() ? recovery_discovery_error_ : status.error;
  if (content_ == saved_content_ || content_ == recovery_content_ ||
      action_active_locked() || pending_locked() || control_before_ ||
      state_.asset_busy) return;
  try {
    // A preceding successful Open/Save may have committed while allocation of
    // the best-effort service source update failed. Retry synchronization before
    // enqueueing a later edit, instead of leaving recovery permanently stale.
    recovery_->set_source(recovery_source_);
    auto result = recovery_->enqueue(
        {recovery_source_, state_.revision, content_, scene_.snapshot()});
    if (result == RecoveryEnqueue::Accepted || result == RecoveryEnqueue::Coalesced)
      recovery_content_ = content_;
  } catch (const std::exception &e) {
    state_.recovery_error = e.what();
  }
}
RecoveryStatus Document::recovery_status() const {
  std::lock_guard lock(model_mutex_);
  return recovery_ ? recovery_->status() : RecoveryStatus{};
}
void Document::refresh_recovery() {
  std::lock_guard lock(model_mutex_);
  refresh_recovery_locked();
}
void Document::refresh_recovery_locked() {
  if (!recovery_) throw std::logic_error("Recovery is not enabled");
  if (state_.recovery_busy) throw std::logic_error("Recovery work is pending");
  std::lock_guard file_lock(file_mutex_);
  if (files_.size() >= 8) throw std::runtime_error("File work queue is full");
  FileJob job{};
  job.path = recovery_root_;
  job.generation = state_.generation;
  job.content = content_;
  job.discover_recovery = true;
  Transaction transaction(*this);files_.push_back(std::move(job));
  try{state_.recovery_busy=true;recovery_content_=0;publish_locked(false,false);transaction.commit();}
  catch(...){files_.pop_back();throw;}
  file_changed_.notify_one();
}
void Document::restore_recovery(size_t index) {
  std::lock_guard lock(model_mutex_);
  restore_recovery_locked(index);
}
void Document::restore_recovery_locked(size_t index) {
  if (!recovery_) throw std::logic_error("Recovery is not enabled");
  if (state_.dirty || action_active_locked() || control_before_ ||
      state_.asset_busy || state_.recovery_busy)
    throw std::logic_error("Save the current project before restoring recovery");
  if (index >= recovery_catalog_.size())
    throw std::out_of_range("No recovery project selected; refresh recovery first");
  std::lock_guard file_lock(file_mutex_);
  if (files_.size() >= 8) throw std::runtime_error("File work queue is full");
  FileJob job{};
  job.generation = state_.generation;
  job.content = content_;
  job.restore_recovery = recovery_catalog_[index];
  Transaction transaction(*this);files_.push_back(std::move(job));
  try{state_.recovery_busy=true;state_.asset_busy=true;publish_locked(false,false);transaction.commit();}
  catch(...){files_.pop_back();throw;}
  file_changed_.notify_one();
}
} // namespace dome
