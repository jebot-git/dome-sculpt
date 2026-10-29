// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-morph.hpp"
#include <set>
namespace dome::detail {
[[noreturn]] void SourceMorphTransport::fail(const char *s) const {
  throw std::invalid_argument("Source attribute export for " + object_.name +
                              ": " + s);
}
void SourceMorphTransport::check_cancel() const {
  if (cancelled_ && cancelled_())
    throw std::runtime_error("Source attribute export cancelled");
}
void SourceMorphTransport::visit() const {
  if (++work_ > max_work_)
    throw std::length_error("Source attribute export work limit exceeded");
  if (work_ == 1 || (work_ & 4095) == 0)
    check_cancel();
}
SourceMorphTransport::SourceMorphTransport(const ObjectSurface &o,
                                           SourceMorphSpace space,
                                           std::function<bool()> cancelled,
                                           size_t max_work)
    : object_(o), cancelled_(std::move(cancelled)), max_work_(max_work) {
  if (!max_work || max_work > 256 * 1024 * 1024)
    fail("invalid bounded-work policy");
  visit();
  o.mesh.validate();
  if (o.deformation)
    o.deformation->validate(o.mesh);
  if (!o.appearance)
    return;
  const auto &a = *o.appearance;
  if (!a.asset || a.asset->mode != ImportMode::PreserveGltf)
    fail("missing preserved source");
  for (const auto &p : a.asset->primitives)
    if (p.source_mesh == a.source_mesh &&
        p.source_primitive == a.source_primitive) {
      if (source_)
        fail("ambiguous source primitive");
      source_ = &p;
    }
  if (!source_)
    fail("missing source primitive");
  if (a.source_vertices.size() != o.mesh.positions.size())
    fail("source vertex count differs");
  for (const auto &t : source_->morph_targets)
    has_source_morph_channels_ |= !t.normals.empty() || !t.tangents.empty();
  bool needs_basis =
      !source_->tangents.empty() ||
      (has_source_morph_channels_ && o.deformation && o.deformation->morphs &&
       !o.deformation->morphs->targets.empty());
  if (!needs_basis)
    return;
  if (!a.source_basis_quaternion)
    fail("coordinate basis provenance is missing");
  auto q = *a.source_basis_quaternion;
  float norm = q.w * q.w + q.x * q.x + q.y * q.y + q.z * q.z;
  if (!std::isfinite(norm) || std::abs(norm - 1) > 1e-5f)
    fail("coordinate basis is not finite and unit");
  basis_ = space == SourceMorphSpace::Native ? q.unit() : Quat{};
  if (o.mesh.positions.empty())
    fail("empty source geometry");
  Vec3 p0{}, q0 = o.mesh.positions.front();
  float farthest = 0;
  for (size_t v = 0; v < a.source_vertices.size(); ++v) {
    visit();
    auto id = a.source_vertices[v];
    if (id >= source_->mesh.positions.size() ||
        id >= source_->mesh.normals.size())
      fail("source base vertex is outside its accessor");
    if (!finite(source_->mesh.positions[id]) ||
        !finite(source_->mesh.normals[id]))
      fail("source base direction/position is nonfinite");
    auto p = basis_.rotate(source_->mesh.positions[id]);
    if (!v)
      p0 = p;
    auto dp = p - p0;
    float distance = dot(dp, dp);
    if (distance > std::max(1e-8f, farthest)) {
      farthest = distance;
      scale_ = dot(o.mesh.positions[v] - q0, dp) / distance;
    }
  }
  Vec3 offset = q0 - p0 * scale_;
  frame_valid_ = std::isfinite(scale_) && scale_ > 0;
  for (size_t v = 0; v < a.source_vertices.size(); ++v) {
    visit();
    auto id = a.source_vertices[v];
    if (length(o.mesh.positions[v] -
               (basis_.rotate(source_->mesh.positions[id]) * scale_ + offset)) >
            1e-5f ||
        length(o.mesh.normals[v] - basis_.rotate(source_->mesh.normals[id])) >
            1e-5f)
      frame_valid_ = false;
  }
  if (has_source_morph_channels_ && o.deformation && o.deformation->morphs) {
    if (a.source_morph_targets.size() != o.deformation->morphs->targets.size())
      fail("explicit morph identity map is missing or stale");
    std::set<uint32_t> seen;
    for (auto id : a.source_morph_targets) {
      visit();
      if (id >= source_->morph_targets.size() || !seen.insert(id).second)
        fail("morph identities are not unique and in range");
    }
  }
}
void SourceMorphTransport::check_tangent_frame() const {
  if (tangents_checked_)
    return;
  if (!frame_valid_)
    fail("edited tangent frames need explicit tangent recomputation");
  const auto &a = *object_.appearance;
  for (size_t v = 0; v < a.source_vertices.size(); ++v) {
    visit();
    auto id = a.source_vertices[v];
    if (id >= source_->tangents.size())
      fail("source tangent vertex is missing");
    if (!source_->mesh.uv.empty() &&
        (id >= source_->mesh.uv.size() || object_.mesh.uv.empty() ||
         object_.mesh.uv[v] != source_->mesh.uv[id]))
      fail("edited tangent UVs need explicit tangent recomputation");
  }
  tangents_checked_ = true;
}
std::vector<std::array<float, 4>> SourceMorphTransport::base_tangents() const {
  std::vector<std::array<float, 4>> result;
  if (!source_ || source_->tangents.empty())
    return result;
  check_tangent_frame();
  for (auto id : object_.appearance->source_vertices) {
    visit();
    auto t = source_->tangents[id];
    if (!finite(Vec3{t[0], t[1], t[2]}) || (t[3] != 1 && t[3] != -1))
      fail("source tangent value is invalid");
    auto xyz = basis_.rotate({t[0], t[1], t[2]});
    result.push_back({xyz.x, xyz.y, xyz.z, t[3]});
  }
  check_cancel();
  return result;
}
SourceMorphChannels SourceMorphTransport::target(size_t target,
                                                 bool recompute) const {
  SourceMorphChannels result;
  check_cancel();
  if (!source_ || !has_source_morph_channels_)
    return result;
  if (!object_.deformation || !object_.deformation->morphs ||
      target >= object_.deformation->morphs->targets.size())
    fail("native morph target is missing");
  const auto &a = *object_.appearance;
  if (target >= a.source_morph_targets.size())
    fail("explicit native morph identity is missing");
  const auto &source =
      source_->morph_targets.at(a.source_morph_targets[target]);
  if (source.normals.empty() && source.tangents.empty())
    return result;
  bool unchanged = frame_valid_;
  const auto &native = object_.deformation->morphs->targets[target];
  for (size_t v = 0; v < a.source_vertices.size(); ++v) {
    visit();
    auto id = a.source_vertices[v];
    if (!source.positions.empty() && id >= source.positions.size())
      fail("source morph position vertex is missing");
    if (!source.positions.empty() && !finite(source.positions[id]))
      fail("source morph position is nonfinite");
    Vec3 delta = source.positions.empty()
                     ? Vec3{}
                     : basis_.rotate(source.positions[id]) * scale_;
    if (length(native.delta[v] - delta) > 1e-5f)
      unchanged = false;
  }
  if (!unchanged && !source.tangents.empty())
    fail("edited morph tangents need explicit tangent recomputation");
  if (!unchanged && !source.normals.empty() && !recompute)
    fail("edited morph normals need explicit recomputation policy");
  auto mapped = [&](const std::vector<Vec3> &values) {
    std::vector<Vec3> result;
    result.reserve(a.source_vertices.size());
    for (auto id : a.source_vertices) {
      visit();
      if (id >= values.size())
        fail("source direction vertex is missing");
      if (!finite(values[id]))
        fail("source morph direction is nonfinite");
      result.push_back(basis_.rotate(values[id]));
    }
    return result;
  };
  if (!source.normals.empty()) {
    if (unchanged)
      result.normals = mapped(source.normals);
    else {
      // Same area-weighted convention as native MorphSet::evaluate, without
      // rescanning/copying every unrelated target for each exported channel.
      std::vector<Vec3> normals(object_.mesh.positions.size());
      const auto &indices = object_.mesh.indices;
      for (size_t i = 0; i < indices.size(); i += 3) {
        visit();
        auto a = indices[i], b = indices[i + 1], c = indices[i + 2];
        auto pa = object_.mesh.positions[a] + native.delta[a];
        auto pb = object_.mesh.positions[b] + native.delta[b];
        auto pc = object_.mesh.positions[c] + native.delta[c];
        auto face = cross(pb - pa, pc - pa);
        if (!finite(face))
          fail("normal recomputation overflowed");
        normals[a] += face;
        normals[b] += face;
        normals[c] += face;
      }
      for (size_t v = 0; v < normals.size(); ++v) {
        visit();
        auto n = normalized(normals[v]);
        if (!finite(n))
          fail("normal recomputation is nonfinite");
        result.normals.push_back(n - object_.mesh.normals[v]);
      }
      result.normals_recomputed = true;
    }
  }
  if (!source.tangents.empty()) {
    if (source_->tangents.empty())
      fail("source morph tangent lacks its base tangent frame");
    check_tangent_frame();
    result.tangents = mapped(source.tangents);
  }
  check_cancel();
  return result;
}
} // namespace dome::detail
