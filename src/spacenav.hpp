// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "spacenav-unix.hpp"
#include "spacenav-view.hpp"
#include <bitset>
namespace dome::desktop {
struct SpaceUpdate {
  bool moved=false;
  enum class Action { None, FrameSelection, FrameAll } action=Action::None;
};
class SpaceNavigator {
public:
  explicit SpaceNavigator(std::vector<std::string> sockets = spacenav_socket_paths());
  const SpaceSettings &settings() const noexcept {return settings_;}
  void settings(SpaceSettings,double now);
  void reconnect(double now);
  void fence(double now) noexcept;
  SpaceUpdate tick(SpaceView&,bool owns_view,double now);
  const char *status() const noexcept;
  const SpaceUnix &transport() const noexcept {return unix_;}
private:
  void clear_input(bool connection=false) noexcept;
  SpaceUnix unix_;
  SpaceSettings settings_;
  std::array<float,6> axes_{};
  std::bitset<128> held_,released_;
  bool armed_=false,need_drain_=true,owns_=false,started_=false;
  double previous_=0,last_motion_=0;
  uint64_t connection_=0;
};
} // namespace dome::desktop
