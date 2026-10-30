// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <cstddef>
#include <limits>
#include <memory>
#include <stdexcept>
namespace dome {
struct SculptStorageStats {
  size_t live_bytes = 0, peak_bytes = 0, allocations = 0, allocated_bytes = 0;
  size_t root_copies = 0, shard_copies = 0, page_copies = 0, copied_bytes = 0;
  size_t live_pages = 0, dense_pages = 0;
};
SculptStorageStats sculpt_storage_stats();
void set_sculpt_storage_budget(size_t bytes);
size_t sculpt_storage_budget();
namespace storage_detail {
void reserve(size_t);
void release(size_t) noexcept;
void copied(unsigned kind, size_t bytes) noexcept;
void page_live(int delta) noexcept;
void page_dense(int delta) noexcept;
template <class T> struct Allocator {
  using value_type = T;
  Allocator() = default;
  template <class U> Allocator(const Allocator<U> &) noexcept {}
  T *allocate(size_t n) {
    if (n > (std::numeric_limits<size_t>::max() - 64) / sizeof(T))
      throw std::bad_array_new_length();
    auto charged = n * sizeof(T) + 64;
    reserve(charged);
    try {
      return std::allocator<T>{}.allocate(n);
    } catch (...) {
      release(charged);
      throw;
    }
  }
  void deallocate(T *p, size_t n) noexcept {
    std::allocator<T>{}.deallocate(p, n);
    release(n * sizeof(T) + 64);
  }
  template <class U> bool operator==(const Allocator<U> &) const noexcept {
    return true;
  }
};
template <class T, class... Args> auto shared(Args &&...args) {
  return std::allocate_shared<T>(Allocator<T>{}, std::forward<Args>(args)...);
}
} // namespace storage_detail
} // namespace dome
