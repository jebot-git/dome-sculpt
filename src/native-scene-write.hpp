// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <span>

namespace dome {
class Scene;
namespace detail {
inline constexpr size_t maximum_native_scene_bytes=256*1024*1024;
inline constexpr uint64_t scene_checksum_seed=14695981039346656037ull;
inline uint64_t extend_scene_checksum(uint64_t hash,std::span<const uint8_t> bytes) {
  for(const uint8_t byte:bytes)hash=(hash^byte)*1099511628211ull;
  return hash;
}
struct NativeSceneWriteResult {
  size_t bytes=0;
  uint64_t checksum=scene_checksum_seed;
};
NativeSceneWriteResult write_native_scene(const Scene &,const std::filesystem::path &,
                                         size_t limit,bool checksum);
}
}
