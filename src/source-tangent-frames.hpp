// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/import.hpp"
#include "dome/tangents.hpp"
namespace dome::detail {
struct SourceTangentFrames {
  // Corner identity is intentional: MikkTSpace can split a shared vertex at a
  // mirrored seam. Export must split/remap all attributes before writing these.
  std::vector<Tangent> base;
  std::vector<std::vector<Vec3>> targets;
  std::vector<Vec3> normals;
  std::vector<std::vector<Vec3>> normal_targets;
  // Original primitive vertex identity for every corner, independent of any
  // later sampling-atlas vertex welding/splitting.
  size_t origin_vertex_count = 0;
  std::vector<uint32_t> origin_vertices;
  bool generated = false;
};
// frame_uv is the original normal-frame coordinate basis, selected explicitly
// by the source format/version contract. It is independent of a new sampling
// atlas. This helper does not infer KHR/Unity frame semantics from sampling ST.
inline SourceTangentFrames source_tangent_frames(
    const ImportedPrimitive &source,
    std::span<const std::array<float,2>> frame_uv,
    const std::function<bool()> &cancelled = {}, size_t max_work = 64*1024*1024) {
  auto stop=[&] { if(cancelled && cancelled())throw std::runtime_error("Source tangent generation cancelled"); };
  stop();source.mesh.validate();
  if(!max_work || max_work>256*1024*1024 || source.morph_targets.size()>4096)
    throw std::length_error("Source tangent generation resource policy exceeded");
  const size_t vertices=source.mesh.positions.size(), corners=source.mesh.indices.size();
  const size_t stride=vertices+corners;
  if(stride && source.morph_targets.size()+1>max_work/stride)
    throw std::length_error("Source tangent generation corner work limit exceeded");
  // Bound the largest possible immutable result independently of work policy.
  // Base T4+N3+origin ID and targets T3+N3 need 32 and 24 bytes/corner.
  constexpr size_t bytes_limit=256*1024*1024;
  if(corners>bytes_limit/32 || (corners && source.morph_targets.size()>(bytes_limit-corners*32)/(corners*24)))
    throw std::length_error("Source tangent result memory limit exceeded");
  auto vectors=[&](const auto &v) {
    if(!v.empty() && v.size()!=vertices)throw std::invalid_argument("Source tangent attribute identity differs from positions");
    for(auto x:v)if(!finite(x))throw std::invalid_argument("Source tangent direction/position is nonfinite");
  };
  SourceTangentFrames result;result.generated=source.tangents.empty();
  result.origin_vertex_count=vertices;result.origin_vertices=source.mesh.indices;
  result.targets.resize(source.morph_targets.size());
  result.normal_targets.resize(source.morph_targets.size());
  result.normals.reserve(corners);
  if(source.mesh.normals.size()!=vertices)throw std::invalid_argument("Source tangent frame requires vertex normals");
  for(auto id:source.mesh.indices)result.normals.push_back(source.mesh.normals[id]);
  if(!result.generated) {
    result.base.reserve(corners);
    if(source.tangents.size()!=vertices)throw std::invalid_argument("Authored source tangent count differs from positions");
    for(auto t:source.tangents)
      if(!finite(Vec3{t[0],t[1],t[2]}) || (t[3]!=-1 && t[3]!=1))
        throw std::invalid_argument("Authored source tangent is invalid");
    for(auto id:source.mesh.indices)result.base.push_back(source.tangents[id]);
  } else {
    if(frame_uv.size()!=vertices)throw std::invalid_argument("Generated source tangents require an explicit original frame UV basis");
    Mesh base=source.mesh;base.uv.assign(frame_uv.begin(),frame_uv.end());
    result.base=tangent_corners(base,cancelled);
  }
  for(size_t i=0;i<source.morph_targets.size();++i) {
    stop();const auto &target=source.morph_targets[i];vectors(target.positions);vectors(target.normals);
    if(!target.normals.empty()) {
      result.normal_targets[i].reserve(corners);
      for(auto id:source.mesh.indices)result.normal_targets[i].push_back(target.normals[id]);
    }
    if(!result.generated) {
      vectors(target.tangents);
      if(!target.tangents.empty()) {
        result.targets[i].reserve(corners);
        for(auto id:source.mesh.indices)result.targets[i].push_back(target.tangents[id]);
      }
      continue;
    }
    // glTF requires target generation when base tangents are generated. Any
    // supplied target TANGENT deltas are deliberately ignored on this path.
    Mesh posed=source.mesh;posed.uv.assign(frame_uv.begin(),frame_uv.end());
    for(size_t v=0;v<vertices;++v) {
      if(!target.positions.empty())posed.positions[v]+=target.positions[v];
      if(!target.normals.empty())posed.normals[v]+=target.normals[v];
    }
    auto tangent=tangent_corners(posed,cancelled);auto &delta=result.targets[i];delta.reserve(corners);
    for(size_t c=0;c<corners;++c) {
      if(tangent[c][3]!=result.base[c][3])
        throw std::invalid_argument("Generated target changes tangent handedness which glTF morphs cannot encode");
      delta.push_back({tangent[c][0]-result.base[c][0],tangent[c][1]-result.base[c][1],tangent[c][2]-result.base[c][2]});
    }
  }
  stop();return result;
}
struct SourceLocalFrame { Vec3 tangent, normal; float handedness; };
// Inputs/outputs remain in the original local basis, before any skin or node
// transform. The caller maps native slots to exact source target identities.
inline SourceLocalFrame source_local_frame(const SourceTangentFrames &frames,
    size_t corner,std::span<const float> source_weights) {
  if(source_weights.size()>4096 || corner>=frames.base.size() || frames.normals.size()!=frames.base.size() ||
     frames.targets.size()!=frames.normal_targets.size() || source_weights.size()!=frames.targets.size())
    throw std::invalid_argument("Source local frame identities differ");
  auto t=frames.base[corner];SourceLocalFrame result{{t[0],t[1],t[2]},frames.normals[corner],t[3]};
  for(size_t i=0;i<source_weights.size();++i) {
    float w=source_weights[i];if(!std::isfinite(w))throw std::invalid_argument("Source local morph weight is nonfinite");
    auto add=[&](const std::vector<Vec3> &delta,Vec3 &v) {
      if(delta.empty())return;
      if(delta.size()!=frames.base.size())throw std::invalid_argument("Source local morph corner identity differs");
      v+=delta[corner]*w;
    };
    add(frames.targets[i],result.tangent);add(frames.normal_targets[i],result.normal);
  }
  if(!finite(result.tangent) || !finite(result.normal) || length(result.tangent)<1e-10f || length(result.normal)<1e-10f ||
     (result.handedness!=-1 && result.handedness!=1))
    throw std::invalid_argument("Source local morph frame is singular or invalid");
  return result;
}
} // namespace dome::detail
