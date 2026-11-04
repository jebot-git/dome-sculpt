// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "heat-binding.hpp"
#include "volume-topology.hpp"
#include <bit>
#include <map>
#include <numeric>
namespace dome::detail {
class HeatSeamGraph {
  uint64_t object_ = 0;
  std::shared_ptr<const PaintedMesh> paint_;
  std::shared_ptr<const Mesh> mesh_;
  std::shared_ptr<const DeformationState> deformation_;
  std::shared_ptr<const CreatorAppearance> appearance_;
  std::vector<uint32_t> groups_;
  size_t joined_ = 0, rejected_ = 0, ambiguous_ = 0, work_ = 0, bytes_ = 0;
  template <class T, class U>
  static bool same(const std::shared_ptr<T> &a, const std::shared_ptr<U> &b) {
    return a.get() == b.get() && !a.owner_before(b) && !b.owner_before(a);
  }
  friend HeatSeamGraph capture_heat_seams(const SculptObject &,
                                          const HeatBindingOptions &,
                                          const std::function<bool()> &);

public:
  // Worker-local certificate, never a persistent or cross-call authority cache.
  bool matches(const SculptObject &o) const {
    return o.id == object_ && same(paint_, o.painted) && o.painted &&
           same(mesh_, o.painted->mesh_owner()) &&
           same(deformation_, o.deformation) && same(appearance_, o.appearance);
  }
  std::span<const uint32_t> groups(const SculptObject &o) const {
    if (!matches(o))
      throw std::invalid_argument("Heat seam source owner or effective current "
                                  "rig/bind authority changed");
    return groups_;
  }
  size_t joined_edges() const { return joined_; }
  size_t rejected_edges() const { return rejected_; }
  size_t ambiguous_edges() const { return ambiguous_; }
  size_t work() const { return work_; }
  size_t planned_bytes() const { return bytes_; }
};
inline HeatSeamGraph
capture_heat_seams(const SculptObject &o, const HeatBindingOptions &options,
                   const std::function<bool()> &cancelled = {}) {
  if (!o.id || !o.painted || !o.deformation || !o.deformation->skeleton ||
      !o.deformation->skin)
    throw std::invalid_argument(
        "Heat seam continuity needs one captured current mesh/skin/bind owner");
  if (!options.maximum_work || options.maximum_work > 500'000'000 ||
      options.maximum_bytes < 4096 ||
      options.maximum_bytes > 512u * 1024u * 1024u)
    throw std::invalid_argument("Invalid heat seam capture resource bounds");
  HeatSeamGraph result;
  result.object_ = o.id;
  result.paint_ = o.painted;
  result.mesh_ = o.painted->mesh_owner();
  result.deformation_ = o.deformation;
  result.appearance_ = o.appearance;
  const auto &m = *result.mesh_;
  const auto &d = *result.deformation_;
  const size_t n = m.positions.size(), nt = m.indices.size() / 3;
  if (!n || n > 2'000'000 || !nt || nt > 2'000'000 ||
      d.skeleton->bones.empty() || d.skeleton->bones.size() > 256 ||
      (d.morphs && d.morphs->targets.size() > 256))
    throw std::length_error("Heat seam geometry/rig limits exceeded");
  auto step = [&](size_t count = 1) {
    if (count > options.maximum_work - result.work_)
      throw std::length_error("Heat seam capture work budget exceeded");
    result.work_ += count;
    if ((result.work_ & 1023) < count && cancelled && cancelled())
      throw std::runtime_error("Heat seam capture cancelled");
  };
  auto bytes = [&](size_t count, size_t stride) {
    if (count > (options.maximum_bytes - result.bytes_) / stride)
      throw std::length_error(
          "Heat seam capture memory budget exceeded before construction");
    result.bytes_ += count * stride;
  };
  bytes(n, 32);
  bytes(nt, 640);
  bytes(d.skeleton->bones.size() * d.skeleton->bones.size(), 64);
  bytes(1, 8192);
  step(n + m.indices.size() +
       d.skeleton->bones.size() * d.skeleton->bones.size());
  if (d.skin->vertices.size() != n)
    throw std::invalid_argument("Heat seam skin owner dimensions differ");
  for (const auto &row : d.skin->vertices) {
    step();
    if (row.size() > 32)
      throw std::invalid_argument(
          "Heat seam source skin row exceeds its bound");
    step(row.size() * (4 * std::bit_width(row.size() + 1) + 1));
  }
  if (d.morphs) {
    bytes(d.morphs->targets.size(), 256);
    step(n * d.morphs->targets.size() * 4 + n + m.indices.size());
  }
  if (cancelled && cancelled())
    throw std::runtime_error("Heat seam capture cancelled");
  m.validate();
  d.skeleton->validate();
  d.skin->validate(n, d.skeleton->bones.size());
  if (d.morphs)
    d.morphs->validate(m);
  step(m.indices.size() * (8 * std::bit_width(m.indices.size() + 1) + 1));
  struct Edge {
    uint32_t a = 0, b = 0, triangle = 0, count = 0;
  };
  std::map<std::array<uint32_t, 2>, Edge> raw;
  for (uint32_t t = 0; t < nt; ++t)
    for (int k = 0; k < 3; ++k) {
      step();
      auto a = m.indices[size_t(t) * 3 + k],
           b = m.indices[size_t(t) * 3 + (k + 1) % 3];
      auto &edge = raw[{std::min(a, b), std::max(a, b)}];
      if (!edge.count)
        edge = {a, b, t, 0};
      ++edge.count;
    }
  using P = std::array<float, 3>;
  auto position = [&](uint32_t i) {
    auto p = m.positions[i];
    return P{p.x, p.y, p.z};
  };
  struct Pair {
    Edge first{}, second{};
    uint32_t count = 0;
  };
  std::map<std::array<P, 2>, Pair> boundaries;
  for (auto [key, edge] : raw) {
    step();
    if (edge.count != 1)
      continue;
    auto a = position(edge.a), b = position(edge.b);
    auto &pair = boundaries[{std::min(a, b), std::max(a, b)}];
    if (pair.count == 0)
      pair.first = edge;
    else if (pair.count == 1)
      pair.second = edge;
    ++pair.count;
  }
  result.groups_.resize(n);
  std::iota(result.groups_.begin(), result.groups_.end(), 0);
  auto root = [&](uint32_t i) {
    while (i != result.groups_[i]) {
      step();
      result.groups_[i] = result.groups_[result.groups_[i]];
      i = result.groups_[i];
    }
    return i;
  };
  auto equivalent = [&](uint32_t a, uint32_t b) {
    step();
    if (position(a) != position(b))
      return false;
    const auto &x = d.skin->vertices[a];
    const auto &y = d.skin->vertices[b];
    step(x.size() * y.size());
    if (x.size() != y.size())
      return false;
    for (auto i : x)
      if (std::none_of(y.begin(), y.end(), [&](Influence j) { return i == j; }))
        return false;
    if (d.morphs)
      for (const auto &target : d.morphs->targets) {
        step();
        auto p = target.delta[a], q = target.delta[b];
        if (p.x != q.x || p.y != q.y || p.z != q.z)
          return false;
      }
    return true;
  };
  auto triangle = [&](uint32_t t) {
    volume_topology::Triangle points;
    for (int k = 0; k < 3; ++k)
      points[k] = position(m.indices[size_t(t) * 3 + k]);
    return points;
  };
  using Visit = decltype(step);
  struct Work {
    Visit &visit;
    void step(size_t n = 1) { visit(n); }
  } work{step};
  for (auto [key, pair] : boundaries) {
    step();
    if (pair.count != 2) {
      if (pair.count > 2)
        ++result.ambiguous_;
      continue;
    }
    const auto &a = pair.first, &b = pair.second;
    if (position(a.a) != position(b.b) || position(a.b) != position(b.a) ||
        !equivalent(a.a, b.b) || !equivalent(a.b, b.a) ||
        volume_topology::unexpected_contact(triangle(a.triangle),
                                            triangle(b.triangle), work)) {
      ++result.rejected_;
      continue;
    }
    result.groups_[root(a.a)] = root(b.b);
    result.groups_[root(a.b)] = root(b.a);
    ++result.joined_;
  }
  for (uint32_t i = 0; i < n; ++i) {
    step();
    result.groups_[i] = root(i);
  }
  return result;
}
} // namespace dome::detail
