// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/document.hpp"
#include <type_traits>
namespace dome {
// Model-mutex-only rollback. Bulk geometry remains immutable/COW. History nodes
// and redo are staged, never copied or discarded before publication succeeds.
class Document::Transaction {
  Document &d_;
  Scene before_;
  EditorState state_;
  SpatialInput input_;
  std::optional<Scene> control_before_;
  std::shared_ptr<Volume> bound_;
  std::shared_ptr<const Scene> source_;
  std::filesystem::path path_;
  RecoverySource recovery_source_;
  uint64_t volume_,paint_,content_,next_,saved_,recovery_,rig_;
  bool desktop_,anchored_,control_changed_,uv_accept_,uv_solving_;
  AssetSource control_source_;
  std::deque<History> redo_,undo_;
  ImportSettings import_settings_;
  std::shared_ptr<const ImportView> import_view_;
  AssetSource import_source_;
  uint64_t import_request_=0;
  std::shared_ptr<AssetWork> work_;
  std::shared_ptr<NativeStroke> native_;
  std::shared_ptr<const HeldAffine> held_affine_;
  CanvasTransform held_start_,held_preview_;
  CanvasNavigation object_navigation_;
  bool native_end_=false;
  std::deque<History> *undo_source_=nullptr,*undo_destination_=nullptr;
  std::optional<UvWorkspace> removed_uv_;
  bool replaced_=false,recorded_=false,committed_=false,rebind_=false,finish_uv_=false,finish_retopo_=false,finish_cut_=false,advance_import_=false,finish_polygon_=false,cancel_polygon_=false;
public:
  explicit Transaction(Document &);
  Transaction(const Transaction &)=delete;
  const Scene &before()const{return before_;}
  void rebind(){rebind_=true;}
  void stage_native_end(){native_end_=true;}
  bool native_ending()const{return native_end_;}
  void release_native() noexcept {
    if(!native_end_)return;
    {std::lock_guard lock(d_.native_mutex_);native_->accepting=false;native_->accepted=true;}
    d_.native_changed_.notify_all();
  }
  size_t history_size(bool redo)const {const auto &history=redo?d_.redo_:d_.undo_;return history.size()-(undo_source_==&history?1:0);}
  void clear_history(){d_.undo_.swap(undo_);d_.redo_.swap(redo_);replaced_=true;}
  void record(History entry){
    if(recorded_||undo_source_)throw std::logic_error("Multiple history changes in one transaction");
    if(d_.next_content_==UINT64_MAX)throw std::overflow_error("Document history identity exhausted");
    entry.from=d_.content_;entry.to=d_.next_content_;
    d_.undo_.push_back(std::move(entry)); // The only history allocation.
    recorded_=true;redo_.swap(d_.redo_);
    d_.content_=d_.next_content_++;
  }
  void move_history(std::deque<History> &source,std::deque<History> &destination){
    if(recorded_||undo_source_)throw std::logic_error("Multiple history changes in one transaction");
    destination.push_back(source.back()); // Source stays exact until commit.
    undo_source_=&source;undo_destination_=&destination;
  }
  void finish_uv(){
    static_assert(std::is_nothrow_move_constructible_v<UvWorkspace>);
    removed_uv_=std::move(d_.uv_);d_.uv_.reset();d_.uv_accept_pending_=false;
    d_.control_before_.reset();d_.control_changed_=false;d_.state_.controls={};d_.state_.uv.reset();
    d_.input_.complete_control();finish_uv_=true;
  }
  void finish_retopo(){d_.state_.retopology.reset();finish_retopo_=true;}
  void finish_cut(){d_.state_.cut.reset();finish_cut_=true;}
  void cancel_polygon(){d_.state_.polygon.reset();d_.state_.polygon_object=0;cancel_polygon_=true;}
  void finish_polygon(bool handoff) {
    if(handoff){d_.state_.polygon_handoff=d_.state_.polygon;d_.state_.polygon_handoff_object=d_.state_.polygon_object;
      d_.state_.polygon_handoff_generation=d_.state_.generation;d_.state_.polygon_handoff_revision=UINT64_MAX;}
    d_.state_.polygon.reset();d_.state_.polygon_object=0;finish_polygon_=true;
  }
  // Private view reset may precede publication, but a scanner must not observe
  // irreversible cancellation until publication succeeds (including OOM).
  uint64_t advance_import_request(){
    if(import_request_==UINT64_MAX)throw std::overflow_error("Import request identity exhausted");
    advance_import_=true;return import_request_+1;
  }
  void invalidate_import(){advance_import_request();d_.import_view_.reset();d_.import_source_={};}
  void commit(bool checkpoint=true) noexcept;
  ~Transaction() noexcept;
};
}
