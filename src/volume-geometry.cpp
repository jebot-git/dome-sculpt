// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/volume.hpp"
namespace dome {
namespace {
int chunk(int n) { return n / 16 - (n % 16 < 0); }
Int3 chunk(Int3 p) { return {chunk(p.x), chunk(p.y), chunk(p.z)}; }
template <class A, class B> bool same_owner(const A &a, const B &b) {
  return !a.owner_before(b) && !b.owner_before(a);
}
} // namespace
struct Volume::GeometryHistory {
  struct Event {
    std::weak_ptr<const VoxelStorage> parent, source;
    GeometryChange change;
  };
  std::vector<std::shared_ptr<const Event>> events;
};
void Volume::geometry_include(GeometryChange &change, Int3 p) {
  geometry_include(change, GeometryRegion{p, p});
}
void Volume::geometry_include(GeometryChange &change, GeometryRegion region) {
  if (change.kind != GeometryChange::Kind::Local)
    return;
  auto key = chunk(region.minimum);
  if (chunk(region.maximum) != key) {
    change.kind = GeometryChange::Kind::Rebuild;
    change.count = 0;
    return;
  }
  for (size_t i = 0; i < change.count; ++i) {
    auto &stored = change.samples[i];
    if (chunk(stored.minimum) != key)
      continue;
    stored.minimum = {std::min(stored.minimum.x, region.minimum.x),
                      std::min(stored.minimum.y, region.minimum.y),
                      std::min(stored.minimum.z, region.minimum.z)};
    stored.maximum = {std::max(stored.maximum.x, region.maximum.x),
                      std::max(stored.maximum.y, region.maximum.y),
                      std::max(stored.maximum.z, region.maximum.z)};
    return;
  }
  if (change.count == change.samples.size()) {
    change.kind = GeometryChange::Kind::Rebuild;
    change.count = 0;
    return;
  }
  change.samples[change.count++] = region;
}
Volume::GeometryHistoryPtr
Volume::prepare_geometry_history(const CowMap<Int3, float, IntHash> &next,
                                 const GeometryChange &change) const {
  if (next.owner() == geometry_owner())
    return geometry_history_;
  auto event = std::make_shared<GeometryHistory::Event>();
  event->parent = geometry_owner();
  event->source = next.owner();
  event->change = change;
  if (event->change.kind == GeometryChange::Kind::Unchanged ||
      (event->change.kind == GeometryChange::Kind::Local &&
       !event->change.count))
    event->change.kind = GeometryChange::Kind::Rebuild;
  event->change.events = 1;
  auto next_history = std::make_shared<GeometryHistory>();
  if (geometry_history_)
    next_history->events = geometry_history_->events;
  constexpr size_t maximum_events = 64;
  if (next_history->events.size() == maximum_events)
    next_history->events.erase(next_history->events.begin());
  next_history->events.push_back(std::move(event));
  return next_history;
}
Volume::GeometryChange
Volume::geometry_change(const GeometryOwner &base) const {
  GeometryChange result;
  auto current = geometry_owner();
  if (!base)
    return result;
  if (base == current && same_owner(base, current)) {
    result.kind = GeometryChange::Kind::Unchanged;
    return result;
  }
  if (!geometry_history_)
    return result;
  std::weak_ptr<const VoxelStorage> expected = current;
  result.kind = GeometryChange::Kind::Local;
  for (auto it = geometry_history_->events.rbegin();
       it != geometry_history_->events.rend(); ++it) {
    auto &event = **it;
    if (!same_owner(event.source, expected) ||
        event.change.kind != GeometryChange::Kind::Local)
      return GeometryChange{};
    ++result.events;
    for (auto region : event.change.regions()) {
      geometry_include(result, region);
      if (result.kind == GeometryChange::Kind::Rebuild)
        return GeometryChange{};
    }
    if (same_owner(event.parent, base)) {
      // A held base keeps the actual old map alive; reject forged alias
      // pointers sharing the control block but referring to an unrelated map
      // object.
      auto actual = event.parent.lock();
      return actual && actual.get() == base.get() ? result : GeometryChange{};
    }
    expected = event.parent;
  }
  return GeometryChange{};
}
} // namespace dome
