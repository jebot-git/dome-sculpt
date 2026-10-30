// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/material-bindings.hpp"
#include "dome/deformation.hpp"
#include "dome/paint.hpp"
#include "source-frame-skin.hpp"
#include <bit>
#include <unordered_map>
namespace dome::detail {
// Remap only native authoring arrays, with aggregate dimensions checked before
// allocation. UV-only edits with identical vertex topology share the exact const
// state; pose/skin/morph data never roundtrip through evaluated geometry.
inline std::shared_ptr<const DeformationState> normal_frame_remap_deformation(
    const std::shared_ptr<const DeformationState> &input,const Mesh &source,const Mesh &target,
    std::span<const uint32_t> vertices,const std::function<bool()> &cancelled={}) {
  if(!input)return {};
  input->validate(source);
  if(vertices.size()!=target.positions.size() || vertices.size()>2'000'000)throw std::invalid_argument("Deformation remap dimensions differ");
  size_t entries=input->pose.size()+input->morph_weights.size();constexpr size_t limit=8'000'000;
  if(input->morphs && vertices.size() && input->morphs->targets.size()>(limit-entries)/vertices.size())throw std::length_error("Remapped morph POSITION arrays exceed native deformation quota");
  if(input->morphs)entries+=input->morphs->targets.size()*vertices.size();
  bool identity=vertices.size()==source.positions.size();
  for(size_t n=0;n<vertices.size();++n){if((n&1023)==0 && cancelled && cancelled())throw std::runtime_error("Deformation remap cancelled");auto v=vertices[n];if(v>=source.positions.size())throw std::invalid_argument("Deformation remap vertex is outside its source");if(target.positions[n].x!=source.positions[v].x || target.positions[n].y!=source.positions[v].y || target.positions[n].z!=source.positions[v].z)throw std::invalid_argument("Deformation remap changed rest POSITION values");identity&=v==n;if(input->skin){auto count=input->skin->vertices[v].size();if(count>limit-entries)throw std::length_error("Remapped skin and morph arrays exceed native deformation quota");entries+=count;}}
  if(identity && target.indices==source.indices)return input;
  auto out=std::make_shared<DeformationState>(target);out->skeleton=input->skeleton;out->humanoid=input->humanoid;out->landmarks=input->landmarks;out->expressions=input->expressions;out->pose=input->pose;out->morph_weights=input->morph_weights;
  if(input->skin){out->skin=SkinWeights{};out->skin->vertices.reserve(vertices.size());for(size_t n=0;n<vertices.size();++n){if((n&1023)==0 && cancelled && cancelled())throw std::runtime_error("Deformation remap cancelled");out->skin->vertices.push_back(input->skin->vertices[vertices[n]]);}}
  if(input->morphs){out->morphs=MorphSet(target);for(const auto &old:input->morphs->targets){MorphTarget t;t.name=old.name;t.delta.reserve(vertices.size());for(size_t n=0;n<vertices.size();++n){if((n&1023)==0 && cancelled && cancelled())throw std::runtime_error("Deformation remap cancelled");t.delta.push_back(old.delta[vertices[n]]);}out->morphs->targets.push_back(std::move(t));}}
  out->validate(target);return out;
}
struct NormalFrameLayout {
  // Exact current-paint vertex IDs and representative aligned frame corners.
  std::vector<uint32_t> vertices,corners,indices;
};
inline void normal_frame_current_authority(const SourceMaterialBindings &binding,
    const Mesh &current,const DeformationState *deformation,const std::function<bool()> &cancelled={}) {
  binding.validate(false);
  if(!binding.normal_frames || !binding.uv_origin || !binding.uv_origin->paint)
    throw std::invalid_argument("Original normal transport requires retained native frame authority");
  auto equal=[](Vec3 a,Vec3 b){return a.x==b.x && a.y==b.y && a.z==b.z;};
  const auto &set=*binding.normal_frames;const auto &origin=binding.uv_origin->paint->mesh();
  if(current.indices.size()!=origin.indices.size())throw std::invalid_argument("Normal frame corner identity changed: reauthor frames before preview/export");
  if(deformation)deformation->validate(current);
  const size_t active=deformation && deformation->morphs?deformation->morphs->targets.size():0;
  if(active!=set.current_targets.size())throw std::invalid_argument("Native morph slot mapping changed: update explicit frame target identities");
  if(active+1>128*1024*1024/std::max(size_t(1),current.indices.size()))throw std::length_error("Normal frame authority work exceeds native budget");
  std::unordered_map<uint64_t,const FrameTargetIdentity*> identities;
  for(const auto &t:set.targets)identities.emplace(t.id,&t);
  for(size_t c=0;c<current.indices.size();++c) {
    if((c&1023)==0 && cancelled && cancelled())throw std::runtime_error("Normal frame authority check cancelled");
    auto a=current.indices[c],b=origin.indices[c];
    if(a>=current.positions.size() || b>=origin.positions.size() ||
       !equal(current.positions[a],origin.positions[b]))
      throw std::invalid_argument("Native rest POSITION authority changed: explicitly reauthor or reproject normal frames");
    for(size_t slot=0;slot<active;++slot) {
      const auto &identity=*identities.at(set.current_targets[slot]);
      const auto &authority=identity.positions.empty()?binding.uv_origin->deformation->morphs->targets.at(*identity.origin_slot).delta:identity.positions;
      if(!equal(deformation->morphs->targets[slot].delta.at(a),authority.at(b)))
        throw std::invalid_argument("Native morph POSITION authority changed: explicitly reauthor frame target and mapping");
    }
  }
}
// Every base and target channel of every role participates in splitting. PBR,
// MToon and canonical normals need not alias simply because their base T does.
inline NormalFrameLayout normal_frame_layout(const Mesh &mesh,const NormalFrameSet &set,
    const std::function<bool()> &cancelled={},size_t max_work=128*1024*1024) {
  set.validate(false);mesh.validate();
  if(!max_work || max_work>256*1024*1024 || mesh.positions.size()>2'000'000 || mesh.indices.size()>6'000'000)
    throw std::length_error("Normal frame layout resource policy exceeded");
  size_t work=0;auto visit=[&]{if(++work>max_work)throw std::length_error("Normal frame layout work exceeds budget");if((work&1023)==0 && cancelled && cancelled())throw std::runtime_error("Normal frame layout cancelled");};
  auto bits=[&](float value){visit();return std::bit_cast<uint32_t>(value);};
  const std::array<std::shared_ptr<const NormalFrames>,3> roles{set.pbr,set.mtoon,set.canonical};
  for(const auto &f:roles)if(f && f->normals.size()!=mesh.indices.size())throw std::invalid_argument("Normal frame current corner count differs");
  NormalFrameLayout result;result.indices.resize(mesh.indices.size());const size_t reserve=std::min(mesh.indices.size(),size_t(2'000'000));
  result.vertices.reserve(reserve);result.corners.reserve(reserve);std::vector<uint32_t> next;next.reserve(reserve);
  std::unordered_map<uint64_t,uint32_t> heads;heads.reserve(std::min(reserve,mesh.positions.size()));constexpr uint32_t none=UINT32_MAX;
  auto each=[&](size_t c,auto scalar,auto integer) {
    integer(mesh.indices[c]);
    for(const auto &f:roles)if(f) {
      integer(f->origin_vertices[c]);
      auto vector=[&](Vec3 v){scalar(v.x);scalar(v.y);scalar(v.z);};
      vector(f->normals[c]);if(!f->tangents.empty())for(auto x:f->tangents[c])scalar(x);
      for(const auto &t:f->targets){if(!t.normals.empty())vector(t.normals[c]);if(!t.tangents.empty())vector(t.tangents[c]);}
    }
  };
  for(size_t c=0;c<mesh.indices.size();++c) {
    uint64_t hash=1469598103934665603ull;each(c,[&](float v){hash=(hash^bits(v))*1099511628211ull;},[&](uint32_t v){visit();hash=(hash^v)*1099511628211ull;});
    auto bucket=heads.try_emplace(hash,none).first;uint32_t v=bucket->second;
    auto equal=[&](size_t a,size_t b) {
      if(mesh.indices[a]!=mesh.indices[b])return false;
      auto veq=[&](const std::vector<Vec3> &values){if(values.empty())return true;const auto x=values[a],y=values[b];return bits(x.x)==bits(y.x)&&bits(x.y)==bits(y.y)&&bits(x.z)==bits(y.z);};
      for(const auto &f:roles)if(f){if(f->origin_vertices[a]!=f->origin_vertices[b] || !veq(f->normals))return false;if(!f->tangents.empty())for(size_t k=0;k<4;++k)if(bits(f->tangents[a][k])!=bits(f->tangents[b][k]))return false;for(const auto &t:f->targets)if(!veq(t.normals)||!veq(t.tangents))return false;}
      return true;
    };
    while(v!=none && !equal(c,result.corners[v])){visit();v=next[v];}
    if(v==none){if(result.vertices.size()>=2'000'000)throw std::length_error("Normal frame split exceeds native renderer vertex limit");v=uint32_t(result.vertices.size());result.vertices.push_back(mesh.indices[c]);result.corners.push_back(uint32_t(c));next.push_back(bucket->second);bucket->second=v;}
    result.indices[c]=v;
  }
  if(cancelled && cancelled())throw std::runtime_error("Normal frame layout cancelled");
  return result;
}
struct PosedNormalFrames {
  std::array<std::vector<Vec3>,3> normals;
  std::array<std::vector<std::array<float,4>>,3> tangents;
};
// Explicit Dome weighted-joint policy: raw local N/T morph addition, then N by
// sum(weight*joint^-T), T by sum(weight*joint). Material shader version owns
// normalization/interpolation/B reconstruction. This is not inverse(sum(joint)).
inline PosedNormalFrames normal_frame_preview(const NormalFrameSet &set,const NormalFrameLayout &layout,
    const DeformationState *deformation,const std::function<bool()> &cancelled={},unsigned roles_mask=7) {
  if(roles_mask>7)throw std::invalid_argument("Invalid requested normal frame role mask");
  set.validate(false);std::unordered_map<uint64_t,float> weights;
  for(size_t slot=0;slot<set.current_targets.size();++slot)weights[set.current_targets[slot]]=deformation && !deformation->morph_weights.empty()?deformation->morph_weights.at(slot):0;
  std::vector<RigMatrix> linear,normal;
  if(deformation && deformation->skeleton && deformation->skin && !deformation->pose.empty()) {
    linear=deformation->skeleton->skin_matrices(deformation->pose);normal.reserve(linear.size());for(const auto &m:linear)normal.push_back(source_frame_inverse_transpose(m));
  }
  PosedNormalFrames out;const std::array<std::shared_ptr<const NormalFrames>,3> roles{set.pbr,set.mtoon,set.canonical};
  for(size_t r=0;r<roles.size();++r)if(const auto &f=roles[r];f && (roles_mask&(1u<<r))) {
    std::vector<std::pair<const NormalFrameTarget*,float>> active;
    for(const auto &target:f->targets){auto found=weights.find(target.id);if(found!=weights.end() && found->second!=0)active.emplace_back(&target,found->second);}
    out.normals[r].reserve(layout.vertices.size());if(!f->tangents.empty())out.tangents[r].reserve(layout.vertices.size());
    for(size_t v=0;v<layout.vertices.size();++v) {
      if((v&1023)==0 && cancelled && cancelled())throw std::runtime_error("Normal frame preview cancelled");
      auto c=layout.corners.at(v);Vec3 N=f->normals.at(c),T{};float sign=1;
      if(!f->tangents.empty()){const auto t=f->tangents.at(c);T={t[0],t[1],t[2]};sign=t[3];}
      for(auto [target,w]:active){if(!target->normals.empty())N+=target->normals[c]*w;if(!target->tangents.empty())T+=target->tangents[c]*w;}
      if(!linear.empty()) {
        // N-only transport must never manufacture an authoritative tangent.
        const auto &influences=deformation->skin->vertices.at(layout.vertices[v]);
        if(f->tangents.empty()){Vec3 posed{};for(auto i:influences)posed+=source_frame_direction(normal.at(i.joint),N)*i.weight;N=posed;}
        else {auto posed=source_skin_frame({T,N,sign},influences,linear,normal);N=posed.normal;T=posed.tangent;}
      }
      const float n=length(N),t=length(T);
      if(!finite(N)||!std::isfinite(n)||n<1e-10f || (!f->tangents.empty()&&(!finite(T)||!std::isfinite(t)||t<1e-10f)))throw std::invalid_argument("Posed retained normal frame is singular");
      out.normals[r].push_back(N);if(!f->tangents.empty())out.tangents[r].push_back({T.x,T.y,T.z,sign});
    }
  }
  return out;
}
} // namespace dome::detail
