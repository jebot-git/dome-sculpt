// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "source-frame-skin.hpp"
namespace dome::detail {
// Reauthor native shape KEYS, matching the creator's existing rule: shape each
// base/target key first, then linearly blend the resulting native attributes.
// This is not evaluation of a nonlinear shape at a blended original pose.
// Differential matrices map original source directions to native rest axes.
inline SourceTangentFrames source_shape_key_frames(const ImportedPrimitive &source,
    const SourceTangentFrames &original,const std::function<RigMatrix(Vec3)> &differential,
    const std::function<bool()> &cancelled={},size_t max_work=64*1024*1024) {
  source.mesh.validate();
  const size_t count=source.mesh.indices.size(),vertices=source.mesh.positions.size(),targets=source.morph_targets.size();
  if(!differential || !max_work || max_work>256*1024*1024 || targets>4096 || !vertices || !count || vertices>2'000'000 || count>6'000'000 ||
     original.base.size()!=count || original.normals.size()!=count || original.targets.size()!=targets ||
     original.normal_targets.size()!=targets || original.origin_vertices!=source.mesh.indices ||
     original.origin_vertex_count!=vertices || (count+vertices && targets+1>max_work/(count+vertices)))
    throw std::invalid_argument("Source shape frame identities or work bounds differ");
  constexpr size_t bytes_limit=256*1024*1024;
  if(count>bytes_limit/32 || (count && targets>(bytes_limit-count*32)/(count*24)))
    throw std::length_error("Native shape frame result exceeds memory budget");
  if(count>bytes_limit/8 || vertices>(bytes_limit-count*8)/(2*sizeof(RigMatrix)))
    throw std::length_error("Native shape Jacobian working storage exceeds budget");
  auto stop=[&]{if(cancelled && cancelled())throw std::runtime_error("Source shape frame generation cancelled");};
  SourceTangentFrames result;result.generated=original.generated;
  result.origin_vertex_count=original.origin_vertex_count;result.origin_vertices=original.origin_vertices;
  result.base.reserve(count);result.normals.reserve(count);result.targets.assign(targets,{});result.normal_targets.assign(targets,{});
  std::vector<float> tangent_length(count),normal_length(count);
  // Process one key's Jacobians at a time; shared corner vertices reuse them.
  std::vector<RigMatrix> linear(vertices),normal(vertices);
  auto matrices=[&](const std::vector<Vec3> *delta) {
    if(delta && !delta->empty() && delta->size()!=vertices)throw std::invalid_argument("Source shape target position identity differs");
    for(size_t v=0;v<vertices;++v) {
      if((v&1023)==0)stop();
      Vec3 p=source.mesh.positions[v];if(delta && !delta->empty())p+=(*delta)[v];
      if(!finite(p))throw std::invalid_argument("Source shape key position is nonfinite");
      linear[v]=differential(p);normal[v]=source_frame_inverse_transpose(linear[v]);
    }
  };
  auto sign=[](const RigMatrix &m) {
    const double determinant=double(m[0])*(double(m[5])*m[10]-double(m[9])*m[6])-
      double(m[4])*(double(m[1])*m[10]-double(m[9])*m[2])+double(m[8])*(double(m[1])*m[6]-double(m[5])*m[2]);
    return determinant<0?-1.f:1.f;
  };
  matrices(nullptr);
  for(size_t c=0;c<count;++c) {
    if((c&1023)==0)stop();
    auto v=source.mesh.indices[c];auto t=original.base[c];
    auto T=source_frame_direction(linear[v],{t[0],t[1],t[2]}),N=source_frame_direction(normal[v],original.normals[c]);
    tangent_length[c]=length(T);normal_length[c]=length(N);
    if(!finite(T) || !finite(N) || !std::isfinite(tangent_length[c]) || !std::isfinite(normal_length[c]) ||
       tangent_length[c]<1e-10f || normal_length[c]<1e-10f || (t[3]!=-1 && t[3]!=1))
      throw std::invalid_argument("Native shape base frame is singular");
    T=T/tangent_length[c];N=N/normal_length[c];
    result.base.push_back({T.x,T.y,T.z,t[3]*sign(linear[v])});result.normals.push_back(N);
  }
  for(size_t c=0;c<count;c+=3)if(result.base[c][3]!=result.base[c+1][3] || result.base[c][3]!=result.base[c+2][3])
    throw std::invalid_argument("Native shape changes tangent handedness within one triangle");
  for(size_t target=0;target<targets;++target) {
    matrices(&source.morph_targets[target].positions);auto &dt=result.targets[target],&dn=result.normal_targets[target];dt.reserve(count);dn.reserve(count);
    const auto &sourceT=original.targets[target],&sourceN=original.normal_targets[target];
    if((!sourceT.empty() && sourceT.size()!=count) || (!sourceN.empty() && sourceN.size()!=count))
      throw std::invalid_argument("Source shape target direction identity differs");
    for(size_t c=0;c<count;++c) {
      if((c&1023)==0)stop();
      auto v=source.mesh.indices[c];auto t=original.base[c];
      Vec3 T{t[0],t[1],t[2]},N=original.normals[c];if(!sourceT.empty())T+=sourceT[c];if(!sourceN.empty())N+=sourceN[c];
      // A common BASE length for every target preserves authored parallel delta
      // magnitudes and simultaneous-morph semantics under uniform transforms.
      T=source_frame_direction(linear[v],T)/tangent_length[c];N=source_frame_direction(normal[v],N)/normal_length[c];
      const float target_tangent_length=length(T),target_normal_length=length(N);
      if(!finite(T) || !finite(N) || !std::isfinite(target_tangent_length) || !std::isfinite(target_normal_length) ||
         target_tangent_length<1e-10f || target_normal_length<1e-10f || t[3]*sign(linear[v])!=result.base[c][3])
        throw std::invalid_argument("Native shape target frame is singular or changes unrepresentable tangent W");
      auto base=result.base[c];dt.push_back(T-Vec3{base[0],base[1],base[2]});dn.push_back(N-result.normals[c]);
    }
  }
  stop();return result;
}
} // namespace dome::detail
