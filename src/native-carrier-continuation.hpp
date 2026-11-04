// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native-head-geometry.hpp"
#include "geometry-weight-authoring.hpp"
namespace dome::detail {
// Private, pre-solve geometry policy. A maximal primary axial/limb support
// surface, extended through exact co-moving boundaries, supplies its NEW native
// field to subordinate surfaces. It never supplies original artist values or
// topology/seam authority, and is never selected in response to fit/error.
struct NativeCarrierPlan {std::vector<uint64_t> carrier,targets;size_t work=0;};
inline NativeCarrierPlan native_carrier_plan(const NativeHeadGeometryReport&analysis,
 const DeclaredWeightSupport&support,const HumanoidMapping&roles,
 weight_surface::Budget&work){
 const auto start=work.used;HeatJointMask primary{};
 for(auto name:{"hips","spine","chest","upperChest","neck","head","leftShoulder","rightShoulder","leftUpperArm","rightUpperArm","leftLowerArm","rightLowerArm","leftUpperLeg","rightUpperLeg","leftLowerLeg","rightLowerLeg"}){
   auto it=roles.joints.find(name);if(it!=roles.joints.end())primary[it->second/64]|=uint64_t(1)<<(it->second%64);
 }
 auto mask=[&](const auto&p){auto m=p.geometric_controls;for(size_t k=0;k<4;++k)m[k]&=primary[k];return m;};
 NativeCarrierPlan out;
 for(const auto&p:analysis.parts){
   const auto*decl=support.find(p.object);if(!decl||decl->user_singleton_boundary)continue;
   const auto m=mask(p);bool dominates=m!=HeatJointMask{};
   for(const auto&q:analysis.parts){work.step(8);const auto*d=support.find(q.object);if(!d||d->user_singleton_boundary)continue;const auto n=mask(q);for(size_t k=0;k<4;++k)dominates&=(n[k]&~m[k])==0;}
   if(dominates)out.carrier.push_back(p.object);
 }
 if(out.carrier.size()!=1)throw std::invalid_argument("No unique geometry-witnessed primary anatomical carrier");
 for(size_t pass=0;pass<analysis.parts.size();++pass){bool changed=false;for(const auto&e:analysis.contacts){work.step(16);for(auto ids:{std::array{e.first,e.second},std::array{e.second,e.first}}){if(std::find(out.carrier.begin(),out.carrier.end(),ids[0])!=out.carrier.end()&&std::find(out.carrier.begin(),out.carrier.end(),ids[1])==out.carrier.end()){out.carrier.push_back(ids[1]);changed=true;}}}if(!changed)break;}
 for(const auto&p:analysis.parts){work.step(8);const auto*d=support.find(p.object);if(d&&!d->user_singleton_boundary&&std::find(out.carrier.begin(),out.carrier.end(),p.object)==out.carrier.end())out.targets.push_back(p.object);}
 if(out.targets.empty())throw std::invalid_argument("Anatomical carrier has no subordinate target");
 out.work=work.used-start;return out;
}
// Research owning application on an already detached Scene. Callers must run
// base solver+this continuation in one outer staged transaction before publish.
// Existing bounded native SurfaceIndex transfer is used deliberately, not as a
// numerical/error fallback; maximum distance remains the standard0.1 world m.
inline size_t continue_native_carrier(Scene&scene,const NativeCarrierPlan&plan,
 uint64_t reference,size_t maximum_work,size_t maximum_bytes,
 const std::function<bool()>&cancelled={}){
 using namespace geometry_weight_authoring;
 Work work{0,maximum_work,cancelled};work.stop();
 const auto &ref=scene.object(reference);if(!ref.deformation||!ref.deformation->skeleton)throw std::invalid_argument("Carrier requires actual current native joint identities");
 size_t rows=0,corners=0;for(auto id:plan.carrier){const auto&o=scene.object(id);if(!o.painted||!o.deformation||!o.deformation->skin)throw std::invalid_argument("Unsolved anatomical carrier");rows+=o.painted->mesh().positions.size();corners+=o.painted->mesh().indices.size();}
 for(auto id:plan.targets){const auto&o=scene.object(id);if(!o.painted||!o.deformation||!o.deformation->skin)throw std::invalid_argument("Unsolved subordinate current rig");rows+=o.painted->mesh().positions.size();corners+=o.painted->mesh().indices.size();}
 const size_t bytes=rows*256+corners*64+65536;
 if(rows>2'000'000||corners>6'000'000||bytes>maximum_bytes)
    throw std::length_error("Anatomical continuation bytes before copies");
 auto shift=AffineTransform{}.matrix();const auto refworld=scene.world_matrix(reference);for(size_t k=0;k<3;++k)shift[12+k]=-refworld.matrix()[12+k];const AffineTransform origin(shift);
 Mesh surface,destination;SkinWeights weights;std::vector<size_t>counts;
 auto append=[&](Mesh&output,const Mesh&m){const auto off=uint32_t(output.positions.size());work.step_items(m.positions.size()+m.indices.size(),16);output.positions.insert(output.positions.end(),m.positions.begin(),m.positions.end());output.normals.insert(output.normals.end(),m.normals.begin(),m.normals.end());for(auto i:m.indices)output.indices.push_back(off+i);};
 auto metric=[&](uint64_t id){const auto&o=scene.object(id);SkinGeometryWork g(work.maximum-work.used,cancelled);const auto left=g.remaining();auto m=skin_world_mesh(o.painted->mesh(),origin*scene.world_matrix(id),g);work.step(left-g.remaining());return m;};
 for(auto id:plan.carrier){auto m=metric(id);append(surface,m);const auto&o=scene.object(id);for(const auto&r:o.deformation->skin->vertices){work.step(16+r.size()*8);weights.vertices.push_back(r);}}
 for(auto id:plan.targets){auto m=metric(id);counts.push_back(m.positions.size());append(destination,m);}
 // These are generated native index identities from the same captured rig;
 // this research route refuses an altered/foreign skeleton before transfer.
 size_t replay_peak=0;
 auto joint_authority=[&](uint64_t id){const auto&o=scene.object(id);const auto world=scene.world_matrix(id);
   if(!geometry_reference_rig_matches(*ref.deformation->skeleton,*o.deformation->skeleton,
           world.inverse()*refworld,world.matrix()==refworld.matrix(),work,maximum_bytes-bytes,&replay_peak))
     throw std::invalid_argument("Anatomical continuation current joint/bind authority differs");};
 for(auto id:plan.carrier)joint_authority(id);
 for(auto id:plan.targets)joint_authority(id);
 // Forecast actual recipient cloning/validation before the transfer consumes
 // its reserved budget. This uses the same complete deformation planner as
 // the owning base factory, not a scalar-count approximation.
 Bytes staging{bytes+replay_peak,maximum_bytes};
 for(auto id:plan.targets)deformation_copy_bytes(*scene.object(id).deformation,staging,work);
 work.step_items(rows,256);
 const size_t allowance=work.maximum-work.used;
 // The native API enforces its real construction/query budget internally.
 // Charge its whole reserved allowance conservatively, not an invented actual
 // hit count. No extra budget is gained by splitting targets into calls.
 auto result=transfer_weights(surface,weights,destination,ref.deformation->skeleton->bones.size(),
                         .1f,{4,1e-5f},cancelled,allowance);
 work.step(allowance);
 std::vector<std::pair<uint64_t,std::shared_ptr<const DeformationState>>>staged;
 size_t offset=0;for(size_t i=0;i<plan.targets.size();++i){auto id=plan.targets[i];auto d=std::make_shared<DeformationState>(*scene.object(id).deformation);d->skin->vertices.assign(result.vertices.begin()+offset,result.vertices.begin()+offset+counts[i]);offset+=counts[i];d->validate(scene.object(id).painted->mesh());staged.emplace_back(id,std::move(d));}
 if(cancelled&&cancelled())throw std::runtime_error("Anatomical continuation cancelled before adoption");
 for(auto&[id,d]:staged)scene.object(id).deformation=std::move(d);
 return staging.used;
}
} // namespace dome::detail
