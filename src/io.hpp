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
inline void atomic_write(const std::filesystem::path &path,
                         std::span<const uint8_t> bytes,
                         const AtomicWriteOptions &options) {
  auto check=[&]{if(options.cancelled && options.cancelled())
    throw std::runtime_error("Output cancelled");};
  check();
  auto name = path.string() + ".tmp-XXXXXX";
  std::vector<char> temporary(name.begin(), name.end());
  temporary.push_back(0);
  int fd = mkstemp(temporary.data());
  if (fd < 0)
    throw std::runtime_error("Cannot create output file");
  try {
    size_t done = 0;
    while (done < bytes.size()) {
      check();
      auto n = write(fd, bytes.data() + done,
                     std::min(bytes.size() - done, size_t(1024 * 1024)));
      if (n < 0 && errno == EINTR)
        continue;
      if (n <= 0)
        throw std::runtime_error("Cannot write output file");
      done += size_t(n);
    }
    check();
    if (fsync(fd))
      throw std::runtime_error("Cannot sync output file");
    if (close(fd)) {
      fd = -1;
      throw std::runtime_error("Cannot close output file");
    }
    fd = -1;
    check();
    if(options.before_commit)options.before_commit();
    // Replacement is the commit point. Do not report cancellation after it.
    std::filesystem::rename(temporary.data(), path);
    auto parent = path.parent_path();
    if (parent.empty()) parent = ".";
    int directory = open(parent.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (directory < 0)
      throw std::runtime_error("Cannot open output directory for sync");
    int synced = fsync(directory);
    int closed = close(directory);
    if (synced || closed)
      throw std::runtime_error("Cannot sync output directory");
  } catch (...) {
    if (fd >= 0)
      close(fd);
    unlink(temporary.data());
    throw;
  }
}
inline void atomic_write(const std::filesystem::path &path,
                         std::span<const uint8_t> bytes) {
  atomic_write(path,bytes,AtomicWriteOptions{});
}
} // namespace dome
