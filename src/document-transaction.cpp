// SPDX-License-Identifier: GPL-3.0-or-later
#include "document-transaction.hpp"
namespace dome {
Document::Transaction::Transaction(Document &d):d_(d),before_(d.scene_),state_(d.state_),input_(d.input_),
    control_before_(d.control_before_),bound_(d.bound_volume_),source_(d.native_source_scene_),
    path_(d.path_),recovery_source_(d.recovery_source_),volume_(d.last_volume_),paint_(d.last_paint_),
    content_(d.content_),next_(d.next_content_),saved_(d.saved_content_),recovery_(d.recovery_content_),rig_(d.rig_object_),
    desktop_(d.desktop_),anchored_(d.anchored_),control_changed_(d.control_changed_),uv_accept_(d.uv_accept_pending_),uv_solving_(d.uv_solving_),control_source_(d.control_source_),import_settings_(d.import_settings_),import_view_(d.import_view_),import_source_(d.import_source_),import_request_(d.import_request_.load()),work_(d.asset_work_.load()),native_(d.native_stroke_),held_affine_(d.held_affine_),held_start_(d.held_start_transform_),held_preview_(d.held_preview_transform_),object_navigation_(d.object_navigation_) {
    if(d.transaction_)throw std::logic_error("Nested document transaction");
    d.transaction_=this;
  }

void Document::Transaction::commit(bool checkpoint) noexcept {
    committed_=true;d_.transaction_=nullptr;
    if(advance_import_)d_.import_request_.store(import_request_+1);
    if(undo_source_)undo_source_->pop_back();
    if(recorded_)while(d_.undo_.size()>128)d_.undo_.pop_front();
    if(rebind_){std::lock_guard lock(d_.input_mutex_);d_.inputs_.clear();d_.reset_input_=false;d_.input_changed_.notify_all();}
    if(finish_uv_){d_.invalidate_uv_locked();++d_.uv_session_;}
    if(finish_retopo_)d_.cancel_retopology_locked();
    if(finish_cut_)d_.cancel_cut_locked();
    if(finish_polygon_)d_.cancel_polygon_locked(false);
    else if(cancel_polygon_)d_.cancel_polygon_locked();
    // Persistence observes only the fully published endpoint. It cannot turn a
    // successful user commit into an exception or enqueue a rejected candidate.
    if(checkpoint)try{d_.checkpoint_locked();}catch(...){}
  }
Document::Transaction::~Transaction() noexcept {
    if(committed_)return;
    if(recorded_){d_.undo_.pop_back();d_.redo_.swap(redo_);}
    if(replaced_){d_.undo_.swap(undo_);d_.redo_.swap(redo_);}
    if(undo_destination_)undo_destination_->pop_back();
    d_.scene_=std::move(before_);d_.state_=std::move(state_);d_.input_=std::move(input_);
    d_.control_before_=std::move(control_before_);d_.control_changed_=control_changed_;d_.control_source_=control_source_;
    if(finish_uv_)d_.uv_=std::move(removed_uv_);
    d_.uv_accept_pending_=uv_accept_;d_.uv_solving_=uv_solving_;
    d_.import_settings_=std::move(import_settings_);d_.import_view_=std::move(import_view_);d_.import_source_=import_source_;d_.import_request_.store(import_request_);
    d_.asset_work_.store(std::move(work_));d_.native_stroke_=std::move(native_);
    d_.held_affine_=std::move(held_affine_);d_.held_start_transform_=held_start_;d_.held_preview_transform_=held_preview_;d_.object_navigation_=object_navigation_;
    d_.bound_volume_=std::move(bound_);d_.native_source_scene_=std::move(source_);
    d_.path_=std::move(path_);d_.recovery_source_=std::move(recovery_source_);
    d_.last_volume_=volume_;d_.last_paint_=paint_;d_.content_=content_;d_.next_content_=next_;
    d_.saved_content_=saved_;d_.recovery_content_=recovery_;d_.rig_object_=rig_;
    d_.desktop_=desktop_;d_.anchored_=anchored_;d_.transaction_=nullptr;
  }
}
