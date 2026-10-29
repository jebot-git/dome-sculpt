// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <filesystem>
#include <span>
#include <stdexcept>
#include <unistd.h>
#include <vector>
namespace dome {
inline void atomic_write(const std::filesystem::path &path,
                         std::span<const uint8_t> bytes) {
  auto name = path.string() + ".tmp-XXXXXX";
  std::vector<char> temporary(name.begin(), name.end());
  temporary.push_back(0);
  int fd = mkstemp(temporary.data());
  if (fd < 0)
    throw std::runtime_error("Cannot create output file");
  try {
    size_t done = 0;
    while (done < bytes.size()) {
      auto n = write(fd, bytes.data() + done, bytes.size() - done);
      if (n < 0 && errno == EINTR)
        continue;
      if (n <= 0)
        throw std::runtime_error("Cannot write output file");
      done += size_t(n);
    }
    if (fsync(fd))
      throw std::runtime_error("Cannot sync output file");
    if (close(fd)) {
      fd = -1;
      throw std::runtime_error("Cannot close output file");
    }
    fd = -1;
    std::filesystem::rename(temporary.data(), path);
  } catch (...) {
    if (fd >= 0)
      close(fd);
    unlink(temporary.data());
    throw;
  }
}
} // namespace dome
