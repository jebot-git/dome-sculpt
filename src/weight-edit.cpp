// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/weight-edit.hpp"
#include <set>
namespace dome {
namespace {
bool bounded(Vec3 p) {
  return finite(p) && std::abs(p.x) <= 1e6f && std::abs(p.y) <= 1e6f &&
         std::abs(p.z) <= 1e6f;
}
float influence(const VertexWeights &weights, uint32_t joint) {
  for (auto i : weights)
    if (i.joint == joint) return i.weight;
  return 0;
}
float capsule(Vec3 p, const WeightBrush &brush, bool mirror, float plane) {
  Vec3 from = brush.from, to = brush.to;
  if (mirror) { from.x = 2 * plane - from.x; to.x = 2 * plane - to.x; }
  auto direction = to - from;
  float norm = dot(direction, direction);
  float t = norm > 1e-20f ? std::clamp(dot(p - from, direction) / norm, 0.f, 1.f) : 0;
  float u = std::clamp(1 - length(p - (from + direction * t)) / brush.radius, 0.f, 1.f);
  return u * u * (3 - 2 * u) * brush.strength * brush.pressure;
}
} // namespace
WeightEditResult edit_weights(std::span<const WeightPaintSurface> surfaces,
                              size_t bones, const WeightBrush &brush,
                              WeightEditOptions options,
                              const std::function<bool()> &cancelled) {
  return edit_weights(surfaces, bones, std::span<const WeightBrush>(&brush, 1), std::move(options), cancelled);
}
WeightEditResult edit_weights(std::span<const WeightPaintSurface> surfaces,
                              size_t bones, std::span<const WeightBrush> brushes,
                              WeightEditOptions options,
                              const std::function<bool()> &cancelled) {
  if (brushes.empty() || brushes.size() > 4096)
    throw std::length_error("Weight gesture must contain 1 to 4096 capsule samples");
  const auto &brush = brushes.front();
  auto unit = [](float value) { return std::isfinite(value) && value >= 0 && value <= 1; };
  if (!bones || bones > 256 || brush.joint >= bones || !bounded(brush.from) ||
      !bounded(brush.to) || !std::isfinite(brush.radius) || brush.radius <= 0 ||
      brush.radius > 100 || !unit(brush.strength) || !unit(brush.pressure) ||
      !unit(brush.value) || surfaces.empty() || surfaces.size() > 10000 ||
      options.maximum_vertices > 2'000'000 || !options.maximum_vertices ||
      !options.maximum_work || options.maximum_work > 100'000'000 ||
      (!options.locks.empty() && options.locks.size() != bones) ||
      !options.limits.max_influences || options.limits.max_influences > 32 ||
      !std::isfinite(options.limits.prune) || options.limits.prune < 0 || options.limits.prune >= 1 ||
      brush.mode < WeightBrushMode::Add || brush.mode > WeightBrushMode::Smooth)
    throw std::invalid_argument("Invalid weight brush, locks or resource bounds");
  for (const auto &sample : brushes)
    if (!bounded(sample.from) || !bounded(sample.to) || !unit(sample.pressure) ||
        !unit(sample.strength) || !std::isfinite(sample.radius) || sample.radius <= 0 || sample.radius > 100 ||
        sample.joint != brush.joint || sample.mode != brush.mode || sample.value != brush.value)
      throw std::invalid_argument("Invalid or inconsistent weight gesture capsule");
  if (std::any_of(options.locks.begin(), options.locks.end(),
                  [](uint8_t lock) { return lock > 1; }))
    throw std::invalid_argument("Weight locks must be boolean");
  if (options.symmetry) {
    auto &s = *options.symmetry;
    if (!std::isfinite(s.plane_x) || std::abs(s.plane_x) > 1e6f || s.joints.size() != bones)
      throw std::invalid_argument("Explicit weight symmetry map required");
    for (size_t i = 0; i < bones; ++i)
      if (s.joints[i] >= bones || s.joints[s.joints[i]] != i)
        throw std::invalid_argument("Weight symmetry joint map is not involutive");
  }
  size_t vertices = 0, work = 0;
  auto step = [&](size_t count = 1) {
    if (count > options.maximum_work - work)
      throw std::runtime_error("Weight brush work budget exceeded");
    work += count;
    if (cancelled && cancelled()) throw std::runtime_error("Weight painting cancelled");
  };
  std::set<uint64_t> identities;
  // Validate every input before computing output; one invalid linked part must
  // not yield a partially edited avatar result.
  for (const auto &surface : surfaces) {
    step();
    if (!surface.object || !identities.insert(surface.object).second ||
        !surface.rest || !surface.weights)
      throw std::invalid_argument("Weight surface identity/data missing or duplicated");
    auto count = surface.rest->positions.size();
    if (count > options.maximum_vertices - vertices)
      throw std::runtime_error("Weight brush vertex budget exceeded");
    vertices += count;
    step(count + surface.rest->indices.size());
    surface.rest->validate();
    surface.weights->validate(count, bones);
    if (!surface.positions.empty() && surface.positions.size() != count)
      throw std::invalid_argument("Weight preview position count differs from rest topology");
    auto positions = surface.positions.empty()
        ? std::span<const Vec3>(surface.rest->positions) : surface.positions;
    for (auto p : positions)
      if (!bounded(p)) throw std::invalid_argument("Weight position outside finite coordinate bounds");
  }
  WeightEditResult result;
  for (const auto &surface : surfaces) {
    const auto &mesh = *surface.rest;
    const auto &original = *surface.weights;
    WeightSurfaceEdit edited{surface.object, morph_topology_id(mesh), original};
    std::vector<std::vector<uint32_t>> neighbors;
    if (brush.mode == WeightBrushMode::Smooth) {
      neighbors.resize(mesh.positions.size());
      for (size_t i = 0; i < mesh.indices.size(); i += 3) {
        step(6);
        for (size_t e = 0; e < 3; ++e) {
          auto a = mesh.indices[i + e], b = mesh.indices[i + (e + 1) % 3];
          neighbors[a].push_back(b); neighbors[b].push_back(a);
        }
      }
      for (auto &row : neighbors) {
        std::sort(row.begin(), row.end());
        row.erase(std::unique(row.begin(), row.end()), row.end());
      }
    }
    auto plane = options.symmetry ? options.symmetry->plane_x : 0;
    auto reflected = options.symmetry ? options.symmetry->joints[brush.joint] : brush.joint;
    for (size_t vertex = 0; vertex < mesh.positions.size(); ++vertex) {
      step();
      Vec3 p = surface.positions.empty() ? mesh.positions[vertex] : surface.positions[vertex];
      float direct = 0, mirror = 0;
      for (const auto &sample : brushes) {
        step(options.symmetry ? 2 : 1);
        direct = std::max(direct, capsule(p, sample, false, plane));
        if (options.symmetry) mirror = std::max(mirror, capsule(p, sample, true, plane));
      }
      if (direct == 0 && mirror == 0) continue;
      ++edited.affected;
      bool constrained = false;
      auto assign = [&](uint32_t joint, float alpha) {
        if (alpha == 0) return;
        step(original.vertices[vertex].size());
        const auto &before = original.vertices[vertex];
        float old = influence(before, joint), target = old;
        switch (brush.mode) {
        case WeightBrushMode::Add: target = std::min(1.f, old + alpha * brush.value); break;
        case WeightBrushMode::Subtract: target = std::max(0.f, old - alpha * brush.value); break;
        case WeightBrushMode::Replace: target = old + alpha * (brush.value - old); break;
        case WeightBrushMode::Smooth: {
          if (neighbors[vertex].empty()) return;
          double sum = 0;
          for (auto adjacent : neighbors[vertex]) {
            step(original.vertices[adjacent].size());
            sum += influence(original.vertices[adjacent], joint);
          }
          target = old + alpha * (float(sum / neighbors[vertex].size()) - old);
          break;
        }
        }
        if (std::abs(target - old) < 1e-7f) return;
        try {
          edited.weights.vertices[vertex] = paint_weight(
              edited.weights.vertices[vertex], joint, target, bones,
              options.locks, options.limits);
        } catch (const std::logic_error &error) {
          constrained = true;
          if (result.diagnostics.size() < 8)
            result.diagnostics.push_back("Object " + std::to_string(surface.object) +
                " vertex " + std::to_string(vertex) + ": " + error.what());
        }
      };
      if (reflected == brush.joint) assign(brush.joint, std::max(direct, mirror));
      else {
        // Central overlap uses the stronger side, avoiding order-dependent
        // redistribution between two opposing joint assignments.
        if (direct >= mirror) assign(brush.joint, direct);
        else assign(reflected, mirror);
      }
      if (constrained) ++edited.constrained;
      if (edited.weights.vertices[vertex] != original.vertices[vertex]) ++edited.changed;
    }
    edited.weights.validate(mesh.positions.size(), bones);
    result.affected += edited.affected; result.changed += edited.changed;
    result.constrained += edited.constrained;
    result.surfaces.push_back(std::move(edited));
  }
  step();
  return result;
}
} // namespace dome
