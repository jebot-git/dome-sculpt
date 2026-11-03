// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/mesh-processing.hpp"
#include "heat-binding.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>
#include <stdexcept>
#include <vector>
namespace dome::detail {
// CONSTRUCTION-ONLY diagnostic: axial support is not volumetric ownership.
// No SkinWeights or publication permit is produced. Partial/outside sources
// remain explicit required adapters, never silently omitted eligible controls.
enum class AnatomicalSupport {
  SegmentFull,
  SegmentClipped,
  PointAxial,
  Outside,
  PointAxisMissing,
  Occluded
};
struct AnatomicalProjectionRecord {
  uint32_t bone = 0, component = 0;
  AnatomicalSupport support = AnatomicalSupport::Outside;
  double physical_length = 0, supported_begin = 0, supported_end = 0;
  size_t samples = 0;
};
struct SampledAnatomicalDomain {
  std::vector<std::array<uint64_t, 4>> candidate_anchors;
  std::vector<AnatomicalProjectionRecord> records;
  double mean_edge = 0;
  size_t rays = 0, points = 0, planned_bytes = 0;
  bool all_axially_complete = true;
};
template <class System, class Budget>
SampledAnatomicalDomain diagnose_sampled_anatomical_domain(
    const System &s, const Mesh &m, std::span<const HeatSourceSegment> sources,
    std::span<const std::array<double, 3>> point_axes,
    std::span<const std::array<uint64_t, 4>> component_usage,
    const HeatBindingOptions &o, Budget &budget) {
  using P = std::array<double, 3>;
  auto sub = [](P a, P b) {
    for (int k = 0; k < 3; ++k)
      a[k] -= b[k];
    return a;
  };
  auto dot = [](P a, P b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; };
  auto norm = [&](P a) { return std::sqrt(dot(a, a)); };
  const size_t n = s.positions.size(), corners = m.indices.size();
  budget.stop();
  if (!n || n > 2'000'000 || !corners || corners > 12'000'000 || corners % 3 ||
      sources.empty() || sources.size() > 512 ||
      point_axes.size() != sources.size() || component_usage.size() != n ||
      s.parent.size() != n || s.surface_parent.size() != n ||
      s.starts.size() != n + 1 || s.starts.back() != s.columns.size() ||
      o.visibility != HeatVisibility::EntryFaces ||
      o.occlusion != HeatOcclusion::ConnectedSurface ||
      !(o.maximum_seed_distance > 0) || !std::isfinite(o.maximum_seed_distance))
    throw std::invalid_argument("Invalid anatomical sampling domain");
  size_t pairs = 0;
  std::array<uint64_t, 4> present{};
  for (const auto &source : sources) {
    budget.step(8 + n);
    if (source.bone >= 256)
      throw std::invalid_argument("Anatomical source joint outside mask");
    for (double v : source.head)
      if (!std::isfinite(v))
        throw std::invalid_argument("Nonfinite anatomical head");
    for (double v : source.tail)
      if (!std::isfinite(v))
        throw std::invalid_argument("Nonfinite anatomical tail");
    present[source.bone / 64] |= uint64_t(1) << (source.bone % 64);
    bool represented = false;
    for (size_t root = 0; root < n; ++root)
      if (s.parent[root] == root && (component_usage[root][source.bone / 64] &
                                     (uint64_t(1) << (source.bone % 64)))) {
        ++pairs;
        represented = true;
      }
    if (!represented)
      throw std::invalid_argument(
          "Anatomical source has no declared component");
  }
  for (size_t root = 0; root < n; ++root)
    if (s.parent[root] == root) {
      budget.step(4);
      if (component_usage[root] == std::array<uint64_t, 4>{})
        throw std::invalid_argument("Anatomical component has empty intent");
      for (size_t word = 0; word < 4; ++word)
        if (component_usage[root][word] & ~present[word])
          throw std::invalid_argument("Anatomical intent names missing source");
    }
  for (const auto &axis : point_axes)
    for (double v : axis)
      if (!std::isfinite(v))
        throw std::invalid_argument("Nonfinite anatomical point axis");
  const size_t planned =
      n * 256 + corners * 160 + pairs * 160 + sources.size() * 64 + 8192;
  if (o.maximum_bytes > 512u * 1024u * 1024u || planned > o.maximum_bytes)
    throw std::length_error("Anatomical projection exceeds byte admission");
  SampledAnatomicalDomain out;
  out.planned_bytes = planned;
  out.candidate_anchors.resize(n);
  out.records.reserve(pairs);
  std::vector<size_t> starts(n + 1), cursor;
  std::vector<uint32_t> ordered(n);
  budget.step(n * 3);
  for (size_t row = 0; row < n; ++row) {
    if (s.parent[row] >= n)
      throw std::invalid_argument("Anatomical component outside owner");
    ++starts[s.parent[row] + 1];
  }
  for (size_t i = 0; i < n; ++i)
    starts[i + 1] += starts[i];
  cursor = starts;
  for (uint32_t row = 0; row < n; ++row)
    ordered[cursor[s.parent[row]]++] = row;
  size_t edges = 0;
  long double lengths = 0;
  for (size_t i = 0; i < n; ++i)
    for (size_t at = s.starts[i]; at < s.starts[i + 1]; ++at) {
      budget.step(16);
      auto j = s.columns[at];
      if (j >= n)
        throw std::invalid_argument("Anatomical edge outside owner");
      if (j > i) {
        double length = norm(sub(s.positions[i], s.positions[j]));
        if (!(length > 0) || !std::isfinite(length))
          throw std::invalid_argument(
              "Anatomical sampling needs positive metric edges");
        lengths += length;
        ++edges;
      }
    }
  if (!edges)
    throw std::invalid_argument("Anatomical sampling has no metric edges");
  out.mean_edge = double(lengths / edges);
  if (!(out.mean_edge > 0) || !std::isfinite(out.mean_edge))
    throw std::invalid_argument("Anatomical sampling has no finite mean edge");
  SurfaceQuery query(m, [&] {
    budget.step();
    return false;
  });
  size_t active_row = 0;
  P ray{};
  SurfaceQueryOptions qo;
  qo.cancelled = [&] {
    budget.step();
    return false;
  };
  qo.triangle_filter = [&](uint32_t face) {
    budget.step();
    size_t at = size_t(face) * 3;
    if (s.surface_parent[s.raw_to_row[m.indices[at]]] !=
        s.surface_parent[active_row])
      return false;
    for (int k = 0; k < 3; ++k) {
      budget.step();
      if (s.raw_to_row[m.indices[at + k]] == active_row)
        return false;
    }
    budget.step(16);
    auto point = [&](uint32_t v) {
      auto p = m.positions[v];
      return P{p.x, p.y, p.z};
    };
    auto a = point(m.indices[at]), b = point(m.indices[at + 1]),
         c = point(m.indices[at + 2]);
    auto u = sub(b, a), v = sub(c, a);
    P normal{u[1] * v[2] - u[2] * v[1], u[2] * v[0] - u[0] * v[2],
             u[0] * v[1] - u[1] * v[0]};
    double value = dot(normal, ray), scale = 0;
    for (int k = 0; k < 3; ++k)
      scale += std::abs(normal[k] * ray[k]);
    return value <= 16 * std::numeric_limits<double>::epsilon() * scale;
  };
  struct Candidate {
    double distance;
    uint32_t row;
  };
  std::vector<Candidate> heap;
  heap.reserve(n);
  auto less = [&](const Candidate &a, const Candidate &b) {
    budget.step();
    return a.distance != b.distance ? a.distance < b.distance : a.row < b.row;
  };
  auto exchange = [&](size_t a, size_t b) {
    budget.step(3);
    std::swap(heap[a], heap[b]);
  };
  auto down = [&](size_t at) {
    while (at < heap.size()) {
      size_t child = at * 2 + 1;
      if (child >= heap.size())
        break;
      if (child + 1 < heap.size() && less(heap[child + 1], heap[child]))
        ++child;
      if (!less(heap[child], heap[at]))
        break;
      exchange(at, child);
      at = child;
    }
  };
  auto pop = [&] {
    budget.step();
    auto c = heap.front();
    exchange(0, heap.size() - 1);
    heap.pop_back();
    if (!heap.empty())
      down(0);
    return c;
  };
  for (size_t source_index = 0; source_index < sources.size(); ++source_index) {
    const auto &source = sources[source_index];
    auto delta = sub(source.tail, source.head);
    double length = norm(delta);
    if (!std::isfinite(length))
      throw std::invalid_argument("Anatomical source length overflow");
    P axis = length ? delta : point_axes[source_index];
    double axis_length = norm(axis);
    for (double v : axis)
      if (!std::isfinite(v))
        throw std::invalid_argument("Nonfinite anatomical point axis");
    if (!std::isfinite(axis_length))
      throw std::invalid_argument("Anatomical point axis overflow");
    if (axis_length)
      for (double &v : axis)
        v /= axis_length;
    for (uint32_t root = 0; root < n; ++root) {
      budget.step();
      if (starts[root] == starts[root + 1] ||
          !(component_usage[root][source.bone / 64] &
            (uint64_t(1) << (source.bone % 64))))
        continue;
      AnatomicalProjectionRecord record;
      record.bone = source.bone;
      record.component = root;
      record.physical_length = length;
      if (!axis_length) {
        record.support = AnatomicalSupport::PointAxisMissing;
        out.all_axially_complete = false;
        out.records.push_back(record);
        continue;
      }
      double lo = INFINITY, hi = -INFINITY;
      for (size_t at = starts[root]; at < starts[root + 1]; ++at) {
        budget.step(16);
        double t = dot(sub(s.positions[ordered[at]], source.head), axis);
        lo = std::min(lo, t);
        hi = std::max(hi, t);
      }
      if (length) {
        record.supported_begin = std::max(0., lo);
        record.supported_end = std::min(length, hi);
        if (record.supported_end < record.supported_begin) {
          record.support = AnatomicalSupport::Outside;
          out.all_axially_complete = false;
          out.records.push_back(record);
          continue;
        }
        record.support =
            record.supported_begin == 0 && record.supported_end == length
                ? AnatomicalSupport::SegmentFull
                : AnatomicalSupport::SegmentClipped;
        out.all_axially_complete &=
            record.support == AnatomicalSupport::SegmentFull;
      } else {
        if (lo > 0 || hi < 0) {
          record.support = AnatomicalSupport::Outside;
          out.all_axially_complete = false;
          out.records.push_back(record);
          continue;
        }
        record.support = AnatomicalSupport::PointAxial;
      }
      double span = record.supported_end - record.supported_begin;
      double intervals = std::ceil(span / out.mean_edge);
      size_t component_rows = starts[root + 1] - starts[root];
      if (!std::isfinite(intervals) ||
          intervals + 1 > double((budget.maximum - budget.used) /
                                 (16 * component_rows + 1)))
        throw std::length_error(
            "Anatomical sample construction exceeds remaining work");
      record.samples =
          span > 0 ? std::max(size_t(2), size_t(intervals) + 1) : 1;
      for (size_t sample = 0; sample < record.samples; ++sample) {
        budget.stop();
        ++out.points;
        double distance = record.samples == 1
                              ? record.supported_begin
                              : (sample == record.samples - 1
                                     ? record.supported_end
                                     : record.supported_begin +
                                           span * double(sample) /
                                               double(record.samples - 1));
        P target = source.head;
        for (int k = 0; k < 3; ++k)
          target[k] += distance * axis[k];
        heap.clear();
        for (size_t at = starts[root]; at < starts[root + 1]; ++at) {
          budget.step(16);
          auto row = ordered[at];
          double d = norm(sub(target, s.positions[row]));
          if (d <= o.maximum_seed_distance)
            heap.push_back({d, row});
        }
        for (size_t at = heap.size() / 2; at-- > 0;)
          down(at);
        double best = INFINITY;
        bool visible = false;
        while (!heap.empty()) {
          if (visible && heap.front().distance > best * (1 + 1e-4))
            break;
          auto c = pop();
          active_row = c.row;
          ray = sub(target, s.positions[c.row]);
          bool clear = true;
          if (c.distance > 0) {
            ++out.rays;
            auto p = s.positions[c.row];
            clear =
                !query.raycast({float(p[0]), float(p[1]), float(p[2])},
                               {float(ray[0]), float(ray[1]), float(ray[2])},
                               float(c.distance), qo);
          }
          if (clear) {
            visible = true;
            best = std::min(best, c.distance);
            out.candidate_anchors[c.row][source.bone / 64] |=
                uint64_t(1) << (source.bone % 64);
          }
        }
        if (!visible) {
          record.support = AnatomicalSupport::Occluded;
          out.all_axially_complete = false;
        }
      }
      out.records.push_back(record);
    }
  }
  budget.stop();
  return out;
}
} // namespace dome::detail
