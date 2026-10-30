// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/sculpt-storage.hpp"
#include <atomic>
namespace dome {
namespace {
std::atomic<size_t> budget{512 * 1024 * 1024}, live{0}, peak{0}, allocations{0},
    allocated{0};
std::atomic<size_t> roots{0}, shards{0}, pages{0}, copied_bytes{0},
    live_pages{0}, dense_pages{0};
} // namespace
SculptStorageStats sculpt_storage_stats() {
  return {live.load(),       peak.load(),         allocations.load(),
          allocated.load(),  roots.load(),        shards.load(),
          pages.load(),      copied_bytes.load(), live_pages.load(),
          dense_pages.load()};
}
void set_sculpt_storage_budget(size_t n) {
  if (n < live.load())
    throw std::length_error("Sculpt storage budget is below retained owners");
  budget = n;
}
size_t sculpt_storage_budget() { return budget.load(); }
namespace storage_detail {
void reserve(size_t bytes) {
  auto previous = live.load();
  for (;;) {
    auto maximum = budget.load();
    if (previous > maximum || bytes > maximum - previous)
      throw std::length_error("Paged sculpt storage memory budget exceeded");
    if (live.compare_exchange_weak(previous, previous + bytes))
      break;
  }
  auto p = peak.load();
  while (p < previous + bytes &&
         !peak.compare_exchange_weak(p, previous + bytes)) {
  }
  ++allocations;
  allocated += bytes;
}
void release(size_t bytes) noexcept { live -= bytes; }
void copied(unsigned kind, size_t bytes) noexcept {
  if (kind == 0)
    ++roots;
  else if (kind == 1)
    ++shards;
  else
    ++pages;
  copied_bytes += bytes;
}
void page_live(int delta) noexcept {
  if (delta > 0)
    ++live_pages;
  else
    --live_pages;
}
void page_dense(int delta) noexcept {
  if (delta > 0)
    ++dense_pages;
  else
    --dense_pages;
}
} // namespace storage_detail
} // namespace dome
