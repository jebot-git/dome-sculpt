// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "humanoid-heat-domains.hpp"
#include "weight-boundary-sewing.hpp"
#include "closed-mesh-source-domain.hpp"
namespace dome::detail {
// Current geometry/semantic-role boundary construction. No raw asset, material
// names, artist influences, cross-owner sewing, or posed coordinates are inputs.
// A rigid region is proposed by unique nearest geometric witnesses. Ocular
// ownership additionally requires an entire intrinsic convex projected disk.
inline HumanoidHeatDomains current_humanoid_heat_domains(const Scene&scene,
 std::span<const uint64_t>selected,uint64_t reference,weight_surface::Budget&work,
 size_t maximum_bytes){
 if(selected.empty()||selected.size()>64||maximum_bytes<65536||maximum_bytes>256u*1024u*1024u)throw std::invalid_argument("Current humanoid boundary dimensions/budget");
 const auto&ref=scene.object(reference);if(!ref.deformation||!ref.deformation->skeleton||!ref.deformation->humanoid)throw std::invalid_argument("Current humanoid boundary needs actual native rig/roles");
 const auto&rig=*ref.deformation->skeleton;const auto&roles=ref.deformation->humanoid->joints;ref.deformation->humanoid->validate(rig,false);
 if(!roles.contains("head")||!roles.contains("leftEye")||!roles.contains("rightEye"))throw std::invalid_argument("Current humanoid boundary lacks head/eye roles");
 const auto head=roles.at("head");if(!rig.bones.at(head).deform)throw std::invalid_argument("Current head boundary cannot emit a nondeform control");
 HeatJointMask branch{},eyes{};for(auto role:{"neck","head","leftEye","rightEye"})if(roles.contains(role)){auto j=roles.at(role);branch[j/64]|=uint64_t(1)<<(j%64);}for(auto role:{"leftEye","rightEye"}){auto j=roles.at(role);eyes[j/64]|=uint64_t(1)<<(j%64);}
 // Retained fixed data and the largest simultaneously live analysis workspace
 // are admitted before copying meshes, morph rows, tree keys or boundary data.
 visibility_geodesic::Bytes retained{65536,maximum_bytes};retained.add(selected.size(),sizeof(HumanoidHeatDomains::Part)+128);
 size_t peak=0;for(auto id:selected){const auto&o=scene.object(id);if(!o.mesh_active||!o.painted||o.appearance||!o.deformation||!o.deformation->skeleton||!o.deformation->humanoid||o.deformation->humanoid->joints!=roles)throw std::invalid_argument("Current humanoid boundary requires captured native mesh/role correspondence");const auto&m=o.painted->mesh();const size_t n=m.positions.size(),c=m.indices.size(),targets=o.deformation->morphs?o.deformation->morphs->targets.size():0;if(!n||n>2'000'000||c>6'000'000||targets>128)throw std::length_error("Current humanoid boundary geometry cap");retained.add(n,sizeof(uint32_t));visibility_geodesic::Bytes transient{65536,maximum_bytes};transient.add(n,768+24*targets);transient.add(c,640+48*targets);transient.add(rig.bones.size(),4096);peak=std::max(peak,transient.used);}
 if(peak>maximum_bytes-retained.used)throw std::length_error("Current humanoid boundary analysis workspace before staging");
 const size_t initial_work=work.used;HumanoidHeatDomains result;result.forecast_bytes=retained.used;result.analysis_bytes=retained.used+peak;result.parts.reserve(selected.size());
 const auto ref_world=scene.world_matrix(reference);auto shift=AffineTransform{}.matrix();for(size_t k=0;k<3;++k)shift[12+k]=-ref_world.matrix()[12+k];const AffineTransform origin(shift);
 auto profile=declared_weight_profile(rig,work,DeclaredRigGeometry::NativeBoneAxes);for(auto&s:profile.segments){auto f=[&](std::array<double,3>a){work.step(64);auto p=skin_affine_point(origin*ref_world,{float(a[0]),float(a[1]),float(a[2])});return std::array<double,3>{p.x,p.y,p.z};};s.head=f(s.head);s.tail=f(s.tail);}
 std::vector<HeatSourceSegment>eye_points;const auto segments=rig.segments();for(auto role:{"leftEye","rightEye"}){auto j=roles.at(role);const auto p=segments[j][0];work.step(64);auto q=(origin*ref_world).point({p.x,p.y,p.z});std::array<double,3>a{float(q[0]),float(q[1]),float(q[2])};eye_points.push_back({a,a,j});}if(eye_points.empty())throw std::invalid_argument("Current ocular boundary has no deform eye control");NearestWeightSources nearest_eye(eye_points,work);
 for(auto id:selected){work.stop();const auto&o=scene.object(id);const auto&m=o.painted->mesh();
  std::vector<std::vector<Vec3>>deltas;if(o.deformation->morphs){deltas.reserve(o.deformation->morphs->targets.size());for(const auto&t:o.deformation->morphs->targets){work.step_items(t.delta.size(),4);deltas.push_back(t.delta);}}
  // Sew only this exact raw owner and all authored morph rows before metric
  // conversion. This virtual graph changes neither raw arrays nor UV seams.
  auto sewn=geometric_boundary_sewing(m,deltas,maximum_bytes-retained.used,work);
  SkinGeometryWork geometry(work.maximum-work.used,work.cancel);const auto before=geometry.remaining();const auto metric=skin_world_mesh(m,origin*scene.world_matrix(id),geometry);work.step(before-geometry.remaining());
  DeclaredWeightPart input;input.object=id;input.geometric_row_groups=std::move(sewn.groups);HeatBindingOptions options;options.maximum_work=work.maximum-work.used;options.maximum_bytes=maximum_bytes-retained.used;
  const auto intent=declared_weight_intent(o,input,profile,options,work,DeclaredSupportPolicy::UniqueSourceWitnesses,metric,ref.deformation);
  // Ownership of one paint object is not an anatomical component boundary.
  // Analyze the exact intrinsic quotient before assigning ANY hard labels.
   // Analyze intrinsic components of the exact one-owner boundary quotient,
   // rather than treating newly cut UV charts as separate anatomical parts.
   Mesh intrinsic;std::vector<uint32_t>dense(m.positions.size(),UINT32_MAX),to_intrinsic(m.positions.size()),canonical_raw;canonical_raw.reserve(m.positions.size());intrinsic.positions.reserve(m.positions.size());intrinsic.indices.reserve(m.indices.size());
   for(uint32_t i=0;i<m.positions.size();++i){work.step(8);auto root=input.geometric_row_groups.at(i);auto&v=dense.at(root);if(v==UINT32_MAX){v=uint32_t(intrinsic.positions.size());intrinsic.positions.push_back(metric.positions.at(root));canonical_raw.push_back(root);}to_intrinsic[i]=v;}for(auto i:m.indices){work.step();intrinsic.indices.push_back(to_intrinsic.at(i));}
   size_t local=0;const auto components=heat_intent_components(intrinsic,{},local,work.maximum-work.used,work.cancel);work.step(local);std::map<uint32_t,std::vector<uint32_t>>vertices;for(uint32_t i=0;i<components.size();++i){work.step(16);vertices[components[i]].push_back(i);}std::vector<uint32_t>fixed(intrinsic.positions.size(),UINT32_MAX);bool any_fixed=false,has_head=false,has_ocular=false;
   for(const auto&[root,rows]:vertices){
    const auto masks=intent.masks(o);const auto aggregate=masks[canonical_raw.at(rows.front())];
    for(auto row:rows){work.step(8);if(masks[canonical_raw.at(row)]!=aggregate)throw std::logic_error("Current intrinsic intent changed within a component");}
    bool primary=(aggregate[head/64]&(uint64_t(1)<<(head%64)))!=0,ocular=aggregate!=HeatJointMask{};for(size_t k=0;k<4;++k){work.step(4);primary&=(aggregate[k]&~branch[k])==0;ocular&=(aggregate[k]&~eyes[k])==0;}
    if(!primary&&!ocular)continue;
    any_fixed=true;has_head|=primary;has_ocular|=ocular;uint32_t joint=head;
    if(ocular){
    Mesh subset;std::vector<uint32_t>map(intrinsic.positions.size(),UINT32_MAX);subset.positions.reserve(rows.size());subset.indices.reserve(intrinsic.indices.size());for(auto i:rows){work.step(8);map[i]=uint32_t(subset.positions.size());subset.positions.push_back(intrinsic.positions[i]);}for(size_t c=0;c<intrinsic.indices.size();c+=3){work.step(8);if(components[intrinsic.indices[c]]!=root)continue;for(size_t k=0;k<3;++k)subset.indices.push_back(map[intrinsic.indices[c+k]]);}
    // Large/non-disk regions are not silently reinterpreted as eye caps.
    if(subset.positions.size()>4096||subset.indices.size()/3>16384)throw std::length_error("Current ocular patch exceeds certified geometric descriptor");
    // Closed eye shells need a different witness from open convex disks.
    // The owning exact BVH and embedded-manifold check are local to THIS
    // current intrinsic component; no neighboring actor or skin data enters.
    std::vector<bone_shadow::Point>points;points.reserve(subset.positions.size());for(auto p:subset.positions){work.step(4);points.push_back({p.x,p.y,p.z});}
    auto query=std::make_shared<const visibility_geodesic::ClosestRayVisibility>(points,subset.indices,maximum_bytes-retained.used,work,true);
    visibility_geodesic::ClosestRayVisibility::Scratch scratch(*query);
    if(query->planned_bytes>maximum_bytes-retained.used)throw std::length_error("Current ocular owning BVH workspace");
    visibility_geodesic::ClosedMeshSourceDomain closed(query,maximum_bytes-retained.used-query->planned_bytes,work);
    if(closed.closed(0)){
      size_t inside=0;for(const auto&eye:eye_points){work.step(8);const auto p=eye.head;const bone_shadow::Point pivot{float(p[0]),float(p[1]),float(p[2])};const auto location=closed.closest(0,pivot,pivot,scratch,work);if(location==visibility_geodesic::ClosedMeshSourceDomain::Location::Boundary)throw std::invalid_argument("Current ocular pivot lies on the closed boundary");if(location==visibility_geodesic::ClosedMeshSourceDomain::Location::Inside){joint=eye.bone;++inside;}}
      if(inside>1)throw std::invalid_argument("Current closed ocular component contains multiple eye pivots");if(inside==1&&!rig.bones.at(joint).deform)joint=head;
      // No contained pivot grants no Eye authority. This is a prospective
      // geometric classification, never recovery from a failed solve/query.
    }else{
    const auto patch=analyze_projected_ocular_patch(subset,maximum_bytes-retained.used,work);
    if(patch.disk&&patch.convex_boundary){uint32_t selected_eye=UINT32_MAX;for(auto p:subset.positions){const auto mask=nearest_eye.nearest({p.x,p.y,p.z},work);uint32_t selected_joint=UINT32_MAX;size_t count=0;for(size_t k=0;k<4;++k){work.step();const auto bits=mask[k];count+=std::popcount(bits);if(bits)selected_joint=uint32_t(k*64+std::countr_zero(bits));}if(count!=1||(selected_eye!=UINT32_MAX&&selected_eye!=selected_joint))throw std::runtime_error("Current convex ocular patch lacks a unique actual eye control");selected_eye=selected_joint;}joint=rig.bones.at(selected_eye).deform?selected_eye:head;}
    }
    }
    for(auto i:rows){work.step();fixed[i]=joint;}
   }
  HumanoidHeatDomains::Part part{id,o.painted->mesh_owner(),{},has_head,has_ocular};
  // UINT32_MAX deliberately means this independent component retains the
  // actual generic heat energy; it is not a bone or an abandoned fixed label.
  if(any_fixed){part.fixed.resize(m.positions.size());for(size_t i=0;i<m.positions.size();++i){work.step();part.fixed[i]=fixed.at(to_intrinsic[i]);}}
  result.parts.push_back(std::move(part));
 }
 result.analysis_work=work.used-initial_work;return result;
}
}
