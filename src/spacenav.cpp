// SPDX-License-Identifier: GPL-3.0-or-later
#include "spacenav.hpp"
namespace dome::desktop {
SpaceNavigator::SpaceNavigator(std::vector<std::string> sockets):unix_(std::move(sockets)){}
void SpaceNavigator::clear_input(bool connection) noexcept {
  axes_={};armed_=false;need_drain_=true;
  // Legacy protocol0 does not announce device removal. A held button can
  // disappear without a release; do not make cap neutral depend on that latch.
  // Its release knowledge is revoked, so repeat presses still cannot act.
  if(connection)released_.reset();
  else released_ &= ~held_;
  held_.reset();
  unix_.taint_partial();
}
void SpaceNavigator::fence(double now) noexcept {
  clear_input();
  if(std::isfinite(now)&&now>=0)previous_=now;
}
void SpaceNavigator::settings(SpaceSettings value,double now) {
  value.validate();unix_.enable(value.enabled,now);settings_=value;fence(now);
}
void SpaceNavigator::reconnect(double now){unix_.reconnect(now);clear_input(true);previous_=now;}
const char *SpaceNavigator::status() const noexcept {
  if(!settings_.enabled)return "3D mouse navigation is off";
  if(!unix_.connected())return "No spacenav daemon connected";
  if(!owns_)return "3D mouse paused while another control owns input";
  if(!armed_)return "Release the 3D mouse to resume navigation";
  return "Connected to spacenav; navigation ready";
}
SpaceUpdate SpaceNavigator::tick(SpaceView &view,bool owns_view,double now) {
  if(!std::isfinite(now)||now<0)throw std::invalid_argument("Invalid 3D mouse clock");
  SpaceUpdate result;
  const bool allowed=settings_.enabled&&owns_view;
  if(!started_){previous_=now;started_=true;}
  double seconds=now-previous_;
  bool changed=allowed!=owns_;
  bool stalled=seconds<0||seconds>.15;
  if(changed||stalled)fence(now);
  owns_=allowed;
  const auto old_axes=axes_;
  const bool previously_armed=armed_;
  const double deadline=last_motion_+.15;
  // Bound integration to the freshness interval, not to packet count/period.
  double elapsed=std::max(0.,std::min(now,deadline)-previous_);
  if(!allowed||changed||stalled)elapsed=0;
  const bool held_motion=std::any_of(old_axes.begin(),old_axes.end(),[](float x){return x!=0;});
  bool expired=previously_armed&&held_motion&&now>deadline;
  const bool expiry_only=expired&&!need_drain_;
  if(expired)clear_input();
  previous_=now;
  bool was_draining=need_drain_,neutral=false,invalidated=false;
  auto action=SpaceUpdate::Action::None;
  auto pump=unix_.pump(now,[&](const SpacePacket &packet){
    if(unix_.epoch()!=connection_) {
      connection_=unix_.epoch();clear_input(true);was_draining=true;
    }
    switch(packet.kind) {
    case SpacePacket::Kind::Motion: {
      auto mapped=space_axes(packet.axes,settings_);
      bool zero=std::none_of(mapped.begin(),mapped.end(),[](float x){return x!=0;});
      if(!allowed||was_draining)return;
      if(!armed_){neutral=zero;return;}
      axes_=mapped;last_motion_=now;break;
    }
    case SpacePacket::Kind::Press: {
      auto b=packet.button;
      // Draining/foreign-owner packets may precede an unplug with no release.
      // Revoke action knowledge without rebuilding a stale held latch.
      if(!allowed||was_draining||!armed_) {
        held_.reset(b);released_.reset(b);break;
      }
      bool edge=!held_[b]&&released_[b];held_.set(b);released_.reset(b);
      if(edge&&b<2) {
        bool all=(b==1)!=settings_.swap_buttons;
        action=all?SpaceUpdate::Action::FrameAll:SpaceUpdate::Action::FrameSelection;
      }
      break;
    }
    case SpacePacket::Kind::Release:
      held_.reset(packet.button);released_.set(packet.button);break;
    case SpacePacket::Kind::Device:case SpacePacket::Kind::Config:
      clear_input(true);was_draining=true;invalidated=true;break;
    case SpacePacket::Kind::Invalid:
      clear_input();was_draining=true;invalidated=true;break;
    case SpacePacket::Kind::Ignored:break;
    }
  });
  if(pump.epoch!=connection_) {
    connection_=pump.epoch;clear_input(true);was_draining=true;invalidated=true;
  }
  if(!pump.connected||pump.backlog) {
    clear_input(!pump.connected);invalidated=true;
  } else if(need_drain_) {
    if(pump.drained)need_drain_=false;
  } else if(allowed&&!armed_&&neutral&&held_.none()&&pump.drained) {
    armed_=true;axes_={};last_motion_=now;
  }
  if(!allowed||invalidated)return result;
  if(was_draining) {
    if(expiry_only&&previously_armed&&elapsed>0)
      result.moved=advance_space_view(view,old_axes,settings_,elapsed);
    return result;
  }
  if(action!=SpaceUpdate::Action::None) {
    result.action=action;fence(now);return result;
  }
  if(previously_armed && elapsed>0)
    result.moved=advance_space_view(view,old_axes,settings_,elapsed);
  return result;
}
} // namespace dome::desktop
