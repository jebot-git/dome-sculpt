// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/document.hpp"
namespace dome {
// Permission publication rollback is allocation-free. Cancelled unaccepted work
// deliberately stays cancelled; it cannot later publish under a restored lease.
class Document::AuthorityTransition {
 Document &d;std::shared_ptr<const EditorState> publication;uint64_t active,held,epoch;
 bool mode,isolate,grabbing,desktop,done=false;CanvasNavigation navigation,object_navigation;
 CanvasTransform start,preview;
 std::shared_ptr<const HeldAffine> affine;
public:
 explicit AuthorityTransition(Document &v):d(v),publication(v.published_.load()),active(v.scene_.active().id),held(v.state_.held_object),epoch(v.state_.edit_authority),mode(v.state_.object_grab_mode),isolate(v.state_.isolate_held),grabbing(v.state_.object_grabbing),desktop(v.desktop_),navigation(v.navigation_),object_navigation(v.object_navigation_),start(v.held_start_transform_),preview(v.held_preview_transform_),affine(v.held_affine_){}
 void commit()noexcept{done=true;}
 ~AuthorityTransition()noexcept{if(done)return;d.state_.object_grab_mode=mode;d.state_.isolate_held=isolate;d.state_.object_grabbing=grabbing;d.state_.held_object=held;d.state_.edit_authority=epoch;d.state_.active_object=active;d.desktop_=desktop;d.navigation_=navigation;d.object_navigation_=object_navigation;d.held_start_transform_=start;d.held_preview_transform_=preview;d.held_affine_=std::move(affine);
  if(std::any_of(d.scene_.objects().begin(),d.scene_.objects().end(),[&](const auto&o){return o.id==active;})){d.scene_.select(active);d.input_.refresh_volume(d.volume());d.bound_volume_=d.scene_.active().volume;d.last_volume_=d.volume().revision();d.last_paint_=d.scene_.active().mesh_active?d.scene_.active().painted->revision():0;}
  d.published_.store(std::move(publication));}
};
}
