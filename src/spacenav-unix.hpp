// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <array>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <string>
#include <vector>
namespace dome::desktop {
// Native local spacenav event protocol. Layout reference: FreeSpacenav libspnav
// 476a17b6087146f95799e3a2a12b4d13cb4d2818, BSD-3-Clause (see provenance).
struct SpacePacket {
  enum class Kind { Motion, Press, Release, Device, Config, Ignored, Invalid };
  Kind kind = Kind::Invalid;
  std::array<int32_t,6> axes{};
  uint32_t period = 0; // Daemon metadata, never a camera integration clock.
  uint16_t button = 0;
};
struct SpacePump {
  bool connected = false, drained = false, backlog = false;
  size_t packets = 0;
  uint64_t epoch = 0;
};
std::vector<std::string> spacenav_socket_paths();
class SpaceUnix {
public:
  explicit SpaceUnix(std::vector<std::string> paths = spacenav_socket_paths());
  ~SpaceUnix();
  SpaceUnix(const SpaceUnix &) = delete;
  SpaceUnix &operator=(const SpaceUnix &) = delete;
  void enable(bool, double now);
  void reconnect(double now);
  // A context boundary taints an incomplete old packet without losing framing.
  void taint_partial() noexcept;
  template<class Sink> SpacePump pump(double now,Sink &&sink) {
    using Function=std::remove_reference_t<Sink>;
    return pump_erased(now,[](const void *context,const SpacePacket &packet) {
      auto *function=const_cast<Function*>(static_cast<const Function*>(context));
      (*function)(packet);
    },std::addressof(sink));
  }
  bool connected() const noexcept { return fd_ >= 0 && !connecting_; }
  uint64_t epoch() const noexcept { return epoch_; }
  const std::string &path() const noexcept {
    static const std::string empty;
    return paths_.empty()?empty:paths_[path_index_];
  }
  int descriptor() const noexcept { return fd_; }
private:
  SpacePump pump_erased(double,void(*)(const void*,const SpacePacket&),const void*);
  void close(double now) noexcept;
  void connect_ready(double now);
  std::vector<std::string> paths_;
  size_t path_index_ = 0, next_path_ = 0, used_ = 0;
  int fd_ = -1;
  bool enabled_ = true, connecting_ = false, tainted_ = false;
  double retry_at_ = 0, connect_until_ = 0;
  uint64_t epoch_ = 0;
  std::array<unsigned char,32> partial_{};
};
} // namespace dome::desktop
