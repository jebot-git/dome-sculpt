// SPDX-License-Identifier: GPL-3.0-or-later
#include "normal-bake-frames.hpp"
#include "normal-frame-runtime.hpp"
#include "dome/tangents.hpp"
#include <numeric>
namespace dome::detail {
namespace {
struct Budget {
  const NormalBakeFrameOptions &options;
  size_t work=0;
  void check(size_t amount=1) {
    if(amount>options.max_work-work)throw std::length_error("Normal authoring work exceeds its declared budget");
    work+=amount;
    if(options.cancelled&&options.cancelled())throw std::runtime_error("Normal authoring cancelled");
  }
};
// Preserve the exact N authority, including non-unit morph-key N. Mikk itself
// normalizes its inputs; the exported normal deltas retain the raw values.
std::vector<Tangent> generate(const Mesh &mesh,const NormalFrames &frame,
    const std::vector<Vec3> *position_delta,const NormalFrameTarget *key,Budget &budget) {
  budget.check(mesh.indices.size());
  Mesh corners;const size_t count=mesh.indices.size();
  corners.positions.reserve(count);corners.normals.reserve(count);corners.uv.reserve(count);
  corners.indices.resize(count);std::iota(corners.indices.begin(),corners.indices.end(),uint32_t(0));
  for(size_t c=0;c<count;++c) {
    if((c&1023)==0)budget.check();
    const auto vertex=mesh.indices[c];
    auto p=mesh.positions[vertex],n=frame.normals[c];
    if(position_delta)p+=position_delta->at(vertex);
    if(key&&!key->normals.empty())n+=key->normals[c];
    corners.positions.push_back(p);corners.normals.push_back(n);corners.uv.push_back(mesh.uv[vertex]);
  }
  return tangent_corners(corners,budget.options.cancelled);
}
}
std::vector<Vec3> normal_bake_target_origin_positions(const Mesh &current,
    const Mesh &origin,std::span<const Vec3> deltas,const NormalBakeFrameOptions &options) {
  if(!options.max_work||options.max_work>256*1024*1024||!options.max_bytes||options.max_bytes>128*1024*1024)
    throw std::invalid_argument("Invalid normal target origin resource policy");
  Budget budget{options};budget.check();current.validate();origin.validate();
  if(current.indices.size()!=origin.indices.size()||deltas.size()!=current.positions.size())
    throw std::invalid_argument("Normal target origin requires exact ordered corner correspondence");
  if(origin.positions.size()>2'000'000||origin.positions.size()>options.max_bytes/(sizeof(Vec3)+sizeof(uint8_t)))
    throw std::length_error("Normal target origin exceeds its memory budget");
  budget.check(current.indices.size());
  auto equal=[](Vec3 a,Vec3 b){return a.x==b.x&&a.y==b.y&&a.z==b.z;};
  std::vector<Vec3> result(origin.positions.size());std::vector<uint8_t> seen(result.size());
  for(size_t c=0;c<current.indices.size();++c){if((c&1023)==0)budget.check();
    const auto a=current.indices[c],b=origin.indices[c];const auto delta=deltas[a];
    if(!equal(current.positions[a],origin.positions[b])||!finite(delta))
      throw std::invalid_argument("Normal target origin POSITION authority changed or is nonfinite");
    if(seen[b]&&!equal(result[b],delta))
      throw std::invalid_argument("Split current vertices disagree on captured native target POSITION");
    result[b]=delta;seen[b]=1;
  }
  if(std::find(seen.begin(),seen.end(),uint8_t(0))!=seen.end())
    throw std::invalid_argument("Normal target origin has unreferenced POSITION authority; reauthor its full domain explicitly");
  budget.check();return result;
}
NativeNormalBakeTarget normal_bake_author_native_target(const Mesh &mesh,
    const NormalFrames &base,std::span<const Vec3> edited_positions,uint64_t id,
    const NormalBakeFrameOptions &options) {
  if(!id||!options.max_work||options.max_work>256*1024*1024||!options.max_bytes||options.max_bytes>128*1024*1024)
    throw std::invalid_argument("Invalid native normal-key authoring policy");
  Budget budget{options};budget.check();mesh.validate();base.validate();
  if(edited_positions.size()!=mesh.positions.size()||base.normals.size()!=mesh.indices.size()||base.tangents.empty())
    throw std::invalid_argument("Native normal-key authoring requires exact current corners and a complete base frame");
  const size_t corners=mesh.indices.size(),vertices=mesh.positions.size();
  if(corners>2'000'000||vertices>2'000'000||
     corners>options.max_bytes/(6*sizeof(Vec3)+sizeof(Tangent)+sizeof(uint32_t)+sizeof(std::array<float,2>))||
     vertices>(options.max_bytes-corners*(6*sizeof(Vec3)+sizeof(Tangent)+sizeof(uint32_t)+sizeof(std::array<float,2>)))/(3*sizeof(Vec3)))
    throw std::length_error("Native normal-key authoring exceeds its detached memory budget");
  budget.check(corners+vertices);
  MorphSet temporary(mesh);temporary.set_target(mesh,"explicit-native-key",edited_positions);
  auto key_mesh=temporary.evaluate(mesh,std::array<float,1>{1});
  NormalFrames key_base;key_base.normals.reserve(corners);
  for(auto v:mesh.indices)key_base.normals.push_back(key_mesh.normals[v]);
  auto tangent=generate(key_mesh,key_base,nullptr,nullptr,budget);
  NativeNormalBakeTarget out;out.positions=std::move(temporary.targets[0].delta);out.attributes.id=id;
  out.attributes.normals.reserve(corners);out.attributes.tangents.reserve(corners);
  for(size_t c=0;c<corners;++c){if((c&1023)==0)budget.check();
    if(tangent[c][3]!=base.tangents[c][3])throw std::invalid_argument("Native key changes immutable tangent handedness; split or reauthor the basis");
    out.attributes.normals.push_back(key_base.normals[c]-base.normals[c]);
    out.attributes.tangents.push_back({tangent[c][0]-base.tangents[c][0],tangent[c][1]-base.tangents[c][1],tangent[c][2]-base.tangents[c][2]});
  }
  budget.check();return out;
}
NormalBakeFrameCandidate normal_bake_frame_candidate(const Mesh &mesh,
    const DeformationState *deform,const SourceMaterialBindings *binding,
    const NormalBakeFrameOptions &options) {
  if(!options.max_work||options.max_work>256*1024*1024||!options.max_bytes||options.max_bytes>128*1024*1024)
    throw std::invalid_argument("Invalid normal authoring resource policy");
  Budget budget{options};budget.check();mesh.validate();
  if(mesh.uv.size()!=mesh.positions.size()||mesh.indices.empty()||
     mesh.indices.size()>2'000'000||mesh.positions.size()>2'000'000)
    throw std::length_error("Normal authoring requires a bounded UV painting mesh");
  if(deform)deform->validate(mesh);
  const auto *morphs=deform&&deform->morphs?&*deform->morphs:nullptr;
  const size_t target_count=morphs?morphs->targets.size():0;
  const size_t all_targets=binding&&binding->normal_frames?binding->normal_frames->targets.size():target_count;
  if(target_count>128||all_targets>128)throw std::length_error("Normal authoring target count exceeds its declared budget");
  // Account for retained per-corner N/T/deltas and peak detached corner mesh
  // before allocating. Existing retained complete frames are not duplicated.
  const size_t count=mesh.indices.size();
  const size_t bytes_per_corner=3*sizeof(uint32_t)+sizeof(Vec3)+sizeof(Tangent)+
      all_targets*2*sizeof(Vec3)+2*(2*sizeof(Vec3)+sizeof(std::array<float,2>)+sizeof(uint32_t)+sizeof(Tangent));
  const size_t bytes_per_vertex=4*sizeof(Vec3)+2*sizeof(std::array<float,2>)+sizeof(uint8_t);
  if(count>options.max_bytes/bytes_per_corner||
     mesh.positions.size()>(options.max_bytes-count*bytes_per_corner)/bytes_per_vertex)
    throw std::length_error("Normal authoring candidate exceeds its declared memory budget");
  budget.check(count*(all_targets+1));
  NormalBakeFrameCandidate result;std::shared_ptr<NormalFrameSet> set;
  std::shared_ptr<NormalFrames> frame;
  if(binding&&binding->normal_frames) {
    normal_frame_current_authority(*binding,mesh,deform,options.cancelled);
    if(binding->normal_frames->storage_bytes()>options.max_bytes)
      throw std::length_error("Retained normal authority exceeds candidate memory budget");
    const auto selected=binding->normal_frames->pbr;
    if(!selected)throw std::invalid_argument("Choose an explicit PBR normal authority before normal preparation");
    set=std::make_shared<NormalFrameSet>(*binding->normal_frames);
    if(!selected->tangents.empty()) {
      // A target basis may be shared only as an explicit conversion. The caller
      // retains the original MToon role and must qualify its texture conversion.
      result.shared_target_attributes=set->mtoon!=selected;
      set->mtoon=selected;set->canonical.reset();set->validate();result.frames=std::move(set);return result;
    }
    frame=std::make_shared<NormalFrames>(*selected);
  } else {
    set=std::make_shared<NormalFrameSet>();frame=std::make_shared<NormalFrames>();
    frame->origin_vertex_count=uint32_t(mesh.positions.size());frame->origin_vertices=mesh.indices;
    frame->normals.reserve(count);for(auto v:mesh.indices)frame->normals.push_back(mesh.normals[v]);
    // This is new native key authoring, not an assertion of artist/raw source
    // tangents. Imported sources must already have retained source authority.
    if(binding&&binding->uv_origin&&binding->uv_origin->appearance)
      throw std::invalid_argument("Imported normal authoring requires retained original attribute identities");
    for(size_t slot=0;slot<target_count;++slot) {
      budget.check(count);std::vector<float> weights(target_count);weights[slot]=1;
      auto key_mesh=morphs->evaluate(mesh,weights);
      NormalFrameTarget key;key.id=slot+1;key.normals.reserve(count);
      for(size_t c=0;c<count;++c)key.normals.push_back(key_mesh.normals[mesh.indices[c]]-frame->normals[c]);
      frame->targets.push_back(std::move(key));
      set->targets.push_back({slot+1,uint32_t(slot),{}, {}});set->current_targets.push_back(slot+1);
    }
    set->next_target=target_count+1;result.authored_native_normals=true;
  }
  frame->tangents=generate(mesh,*frame,nullptr,nullptr,budget);
  // Generation uses the CURRENT native UV0, explicitly newly authored. Raw
  // source IDs and N/target-N authority are copied unchanged from retained N.
  frame->generated_tangents=true;frame->generated_uv=0;
  for(size_t key_index=0;key_index<frame->targets.size();++key_index) {
    auto &key=frame->targets[key_index];const auto &identity=set->targets[key_index];
    std::vector<Vec3> positions;
    auto active=std::find(set->current_targets.begin(),set->current_targets.end(),identity.id);
    if(active!=set->current_targets.end())positions=morphs->targets.at(size_t(active-set->current_targets.begin())).delta;
    else {
      if(!binding||!binding->uv_origin||!binding->uv_origin->paint)
        throw std::invalid_argument("Inactive normal target has no retained native POSITION authority");
      const auto &origin=binding->uv_origin->paint->mesh();
      const std::vector<Vec3> *authority=&identity.positions;
      if(authority->empty()) {
        if(!identity.origin_slot||!binding->uv_origin->deformation||!binding->uv_origin->deformation->morphs)
          throw std::invalid_argument("Inactive normal target has no captured native key");
        authority=&binding->uv_origin->deformation->morphs->targets.at(*identity.origin_slot).delta;
      }
      positions.resize(mesh.positions.size());std::vector<uint8_t> seen(positions.size());
      for(size_t c=0;c<count;++c){if((c&1023)==0)budget.check();auto v=mesh.indices[c];auto value=authority->at(origin.indices[c]);
        if(seen[v]&&(positions[v].x!=value.x||positions[v].y!=value.y||positions[v].z!=value.z))
          throw std::invalid_argument("Inactive native key has ambiguous current vertex identity");
        positions[v]=value;seen[v]=1;
      }
    }
    auto tangents=generate(mesh,*frame,&positions,&key,budget);key.tangents.reserve(count);
    for(size_t c=0;c<count;++c){if((c&1023)==0)budget.check();const auto a=tangents[c],b=frame->tangents[c];
      if(a[3]!=b[3])throw std::invalid_argument("Normal authoring target changes tangent handedness; split or reauthor the target");
      key.tangents.push_back({a[0]-b[0],a[1]-b[1],a[2]-b[2]});
    }
  }
  result.authored_native_tangents=true;result.shared_target_attributes=bool(set->mtoon);
  set->pbr=frame;set->mtoon=frame;set->canonical.reset();set->validate();
  if(set->storage_bytes()>options.max_bytes)throw std::length_error("Normal authoring retained payload exceeds its memory budget");
  budget.check();result.frames=std::move(set);return result;
}
} // namespace dome::detail
