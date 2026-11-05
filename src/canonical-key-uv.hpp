// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "normal-key-derivation.hpp"
#include "normal-frame-runtime.hpp"
#include "source-paint-pairing.hpp"
namespace dome::detail {
// Worker-only proof of the CURRENT derived key domain. The independent root
// conversion/field owns only sampling; its initial frames cannot prove new keys.
// No result is cached or serialized. This worker proof independently replays
// the complete current material/key graph before returning.
inline uint64_t canonical_key_uv_authority(const PaintedMesh &paint,
    const SourceMaterialBindings &binding,const DeformationState *deformation,
    uint64_t max_work,size_t max_working_bytes,const std::function<bool()> &cancelled={}) {
 const auto &origin=binding.canonical_attributes;const auto &key=binding.key_derivation;
 if(!origin||!origin->valid_owner(origin)||!key||!key->valid_owner(key)||
    !key->before_material()||!key->position_domain()||!binding.normal_frames||
    binding.normal_frame!=SourceMaterialNormalFrame::OriginalAttributes)
  throw std::invalid_argument("Canonical key UV needs its genuine current key/frame domain");
 auto own=[](const auto&a,const auto&b){return a==b&&!a.owner_before(b)&&!b.owner_before(a);};
 if(!own(key->frames(),binding.normal_frames)||!own(key->position_domain(),binding.normal_frames->position_domain)||
    !own(key->before_material()->canonical_attributes,origin)||
    !own(key->before_material()->uv_origin,binding.uv_origin))
  throw std::invalid_argument("Canonical key UV changed its actual current frame/canonical owners");
 const auto &mesh=paint.mesh();const auto corners=mesh.indices.size();const auto targets=deformation&&deformation->morphs?deformation->morphs->targets.size():0;
 if(!corners||targets>128||!max_work||!max_working_bytes)
  throw std::length_error("Canonical key UV has no bounded current-field proof budget");
 uint64_t work=0;size_t scratch=0;
 auto visit=[&](size_t n,size_t stride=1){if(stride&&n>(max_work-work)/stride)throw std::length_error("Canonical key UV current-field proof exceeds work before staging");work+=uint64_t(n)*stride;};
 auto charge=[&](size_t n,size_t stride=1){if(stride&&n>(max_working_bytes-scratch)/stride)throw std::length_error("Canonical key UV current-field metadata exceeds scratch before allocation");scratch+=n*stride;};
 // Include source validation/identity scans and every actual endpoint comparison.
 // Existing deep normal/key replay has its own bounded kernel policies; this
 // ledger charges the additional current-geometry/model proof in this route.
 visit(mesh.positions.size(),8);visit(corners,4);visit(corners,targets+1);
 charge(1,4096);charge(targets,128);
 if(deformation){
  visit(deformation->resource_entries());
  if(deformation->skeleton){const auto &bones=deformation->skeleton->bones;if(bones.size()>256)throw std::length_error("Canonical key UV skeleton exceeds native bound");visit(bones.size(),bones.size()+64);charge(bones.size(),2*sizeof(RigMatrix)+128);for(const auto&bone:bones){charge(bone.name.size()+1);visit(bone.name.size());}}
  // Skin validation owns at most one <=32-entry joint set at a time.
  if(deformation->skin){size_t largest=0;for(const auto &row:deformation->skin->vertices){if(row.size()>32)throw std::length_error("Canonical key UV skin row exceeds native bound");largest=std::max(largest,row.size());}charge(largest,64);}
  if(deformation->morphs)for(const auto &target:deformation->morphs->targets){charge(1,128);charge(target.name.size()+1);visit(target.name.size());}
  if(deformation->humanoid)for(const auto &[name,joint]:deformation->humanoid->joints){charge(1,256);charge(name.size()+1);visit(name.size()+1);}
  if(deformation->landmarks)for(const auto &[name,point]:deformation->landmarks->points){charge(1,768);charge(name.size()+1,4);visit(name.size()+1);}
  if(deformation->expressions)for(const auto &[name,rows]:deformation->expressions->visemes){charge(1,256);charge(name.size()+1);visit(name.size()+1);for(const auto &row:rows){charge(1,256);charge(row.shape.size()+1);visit(row.shape.size()+1);}}
 }
 if(cancelled&&cancelled())throw std::runtime_error("Canonical key UV cancelled");
 binding.validate(true);source_paint_pairing(&paint,&binding,nullptr,true);
 normal_frame_current_authority(binding,paint.mesh(),deformation,cancelled);
 return work;
}
} // namespace dome::detail
