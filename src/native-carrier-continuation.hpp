// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native-head-geometry.hpp"
#include "geometry-weight-authoring.hpp"
#include "native-localized-assembly.hpp"
namespace dome::detail {
// Private, pre-solve geometry policy. A maximal primary axial/limb support
// surface, extended through exact co-moving boundaries, supplies its NEW native
// field to subordinate surfaces. It never supplies original artist values or
// topology/seam authority, and is never selected in response to fit/error.
struct NativeCarrierPlan {std::vector<uint64_t> carrier,targets;size_t work=0;};
inline NativeCarrierPlan native_carrier_plan(const Scene&scene,
 const NativeHeadGeometryReport&analysis,const DeclaredWeightSupport&support,
 const Skeleton&rig,const HumanoidMapping&roles,weight_surface::Budget&work){
 const auto start=work.used;NativeCarrierPlan out;
 for(const auto&p:analysis.parts){
   const auto*decl=support.find(p.object);if(!decl)throw std::invalid_argument("Anatomical assembly source missing");
   bool subordinate=false;
   if(!decl->user_singleton_boundary&&one_anatomical_articulation_star(p.geometric_controls,rig,roles,work)){
     const auto&o=scene.object(p.object);
     auto contact=localized_assembly_contacts(o.painted->mesh(),decl->geometric_row_groups,
        o.deformation&&o.deformation->morphs?&*o.deformation->morphs:nullptr,
        128u*1024u*1024u,work);
     subordinate=contact.point_groups>0;
   }
   (subordinate?out.targets:out.carrier).push_back(p.object);
 }
 if(out.carrier.empty())throw std::invalid_argument("Localized assembly has no independent anatomical support surface");
 out.work=work.used-start;return out;
}
// Prospective subdivision of the unchanged500M whole-method budget. It
// includes complete canonical joint replay and copy/validation/packing work;
// the native BVH/query reserve is separate. No post-failure budget retry.
inline size_t native_carrier_work_plan(const Scene&scene,const NativeCarrierPlan&plan,
 uint64_t reference,weight_surface::Budget&planning){
 if(plan.targets.empty())return 0;
 const auto &rig=*scene.object(reference).deformation->skeleton;
 const size_t bones=rig.bones.size();size_t names=0;
 for(const auto&b:rig.bones){planning.step();names+=b.name.size();}
 std::function<bool()> no=[] {return false;};weight_surface::Budget forecast{0,500'000'000,no};
 forecast.step(20'000'000+65536); // bounded native BVH construction+queries
 auto owner=[&](uint64_t id){planning.step(16);const auto&o=scene.object(id);const auto&m=o.painted->mesh();
   if(m.positions.size()>2'000'000||m.indices.size()>6'000'000)throw std::length_error("Anatomical work-plan geometry");
   forecast.step_items(m.positions.size(),1024);forecast.step_items(m.indices.size(),64);
   forecast.step_items(2*bones+1,8192);forecast.step_items(names+1,2);
 };
 for(auto id:plan.carrier)owner(id);for(auto id:plan.targets)owner(id);
 for(auto id:plan.targets){const auto&o=scene.object(id);if(o.deformation&&o.deformation->morphs)
   for(const auto&t:o.deformation->morphs->targets){planning.step();forecast.step_items(t.delta.size()+1,32);}}
 return forecast.used;
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
