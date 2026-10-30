// SPDX-License-Identifier: GPL-3.0-or-later
#include "ray-index.hpp"
#include <atomic>
#include <bit>
#include <mutex>
#include <set>
namespace dome::sculpt_detail {
namespace {
constexpr size_t memory_limit = 64 * 1024 * 1024,
                 source_limit = 128 * 1024 * 1024;
constexpr size_t maximum_pages = 262144, maximum_nodes = 262144,
                 maximum_intervals = 32768;
std::atomic_size_t live{0}, peak{0}, allocations{0}, cold{0}, local{0},
    pages{0}, visits{0}, cells{0};
std::atomic_size_t fallback_queries{0}, refusals{0}, node_charge{0};
struct Node;
thread_local bool measuring_node = false;
thread_local bool query_active = false;
struct IndexLimit : std::length_error {
  using length_error::length_error;
};
struct MemoryLimit : std::length_error {
  MemoryLimit() : length_error("Sculpt ray index memory budget exceeded") {}
};
template <class T> struct Allocator {
  using value_type = T;
  Allocator() = default;
  template <class U> Allocator(const Allocator<U> &) {}
  T *allocate(size_t n) {
    if (n > (SIZE_MAX - 64) / sizeof(T))
      throw MemoryLimit();
    size_t bytes = n * sizeof(T) + 64, previous = live.load();
    if (measuring_node)
      node_charge = bytes;
    for (;;) {
      if (previous > memory_limit || bytes > memory_limit - previous)
        throw MemoryLimit();
      if (live.compare_exchange_weak(previous, previous + bytes))
        break;
    }
    auto old = peak.load();
    while (old < previous + bytes &&
           !peak.compare_exchange_weak(old, previous + bytes)) {
    }
    ++allocations;
    try {
      return std::allocator<T>{}.allocate(n);
    } catch (...) {
      live -= bytes;
      throw;
    }
  }
  void deallocate(T *p, size_t n) {
    std::allocator<T>{}.deallocate(p, n);
    live -= n * sizeof(T) + 64;
  }
  template <class U> bool operator==(const Allocator<U> &) const {
    return true;
  }
};
template <class T, class... A> auto make(A &&...a) {
  bool old = measuring_node;
  measuring_node = std::is_same_v<T, Node>;
  try {
    auto value = std::allocate_shared<T>(Allocator<T>{}, std::forward<A>(a)...);
    measuring_node = old;
    return value;
  } catch (...) {
    measuring_node = old;
    throw;
  }
}
void checkpoint(const RayCancel &cancel) {
  if (cancel && cancel())
    throw std::runtime_error("Sculpt ray cancelled");
}
struct Node;
using Ptr = std::shared_ptr<Node>;
struct Node {
  Ptr child[2];
  Int3 key{}, lo{}, hi{};
  float minimum = INFINITY;
  unsigned bit = 96;
  Node(Int3 k, float m) : key(k), lo(k), hi(k), minimum(m) {}
  Node(unsigned b, Ptr a, Ptr c) : child{std::move(a), std::move(c)}, bit(b) {
    refresh();
  }
  void refresh() {
    lo = {std::min(child[0]->lo.x, child[1]->lo.x),
          std::min(child[0]->lo.y, child[1]->lo.y),
          std::min(child[0]->lo.z, child[1]->lo.z)};
    hi = {std::max(child[0]->hi.x, child[1]->hi.x),
          std::max(child[0]->hi.y, child[1]->hi.y),
          std::max(child[0]->hi.z, child[1]->hi.z)};
    minimum = std::min(child[0]->minimum, child[1]->minimum);
  }
};
unsigned direction(Int3 key, unsigned bit) {
  uint32_t v = uint32_t(bit % 3 == 0   ? key.x
                        : bit % 3 == 1 ? key.y
                                       : key.z) ^
               0x80000000u;
  return (v >> (31 - bit / 3)) & 1;
}
unsigned difference(Int3 a, Int3 b) {
  unsigned result = 96;
  uint32_t x[] = {uint32_t(a.x) ^ uint32_t(b.x), uint32_t(a.y) ^ uint32_t(b.y),
                  uint32_t(a.z) ^ uint32_t(b.z)};
  for (unsigned k = 0; k < 3; ++k)
    if (x[k])
      result = std::min(result, unsigned(std::countl_zero(x[k])) * 3 + k);
  return result;
}
void detach(Ptr &p) {
  if (!p.unique())
    p = make<Node>(*p);
}
void insert_at(Ptr &p, Int3 key, float minimum, unsigned split) {
  if (p->bit >= split) {
    auto leaf = make<Node>(key, minimum);
    if (direction(key, split))
      p = make<Node>(split, p, std::move(leaf));
    else
      p = make<Node>(split, std::move(leaf), p);
    return;
  }
  detach(p);
  insert_at(p->child[direction(key, p->bit)], key, minimum, split);
  p->refresh();
}
void update(Ptr &p, Int3 key, std::optional<float> minimum) {
  if (!p) {
    if (minimum)
      p = make<Node>(key, *minimum);
    return;
  }
  auto found = p.get();
  while (found->bit < 96)
    found = found->child[direction(key, found->bit)].get();
  auto split = difference(found->key, key);
  if (split < 96) {
    if (minimum)
      insert_at(p, key, *minimum, split);
    return;
  }
  auto change = [&](auto &&self, Ptr &n) -> void {
    detach(n);
    if (n->bit == 96) {
      if (minimum)
        n->minimum = *minimum;
      else
        n.reset();
      return;
    }
    auto side = direction(key, n->bit);
    self(self, n->child[side]);
    if (!n->child[side]) {
      auto other = n->child[1 - side];
      n = std::move(other);
    } else
      n->refresh();
  };
  change(change, p);
}
struct Entry {
  Volume::GeometryOwner owner;
  float voxel, band;
  Ptr tree;
  size_t source_bytes;
};
struct Refusal {
  std::weak_ptr<const Volume::VoxelStorage> owner;
  float voxel, band;
};
struct Cache {
  std::mutex mutex;
  std::vector<std::shared_ptr<Entry>> entries;
  std::vector<Refusal> failures;
  size_t sources = 0;
};
Cache &cache() {
  static Cache c;
  return c;
}
bool same(const Volume::GeometryOwner &a, const Volume::GeometryOwner &b) {
  return a == b && !a.owner_before(b) && !b.owner_before(a);
}
void remember_fallback(const Volume &field) {
  auto &c = cache();
  std::lock_guard lock(c.mutex);
  if (c.failures.size() >= 256)
    c.failures.erase(c.failures.begin());
  c.failures.push_back(
      {field.geometry_owner(), field.voxel_size(), field.band()});
  ++refusals;
}
std::shared_ptr<Entry> index(const Volume &field, const RayCancel &cancel) {
  auto owner = field.geometry_owner();
  std::shared_ptr<Entry> previous;
  Volume::GeometryChange change;
  std::array<std::shared_ptr<Entry>, 256> candidates{};
  size_t candidate_count = 0;
  const size_t source_bytes = owner->ray_retained_bytes(),
               page_count = owner->page_count(), charge = node_charge.load();
  // O(64) proofs survive small edits without rescanning a large field. A binary
  // Patricia tree needs exactly2N-1 nodes; measured allocator charge is exact.
  if (source_bytes > source_limit || page_count > maximum_pages ||
      (charge && page_count > (memory_limit / charge + 1) / 2)) {
    ++refusals;
    return {};
  }
  {
    auto &c = cache();
    std::lock_guard lock(c.mutex);
    for (auto it = c.failures.begin(); it != c.failures.end();) {
      auto p = it->owner.lock();
      if (!p) {
        it = c.failures.erase(it);
        continue;
      }
      if (it->voxel == field.voxel_size() && it->band == field.band() &&
          same(p, owner))
        return {};
      ++it;
    }
    for (size_t i = 0; i < c.entries.size(); ++i) {
      auto e = c.entries[i];
      if (e->voxel == field.voxel_size() && e->band == field.band() &&
          same(e->owner, owner)) {
        c.entries.erase(c.entries.begin() + i);
        c.entries.push_back(e);
        return e;
      }
    }
    for (auto it = c.entries.rbegin(); it != c.entries.rend(); ++it)
      candidates[candidate_count++] = *it;
  }
  for (size_t i = 0; i < candidate_count; ++i) {
    checkpoint(cancel);
    auto &e = *candidates[i];
    if (e.voxel != field.voxel_size() || e.band != field.band())
      continue;
    auto candidate = field.geometry_change(e.owner);
    if (candidate.kind == Volume::GeometryChange::Kind::Local) {
      previous = candidates[i];
      change = candidate;
      break;
    }
  }
  for (auto &candidate : candidates)
    candidate.reset();
  auto build = [&] {
    auto e = make<Entry>();
    e->owner = owner;
    e->voxel = field.voxel_size();
    e->band = field.band();
    e->source_bytes = source_bytes;
    if (previous) {
      e->tree = previous->tree;
      std::set<Int3> dirty;
      for (auto r : change.regions()) {
        auto chunk = [](int v) { return v / 16 - (v % 16 < 0); };
        dirty.insert(
            {chunk(r.minimum.x), chunk(r.minimum.y), chunk(r.minimum.z)});
      }
      for (auto key : dirty) {
        checkpoint(cancel);
        ++pages;
        update(e->tree, key, owner->page_minimum(key));
      }
    } else {
      size_t count = 0;
      owner->visit_pages([&](Int3 key, float minimum) {
        checkpoint(cancel);
        if (++count > maximum_pages)
          throw IndexLimit("Sculpt ray index page budget exceeded");
        ++pages;
        update(e->tree, key, minimum);
      });
    }
    checkpoint(cancel);
    return e;
  };
  std::shared_ptr<Entry> e;
  try {
    try {
      e = build();
    } catch (const MemoryLimit &) {
      clear_ray_index_cache();
      e = build();
    }
  } catch (const MemoryLimit &) {
    remember_fallback(field);
    return {};
  } catch (const IndexLimit &) {
    remember_fallback(field);
    return {};
  }
  {
    auto &c = cache();
    std::lock_guard lock(c.mutex);
    if (previous)
      for (auto it = c.entries.begin(); it != c.entries.end(); ++it)
        if (*it == previous) {
          c.sources -= (*it)->source_bytes;
          c.entries.erase(it);
          break;
        }
    while (!c.entries.empty() && (c.entries.size() >= 256 ||
                                  source_bytes > source_limit - c.sources)) {
      c.sources -= c.entries.front()->source_bytes;
      c.entries.erase(c.entries.begin());
    }
    c.entries.push_back(e);
    c.sources += source_bytes;
  }
  if (previous)
    ++local;
  else
    ++cold;
  return e;
}
bool bounds(const Node &n, const double start[3], const double ray[3],
            double voxel, double maximum, RayInterval &out) {
  out = {0, maximum};
  int lower[] = {n.lo.x, n.lo.y, n.lo.z}, upper[] = {n.hi.x, n.hi.y, n.hi.z};
  for (int k = 0; k < 3; ++k) {
    double lo = std::nextafter((double(lower[k]) * 16 - 1) * voxel, -INFINITY),
           hi = std::nextafter((double(upper[k]) * 16 + 16) * voxel, INFINITY);
    if (ray[k] == 0) {
      if (start[k] < lo || start[k] > hi)
        return false;
      continue;
    }
    double a = (lo - start[k]) / ray[k], b = (hi - start[k]) / ray[k];
    if (a > b)
      std::swap(a, b);
    out.begin = std::max(out.begin, a);
    out.end = std::min(out.end, b);
    if (out.begin > out.end)
      return false;
  }
  return true;
}
thread_local std::vector<RayInterval, Allocator<RayInterval>> workspace;
thread_local std::array<RayInterval, 1> fallback_interval;
std::span<const RayInterval> fallback(double maximum) {
  ++fallback_queries;
  fallback_interval[0] = {0, maximum};
  return fallback_interval;
}
} // namespace
RayQueryScope::RayQueryScope() {
  if (query_active)
    throw std::logic_error("Recursive sculpt ray query is not supported");
  query_active = true;
}
RayQueryScope::~RayQueryScope() { query_active = false; }
void clear_ray_index_cache() {
  auto &c = cache();
  std::lock_guard lock(c.mutex);
  c.entries.clear();
  c.failures.clear();
  c.sources = 0;
}
RayIndexStats ray_index_stats() {
  auto &c = cache();
  std::lock_guard lock(c.mutex);
  return {cold.load(),
          local.load(),
          pages.load(),
          visits.load(),
          cells.load(),
          live.load(),
          peak.load(),
          allocations.load(),
          c.entries.size(),
          c.sources,
          fallback_queries.load(),
          refusals.load()};
}
void ray_cell_visited() { ++cells; }
std::span<const RayInterval>
ray_intervals(const Volume &field, const double origin[3], const double ray[3],
              double maximum, double threshold, const RayCancel &cancel) {
  checkpoint(cancel);
  auto e = index(field, cancel);
  workspace.clear();
  if (!e)
    return fallback(maximum);
  if (!e->tree)
    return workspace;
  try {
    std::array<const Node *, 98> stack{};
    size_t pending = 1, work = 0;
    stack[0] = e->tree.get();
    while (pending) {
      if ((work++ & 255) == 0)
        checkpoint(cancel);
      if (work > maximum_nodes)
        throw IndexLimit("Sculpt ray spatial traversal budget exceeded");
      ++visits;
      auto node = stack[--pending];
      RayInterval interval;
      if (node->minimum > threshold ||
          !bounds(*node, origin, ray, e->voxel, maximum, interval))
        continue;
      if (node->bit < 96) {
        if (pending + 2 > stack.size())
          throw std::length_error("Sculpt ray spatial depth exceeded");
        stack[pending++] = node->child[0].get();
        stack[pending++] = node->child[1].get();
      } else {
        if (workspace.size() >= maximum_intervals)
          throw IndexLimit("Sculpt ray candidate interval budget exceeded");
        interval.begin =
            std::max(0., std::nextafter(interval.begin, -INFINITY));
        interval.end =
            std::min(maximum, std::nextafter(interval.end, INFINITY));
        workspace.push_back(interval);
      }
    }
    std::sort(workspace.begin(), workspace.end(), [](auto a, auto b) {
      return a.begin < b.begin || (a.begin == b.begin && a.end < b.end);
    });
    size_t merged = 0;
    for (auto r : workspace) {
      if (merged && r.begin <= workspace[merged - 1].end)
        workspace[merged - 1].end = std::max(workspace[merged - 1].end, r.end);
      else
        workspace[merged++] = r;
    }
    workspace.resize(merged);
    return workspace;
  } catch (const MemoryLimit &) {
    // Workspace and traversal limits depend on this ray. Do not poison the
    // owner's reusable index or force unrelated short rays onto the old DDA.
    return fallback(maximum);
  } catch (const IndexLimit &) {
    return fallback(maximum);
  }
}
} // namespace dome::sculpt_detail
