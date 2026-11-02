// SPDX-License-Identifier: GPL-3.0-or-later
#include "spacenav-unix.hpp"
#include <cerrno>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <poll.h>
#include <stdexcept>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>
namespace dome::desktop {
namespace {
void valid_time(double now) {
  if (!std::isfinite(now) || now < 0)
    throw std::invalid_argument("Invalid 3D mouse clock");
}
bool valid_path(const std::string &path) {
  return !path.empty() && path.size() < sizeof(sockaddr_un{}.sun_path) &&
         path.find('\0') == std::string::npos;
}
std::string config_socket() {
  int fd = open("/etc/spnavrc",O_RDONLY|O_NONBLOCK|O_CLOEXEC);
  if(fd<0)return {};
  struct Close { int fd; ~Close(){::close(fd);} } close{fd};
  struct stat st{};
  if(fstat(fd,&st) || !S_ISREG(st.st_mode) || st.st_size>65536)return {};
  std::array<char,65537> data{};size_t count=0;
  while(count<65536) {
    auto n=read(fd,data.data()+count,65536-count);
    if(n>0)count+=size_t(n);
    else if(n<0&&errno==EINTR)continue;
    else break;
  }
  std::string_view text(data.data(),count);
  auto trim=[](std::string_view v){while(!v.empty()&&(v.front()==' '||v.front()=='\t'||v.front()=='\r'))v.remove_prefix(1);while(!v.empty()&&(v.back()==' '||v.back()=='\t'||v.back()=='\r'))v.remove_suffix(1);return v;};
  while(!text.empty()) {
    auto end=text.find('\n');auto line=trim(text.substr(0,end));
    auto equal=line.find('=');
    if(equal!=std::string_view::npos&&trim(line.substr(0,equal))=="socket") {
      std::string path(trim(line.substr(equal+1)));
      return valid_path(path)?path:std::string{};
    }
    if(end==std::string_view::npos)break;
    text.remove_prefix(end+1);
  }
  return {};
}
SpacePacket decode(const std::array<unsigned char,32>&bytes) {
  static_assert(sizeof(int32_t)==4);
  std::array<int32_t,8> words{};std::memcpy(words.data(),bytes.data(),bytes.size());
  SpacePacket packet;
  switch(words[0]) {
  case 0:
    packet.kind=SpacePacket::Kind::Motion;
    for(size_t i=0;i<6;++i)packet.axes[i]=words[i+1];
    std::memcpy(&packet.period,&words[7],4);break;
  case 1:case 2:
    if(words[1]<0||words[1]>=128)break;
    packet.kind=words[0]==1?SpacePacket::Kind::Press:SpacePacket::Kind::Release;
    packet.button=uint16_t(words[1]);break;
  case 3:packet.kind=SpacePacket::Kind::Device;break;
  case 4:packet.kind=SpacePacket::Kind::Config;break;
  case 5:case 6:packet.kind=SpacePacket::Kind::Ignored;break;
  default:break;
  }
  return packet;
}
} // namespace
std::vector<std::string> spacenav_socket_paths() {
  // An explicit standard override is exclusive; a typo must not connect this
  // application to a different daemon. No process-global daemon settings writes.
  if(const char *path=std::getenv("SPNAV_SOCKET");path&&*path) {
    if(strnlen(path,sizeof(sockaddr_un{}.sun_path))>=sizeof(sockaddr_un{}.sun_path))
      return {}; // Keep the ordinary application usable with bad daemon config.
    return {std::string(path)};
  }
  std::vector<std::string> result;
  if(auto path=config_socket();!path.empty())result.push_back(std::move(path));
  if(const char *runtime=std::getenv("XDG_RUNTIME_DIR");runtime&&*runtime) {
    auto length=strnlen(runtime,sizeof(sockaddr_un{}.sun_path));
    auto path=length<sizeof(sockaddr_un{}.sun_path)?std::string(runtime)+"/spnav.sock":std::string{};
    if(valid_path(path))result.push_back(std::move(path));
  }
  result.emplace_back("/var/run/spnav.sock");return result;
}
SpaceUnix::SpaceUnix(std::vector<std::string> paths):paths_(std::move(paths)) {
  if(paths_.size()>8)
    throw std::invalid_argument("Invalid 3D mouse socket list");
  for(const auto &p:paths_)if(!valid_path(p))
    throw std::invalid_argument("3D mouse socket path is empty or exceeds the native Unix path bound");
}
SpaceUnix::~SpaceUnix(){if(fd_>=0)::close(fd_);}
void SpaceUnix::close(double now) noexcept {
  if(fd_>=0){::close(fd_);++epoch_;}
  fd_=-1;used_=0;connecting_=tainted_=false;retry_at_=now+.75;
}
void SpaceUnix::enable(bool enabled,double now) {
  valid_time(now);if(enabled_==enabled)return;
  enabled_=enabled;close(now);if(enabled)retry_at_=now;
}
void SpaceUnix::reconnect(double now){valid_time(now);close(now);retry_at_=now;}
void SpaceUnix::taint_partial() noexcept {if(used_)tainted_=true;}
void SpaceUnix::connect_ready(double now) {
  if(fd_<0) {
    if(!enabled_||paths_.empty()||now<retry_at_)return;
    for(size_t attempt=0;attempt<paths_.size();++attempt) {
      path_index_=next_path_;next_path_=(next_path_+1)%paths_.size();
      auto s=socket(AF_UNIX,SOCK_STREAM|SOCK_NONBLOCK|SOCK_CLOEXEC,0);
      if(s<0){retry_at_=now+.75;return;}
      sockaddr_un address{};address.sun_family=AF_UNIX;
      std::memcpy(address.sun_path,paths_[path_index_].c_str(),paths_[path_index_].size()+1);
      int result=::connect(s,reinterpret_cast<sockaddr*>(&address),
                          socklen_t(offsetof(sockaddr_un,sun_path)+paths_[path_index_].size()+1));
      if(result==0 || (result<0&&errno==EINPROGRESS)) {
        fd_=s;connecting_=result!=0;connect_until_=now+.5;used_=0;tainted_=false;
        ++epoch_;break;
      }
      // AF_UNIX backlog exhaustion returns EAGAIN, not a pending connection.
      ::close(s);
    }
    if(fd_<0){retry_at_=now+.75;return;}
  }
  if(connecting_) {
    pollfd descriptor{fd_,POLLOUT,0};
    int ready=poll(&descriptor,1,0);
    if(ready<0&&errno==EINTR)return;
    if(ready<0){close(now);return;}
    if(!ready){if(now>=connect_until_)close(now);return;}
    int error=0;socklen_t length=sizeof(error);
    if(getsockopt(fd_,SOL_SOCKET,SO_ERROR,&error,&length)||error) {close(now);return;}
    connecting_=false;
  }
}
SpacePump SpaceUnix::pump_erased(double now,void(*sink)(const void*,const SpacePacket&),const void*context) {
  valid_time(now);connect_ready(now);SpacePump result;
  if(connected()) {
    // At most 8KiB of complete events per pulse, without a growable input queue.
    size_t calls=0;
    while(result.packets<256 && calls++<512) {
      auto n=recv(fd_,partial_.data()+used_,partial_.size()-used_,MSG_DONTWAIT);
      if(n>0) {
        used_+=size_t(n);
        if(used_!=partial_.size())continue;
        used_=0;++result.packets;
        if(tainted_)tainted_=false;
        else if(sink)sink(context,decode(partial_));
      } else if(n<0&&errno==EINTR)continue;
      else if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK)){result.drained=true;break;}
      else {close(now);break;}
    }
    if(result.packets==256 || (calls>=512&&!result.drained&&connected()))
      result.backlog=true;
  }
  result.connected=connected();result.epoch=epoch_;return result;
}
} // namespace dome::desktop
