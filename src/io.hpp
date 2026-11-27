// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <algorithm>
#include <filesystem>
#include <fcntl.h>
#include <functional>
#include <span>
#include <stdexcept>
#include <unistd.h>
#include <vector>
namespace dome {
struct AtomicWriteOptions {
  std::function<bool()> cancelled;
  // Called once after the temporary file is durable and before replacement.
  // A caller can atomically claim this commit boundary against cancellation.
  std::function<void()> before_commit;
};
namespace detail {
template<class Produce>
void atomic_write_generated(const std::filesystem::path &path,
                            const AtomicWriteOptions &options,Produce produce) {
  auto check=[&]{if(options.cancelled && options.cancelled())
    throw std::runtime_error("Output cancelled");};
  check();
  // Paths and the directory handle must be ready before a caller claims the
  // commit. In particular, parent_path() may allocate after a successful rename.
  auto parent = path.parent_path();
  if (parent.empty()) parent = ".";
  struct Directory {
    int value = -1;
    ~Directory() { if (value >= 0) close(value); }
  } directory;
  auto name = path.string() + ".tmp-XXXXXX";
  std::vector<char> temporary(name.begin(), name.end());
  temporary.push_back(0);
  int fd = mkstemp(temporary.data());
  if (fd < 0)
    throw std::runtime_error("Cannot create output file");
  try {
    const std::filesystem::path temporary_path(temporary.data());
    const auto write_bytes=[&](std::span<const uint8_t> bytes) {
      while(!bytes.empty()) {
        check();
        const auto n=write(fd,bytes.data(),std::min(bytes.size(),size_t(1024*1024)));
        if(n<0&&errno==EINTR)continue;
        if(n<=0)throw std::runtime_error("Cannot write output file");
        bytes=bytes.subspan(size_t(n));
      }
    };
    produce(write_bytes);
    check();
    if (fsync(fd))
      throw std::runtime_error("Cannot sync output file");
    if (close(fd)) {
      fd = -1;
      throw std::runtime_error("Cannot close output file");
    }
    fd = -1;
    directory.value = open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory.value < 0)
      throw std::runtime_error("Cannot open output directory for sync");
    check();
    if(options.before_commit)options.before_commit();
    // Replacement is the commit point. Do not report cancellation after it.
    std::filesystem::rename(temporary_path, path);
    int synced = fsync(directory.value);
    int closed = close(directory.value);
    directory.value = -1;
    if (synced || closed)
      throw std::runtime_error("Cannot sync output directory");
  } catch (...) {
    if (fd >= 0)
      close(fd);
    unlink(temporary.data());
    throw;
  }
}
} // namespace detail
inline void atomic_write(const std::filesystem::path &path,
                         std::span<const uint8_t> bytes,
                         const AtomicWriteOptions &options) {
  detail::atomic_write_generated(path,options,[&](const auto &write_bytes){write_bytes(bytes);});
}
inline void atomic_write(const std::filesystem::path &path,
                         std::span<const uint8_t> bytes) {
  atomic_write(path,bytes,AtomicWriteOptions{});
}
} // namespace dome
