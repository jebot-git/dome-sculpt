// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/weight-edit.hpp"
#include "weight-brush-kernel.hpp"
#include "weight-preview.hpp"
#include <array>
#include <set>
#ifdef DOME_WEIGHT_BRUSH_REFERENCE
namespace dome::detail::weight_brush {
namespace {
class ReferenceQuery final:public Query {
  std::vector<Capsule> direct_,reflected_;
  std::vector<Vec3> cached_points_;
  std::vector<float> cached_hits_;

  size_t dispatch(std::span<const Vec3> points,unsigned sides,std::span<Hit> hits,
                  std::span<float> previous={}){
    if(points.size()!=hits.size())throw std::logic_error("Invalid weight brush reference positions");
    const Stroke stroke{direct_.data(),reflected_.data(),uint32_t(direct_.size()),sides};
    size_t count=0;
    for(size_t i=0;i<points.size();++i){
      const auto alpha=changed_falloff(stroke,points[i],previous.empty()?nullptr:&previous[i]);
      if(alpha!=0)hits[count++]={uint32_t(i),alpha};
    }
    return count;
  }
public:
  ReferenceQuery(std::span<const Capsule> direct,std::span<const Capsule> reflected):
      direct_(direct.begin(),direct.end()),reflected_(reflected.begin(),reflected.end()) {}
  void set_capsules(std::span<const Capsule> direct,std::span<const Capsule> reflected) override {
    direct_.assign(direct.begin(),direct.end());
    reflected_.assign(reflected.begin(),reflected.end());
  }
  size_t search(std::span<const Vec3> points,unsigned sides,std::span<Hit> hits) override {
    return dispatch(points,sides,hits);
  }
  bool cache_points(std::span<const std::span<const Vec3>> surfaces,
                    const std::function<bool()> &cancelled) override {
    if(!cached_points_.empty())throw std::logic_error("A weight preview already owns its posed vertices");
    size_t count=0;
    for(auto part:surfaces){
      if(part.size()>maximum_vertices-count)throw std::length_error("Weight preview vertex budget exceeded");
      count+=part.size();
    }
    if(!count)return false;
    std::vector<Vec3> points;
    std::vector<float> maxima;
    try{
      points.reserve(count);
      maxima.resize(count);
    }catch(const std::bad_alloc &){return false;}
    for(auto part:surfaces){
      if(cancelled&&cancelled())throw std::runtime_error("Weight painting cancelled");
      points.insert(points.end(),part.begin(),part.end());
    }
    cached_points_=std::move(points);
    cached_hits_=std::move(maxima);
    return true;
  }
  void clear_cached_hits() override {
    if(cached_hits_.empty())throw std::logic_error("Weight preview has no cached hits");
    std::ranges::fill(cached_hits_,0.f);
  }
  size_t search_cached(size_t first,unsigned sides,std::span<Hit> hits) override {
    if(first>cached_points_.size()||hits.size()>cached_points_.size()-first||hits.empty())
      throw std::logic_error("Invalid cached weight brush range");
    return dispatch(std::span(cached_points_).subspan(first,hits.size()),sides,hits,
                    std::span(cached_hits_).subspan(first,hits.size()));
  }
};
}
std::unique_ptr<Query> upload(std::span<const Capsule> direct,std::span<const Capsule> reflected,bool){
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
std::span<const Vec3> posed_positions(const WeightPaintSurface &surface){
  return surface.positions.empty()?std::span<const Vec3>(surface.rest->positions):surface.positions;
}
} // namespace
WeightEditResult edit_weights(std::span<const WeightPaintSurface> surfaces,
                              size_t bones, const WeightBrush &brush,
                              WeightEditOptions options,
                              const std::function<bool()> &cancelled) {
  return edit_weights(surfaces, bones, std::span<const WeightBrush>(&brush, 1), std::move(options), cancelled);
}
namespace {
bool same_capsule(const WeightBrush &a,const WeightBrush &b) {
  const auto point=[](Vec3 p,Vec3 q){return p.x==q.x&&p.y==q.y&&p.z==q.z;};
  return point(a.from,b.from)&&point(a.to,b.to)&&a.radius==b.radius&&a.strength==b.strength&&
      a.pressure==b.pressure&&a.value==b.value&&a.joint==b.joint&&a.mode==b.mode;
}

WeightEditResult edit_stroke(std::span<const WeightPaintSurface> surfaces,size_t bones,
    std::span<const WeightBrush> brushes,WeightEditOptions options,detail::WeightFalloffPreview *preview,
    const std::function<bool()> &cancelled) {
  if (brushes.empty() || brushes.size() > 4096)
    throw std::length_error("Weight gesture must contain 1 to 4096 capsule samples");
  const auto &brush = brushes.front();
  auto unit = [](float value) { return std::isfinite(value) && value >= 0 && value <= 1; };
  if (!bones || bones > 256 || brush.joint >= bones || !bounded(brush.from) ||
      !bounded(brush.to) || !std::isfinite(brush.radius) || brush.radius <= 0 ||
      brush.radius > 100 || !unit(brush.strength) || !unit(brush.pressure) ||
      !unit(brush.value) || surfaces.empty() || surfaces.size() > 10000 ||
      options.maximum_vertices > field::maximum_vertices || !options.maximum_vertices ||
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
  size_t prefix=0;
  if(preview&&preview->prefix.size()<=brushes.size()&&
     std::equal(preview->prefix.begin(),preview->prefix.end(),brushes.begin(),same_capsule))
    prefix=preview->prefix.size();
  const auto tail=brushes.subspan(prefix);
  std::vector<field::Capsule> direct_stroke,reflected_stroke;
  direct_stroke.reserve(tail.size());
  for(const auto &sample:tail)direct_stroke.push_back(capsule(sample,sample.from,sample.to));
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
    reflected_stroke.reserve(tail.size());
    for(const auto &sample:tail)
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
    for (auto p : posed_positions(surface))
      if (!bounded(p)) throw std::invalid_argument("Weight position outside finite coordinate bounds");
  }
  const size_t tests_per_vertex=brushes.size()*(options.symmetry?2:1);
  const size_t batch=std::min(field::batch_capacity,
      field::batch_capsules/(std::max(tail.size(),size_t{1})*(options.symmetry?2:1)));
  const auto vertex_cost=1+tests_per_vertex;
  if(preview&&vertices>(options.maximum_work-work)/vertex_cost)
    throw std::runtime_error("Weight brush work budget exceeded");
  if(preview&&!prefix){
    if(preview->resident)preview->query->clear_cached_hits();
    std::ranges::fill(preview->maxima,0.f);
  }
  const auto brush_sides=[&](const WeightPaintSurface &surface){
    const auto &joints=surface.joints?*surface.joints:ordinary;
    return (joints.direct?field::Direct:0u)|(joints.mirrored&&options.symmetry?field::Reflected:0u);
  };
  std::unique_ptr<field::Query> temporary;
  auto &query=preview?preview->query:temporary;
  bool staged=false;
  std::vector<field::Hit> hits;
  const auto stage=[&]{
    if(staged)return;
    if(query)query->set_capsules(direct_stroke,reflected_stroke);
    else {
      query=field::upload(direct_stroke,reflected_stroke,preview!=nullptr);
      if(preview){
        std::array<std::vector<size_t>,(field::Direct|field::Reflected)+1> groups;
        for(size_t i=0;i<surfaces.size();++i)
          if(!posed_positions(surfaces[i]).empty())groups[brush_sides(surfaces[i])].push_back(i);

        std::vector<std::span<const Vec3>> posed;
        posed.reserve(surfaces.size());
        preview->offsets.resize(surfaces.size());
        size_t offset=0;
        // A part keeps its original publication slot. Only its device
        // storage is grouped by the admissible direct/reflected sides.
        for(unsigned sides=field::Direct;sides<groups.size();++sides){
          const auto first=offset;
          for(auto part:groups[sides]){
            preview->offsets[part]=offset;
            posed.push_back(posed_positions(surfaces[part]));
            offset+=posed.back().size();
          }
          if(offset!=first)preview->ranges.push_back({first,offset-first,sides});
        }
        preview->maxima.resize(offset);
        preview->resident=query->cache_points(posed,cancelled);
      }
    }
    hits.resize(std::min(batch,vertices));
    staged=true;
  };
  if(preview&&!tail.empty()&&std::ranges::any_of(surfaces,[&](const auto &surface){
       return brush_sides(surface)&&!posed_positions(surface).empty();
     }))stage();
  const bool resident=preview&&preview->resident;
  if(resident){
    step(vertices*vertex_cost);
    if(!tail.empty()){
      for(const auto &range:preview->ranges){
        for(size_t i=0;i<range.count;i+=batch){
          const auto first=range.first+i;
          const auto count=std::min(batch,range.count-i);
          step(0);
          const auto affected=query->search_cached(first,range.sides,std::span(hits).first(count));
          step(0);
          for(const auto &hit:std::span(hits).first(affected))preview->maxima[first+hit.vertex]=hit.alpha;
        }
      }
    }
  }
  WeightEditResult result;
  for(size_t part=0;part<surfaces.size();++part){
    const auto &surface=surfaces[part];
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
    const auto positions=posed_positions(surface);
    const auto sides=brush_sides(surface);
    const auto assign=[&](size_t vertex,float signed_alpha){
      ++edited.affected;
      const auto joint=signed_alpha>0?*joints.direct:*joints.mirrored;
      const float alpha=std::abs(signed_alpha);
      const auto &before=original.vertices[vertex];
      step(before.size());
      const float old=influence(before,joint);
      float target=old;
      switch (brush.mode) {
      case WeightBrushMode::Add: target=std::min(1.f,old+alpha*brush.value); break;
      case WeightBrushMode::Subtract: target=std::max(0.f,old-alpha*brush.value); break;
      case WeightBrushMode::Replace: target=old+alpha*(brush.value-old); break;
      case WeightBrushMode::Smooth: {
        if(neighbors[vertex].empty())return;
        double sum=0;
        for(auto adjacent:neighbors[vertex]){
          step(original.vertices[adjacent].size());
          sum+=influence(original.vertices[adjacent],joint);
        }
        target=old+alpha*(float(sum/neighbors[vertex].size())-old);
        break;
      }
      }
      if(std::abs(target-old)<1e-7f)return;
      try {
        edited.weights.vertices[vertex]=paint_weight(before,joint,target,joints.bones,joints.locks,options.limits);
      } catch (const std::logic_error &error) {
        ++edited.constrained;
        if(result.diagnostics.size()<8)
          result.diagnostics.push_back("Object "+std::to_string(surface.object)+
              " vertex "+std::to_string(vertex)+": "+error.what());
      }
      if(edited.weights.vertices[vertex]!=before)++edited.changed;
    };
    if(resident){
      if(sides&&!positions.empty()){
        const auto row=std::span(preview->maxima).subspan(preview->offsets[part],positions.size());
        for(size_t v=0;v<row.size();++v)if(row[v]!=0)assign(v,row[v]);
      }
    }else{
      for(size_t first=0;first<positions.size();first+=batch){
        const auto count=std::min(batch,positions.size()-first);
        step(count*vertex_cost);
        if(!sides)continue;
        size_t affected=0;
        if(!tail.empty()){
          stage();
          affected=query->search(positions.subspan(first,count),sides,std::span(hits).first(count));
          step(0);
        }
        if(preview){
          auto row=std::span(preview->maxima).subspan(preview->offsets[part]+first,count);
          for(const auto &hit:std::span(hits).first(affected)){
            auto &prior=row[hit.vertex];
            if(field::stronger(hit.alpha,prior))prior=hit.alpha;
          }
          for(size_t v=0;v<count;++v)if(row[v]!=0)assign(first+v,row[v]);
        }else{
          for(const auto &hit:std::span(hits).first(affected))assign(first+hit.vertex,hit.alpha);
        }
      }
    }
    edited.weights.validate(mesh.positions.size(), joints.bones);
    result.affected += edited.affected; result.changed += edited.changed;
    result.constrained += edited.constrained;
    result.surfaces.push_back(std::move(edited));
  }
  step();
  if(preview)preview->prefix.assign(brushes.begin(),brushes.end());
  return result;
}
} // namespace

WeightEditResult edit_weights(std::span<const WeightPaintSurface> surfaces,size_t bones,
    std::span<const WeightBrush> brushes,WeightEditOptions options,const std::function<bool()> &cancelled) {
  return edit_stroke(surfaces,bones,brushes,std::move(options),nullptr,cancelled);
}

WeightEditResult detail::preview_weights(std::span<const WeightPaintSurface> surfaces,size_t bones,
    std::span<const WeightBrush> brushes,WeightEditOptions options,WeightFalloffPreview &preview,
    const std::function<bool()> &cancelled) {
  try{
    return edit_stroke(surfaces,bones,brushes,std::move(options),&preview,cancelled);
  }catch(...){
    preview={};
    throw;
  }
}
} // namespace dome
