// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/weight-edit.hpp"
#include "weight-brush-kernel.hpp"
#include <set>
#ifdef DOME_WEIGHT_BRUSH_REFERENCE
namespace dome::detail::weight_brush {
namespace {
class ReferenceQuery final:public Query {
  const std::vector<Capsule> direct_,reflected_;
public:
  ReferenceQuery(std::span<const Capsule> direct,std::span<const Capsule> reflected):
      direct_(direct.begin(),direct.end()),reflected_(reflected.begin(),reflected.end()) {}
  size_t search(std::span<const Vec3> points,unsigned sides,std::span<Hit> hits) override {
    const Stroke stroke{direct_.data(),reflected_.data(),uint32_t(direct_.size()),sides};
    size_t count=0;
    for(size_t i=0;i<points.size();++i)
      if(const auto alpha=strongest(stroke,points[i]);alpha!=0)
        hits[count++]={uint32_t(i),alpha};
    return count;
  }
};
}
std::unique_ptr<Query> upload(std::span<const Capsule> direct,std::span<const Capsule> reflected){
  return std::make_unique<ReferenceQuery>(direct,reflected);
}
}
#endif

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
namespace field=detail::weight_brush;
field::Capsule capsule(const WeightBrush &brush,Vec3 from,Vec3 to){
  return field::capsule(from,to,brush.radius,brush.strength,brush.pressure);
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
  std::vector<field::Capsule> direct_stroke,reflected_stroke;
  direct_stroke.reserve(brushes.size());
  for(const auto &sample:brushes)direct_stroke.push_back(capsule(sample,sample.from,sample.to));
  if (options.symmetry) {
    const auto &symmetry = *options.symmetry;
    std::optional<AffineTransform> reflection;
    if (symmetry.object_to_brush) {
      auto local = AffineTransform{}.matrix();
      local[0] = -1;
      local[12] = 2 * double(symmetry.plane_x);
      reflection = *symmetry.object_to_brush * AffineTransform(local) * symmetry.object_to_brush->inverse();
    }
    auto point = [&](Vec3 p) {
      if (!reflection) return Vec3{2 * symmetry.plane_x - p.x, p.y, p.z};
      const auto q = reflection->point({p.x, p.y, p.z});
      const Vec3 result{float(q[0]), float(q[1]), float(q[2])};
      if (!bounded(result)) throw std::invalid_argument("Reflected weight brush leaves the coordinate bounds");
      return result;
    };
    reflected_stroke.reserve(brushes.size());
    for(const auto &sample:brushes)
      reflected_stroke.push_back(capsule(sample,point(sample.from),point(sample.to)));
  }
  const auto mirror_joint = options.symmetry ? options.symmetry->joints[brush.joint] : brush.joint;
  const WeightSurfaceJoints ordinary{bones, brush.joint, mirror_joint, options.locks};
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
    const auto &joints = surface.joints ? *surface.joints : ordinary;
    if (!joints.bones || joints.bones > 256 ||
        (joints.direct && *joints.direct >= joints.bones) ||
        (joints.mirrored && *joints.mirrored >= joints.bones) ||
        (!joints.locks.empty() && joints.locks.size() != joints.bones) ||
        std::ranges::any_of(joints.locks, [](auto lock){return lock > 1;}))
      throw std::invalid_argument("Invalid weight surface joint slots or locks");
    surface.weights->validate(count, joints.bones);
    if (!surface.positions.empty() && surface.positions.size() != count)
      throw std::invalid_argument("Weight preview position count differs from rest topology");
    auto positions = surface.positions.empty()
        ? std::span<const Vec3>(surface.rest->positions) : surface.positions;
    for (auto p : positions)
      if (!bounded(p)) throw std::invalid_argument("Weight position outside finite coordinate bounds");
  }
  const size_t tests_per_vertex=brushes.size()*(options.symmetry?2:1);
  const size_t batch=std::min(field::batch_capacity,field::batch_capsules/tests_per_vertex);
  std::unique_ptr<field::Query> query;
  std::vector<field::Hit> hits;
  WeightEditResult result;
  for (const auto &surface : surfaces) {
    const auto &mesh = *surface.rest;
    const auto &original = *surface.weights;
    const auto &joints = surface.joints ? *surface.joints : ordinary;
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
    const auto positions=surface.positions.empty()?std::span<const Vec3>(mesh.positions):surface.positions;
    const unsigned sides=(joints.direct?field::Direct:0u)|
                         (joints.mirrored&&options.symmetry?field::Reflected:0u);
    for(size_t first=0;first<positions.size();first+=batch){
      const auto count=std::min(batch,positions.size()-first);
      step(count*(1+tests_per_vertex));
      if(!sides)continue;
      if(!query){
        query=field::upload(direct_stroke,reflected_stroke);
        hits.resize(std::min(batch,vertices));
      }
      const auto affected=query->search(positions.subspan(first,count),sides,std::span(hits).first(count));
      step(0);
      edited.affected+=affected;
      for(const auto &hit:std::span(hits).first(affected)){
        const auto vertex=first+hit.vertex;
        const auto joint=hit.alpha>0?*joints.direct:*joints.mirrored;
        const float alpha=std::abs(hit.alpha);
        const auto &before=original.vertices[vertex];
        step(before.size());
        const float old=influence(before,joint);
        float target=old;
        switch (brush.mode) {
        case WeightBrushMode::Add: target=std::min(1.f,old+alpha*brush.value); break;
        case WeightBrushMode::Subtract: target=std::max(0.f,old-alpha*brush.value); break;
        case WeightBrushMode::Replace: target=old+alpha*(brush.value-old); break;
        case WeightBrushMode::Smooth: {
          if(neighbors[vertex].empty())continue;
          double sum=0;
          for(auto adjacent:neighbors[vertex]){
            step(original.vertices[adjacent].size());
            sum+=influence(original.vertices[adjacent],joint);
          }
          target=old+alpha*(float(sum/neighbors[vertex].size())-old);
          break;
        }
        }
        if(std::abs(target-old)<1e-7f)continue;
        try {
          edited.weights.vertices[vertex]=paint_weight(before,joint,target,joints.bones,joints.locks,options.limits);
        } catch (const std::logic_error &error) {
          ++edited.constrained;
          if(result.diagnostics.size()<8)
            result.diagnostics.push_back("Object "+std::to_string(surface.object)+
                " vertex "+std::to_string(vertex)+": "+error.what());
        }
        if(edited.weights.vertices[vertex]!=before)++edited.changed;
      }
    }
    edited.weights.validate(mesh.positions.size(), joints.bones);
    result.affected += edited.affected; result.changed += edited.changed;
    result.constrained += edited.constrained;
    result.surfaces.push_back(std::move(edited));
  }
  step();
  return result;
}
} // namespace dome
