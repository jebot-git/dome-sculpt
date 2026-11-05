// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "native-head-geometry.hpp"
#include "native-anatomical-domain.hpp"
#include "rotary-cap-geometry.hpp"
namespace dome::detail {
// Geometry-generated boundary DATA, not a publication/immutable material seal.
// Input must be a detached worker Scene. Actual native admission/stale leases
// remain a separate requirement. No original artist influence values are read.
struct HumanoidHeatDomains {
  struct Part{uint64_t object;std::shared_ptr<const Mesh>mesh;std::vector<uint32_t>fixed;bool head=false,ocular=false;};
  std::vector<Part>parts;
  size_t analysis_work=0,analysis_bytes=0,forecast_bytes=0;
};
inline HumanoidHeatDomains source_humanoid_heat_domains(const Scene&scene,
    std::span<const uint64_t>selected,uint64_t reference,
    weight_surface::Budget&work,size_t maximum_bytes){
  auto source=native_anatomical_domain(scene,selected,work);
  auto analysis=analyze_native_head_geometry(scene,reference,std::min(size_t(100'000'000),work.maximum-work.used),maximum_bytes,work.cancel,source.objects);work.step(analysis.work);
  const auto&ref=scene.object(reference);const auto&roles=ref.deformation->humanoid->joints;const auto&rig=*ref.deformation->skeleton;const auto head=roles.at("head");
  HeatJointMask eyes{};for(auto role:{"leftEye","rightEye"}){auto j=roles.at(role);eyes[j/64]|=uint64_t(1)<<(j%64);}
  const auto ref_world=scene.world_matrix(reference);auto offset=AffineTransform{}.matrix();for(size_t k=0;k<3;++k)offset[12+k]=-ref_world.matrix()[12+k];const AffineTransform origin(offset);
  std::vector<HeatSourceSegment>eye_points;const auto segments=rig.segments();for(auto role:{"leftEye","rightEye"}){const auto j=roles.at(role);if(!rig.bones[j].deform)continue;auto p=segments[j][0];auto q=(origin*ref_world).point({p.x,p.y,p.z});std::array<double,3>a{float(q[0]),float(q[1]),float(q[2])};eye_points.push_back({a,a,j});}
  NearestWeightSources nearest_eye(eye_points,work);HumanoidHeatDomains result;result.analysis_work=analysis.work;result.analysis_bytes=analysis.bytes;
  size_t planned=65536;for(auto id:source.objects){const auto&m=scene.object(id).painted->mesh();const auto bytes=m.positions.size()*(sizeof(uint32_t)+64)+m.indices.size()*4;if(bytes>maximum_bytes-planned)throw std::length_error("All anatomical boundary data before staging");planned+=bytes;}
  result.forecast_bytes=planned;
  for(auto id:source.objects){const auto&o=scene.object(id);auto found=std::find_if(analysis.parts.begin(),analysis.parts.end(),[&](auto&p){return p.object==id;});if(found==analysis.parts.end())throw std::runtime_error("Head domain omitted actual current part");
    bool ocular=!found->head_region&&found->geometric_controls!=HeatJointMask{};for(size_t k=0;k<4;++k)ocular&=(found->geometric_controls[k]&~eyes[k])==0;
    HumanoidHeatDomains::Part part{id,o.painted->mesh_owner(),{},found->head_region,ocular};
    if(part.head||ocular){const auto&m=*part.mesh;part.fixed.assign(m.positions.size(),head);
      if(!o.deformation||!o.deformation->humanoid||o.deformation->humanoid->joints!=roles)throw std::invalid_argument("Anatomical boundary labels need actual current joint-role identity");
      if(ocular){size_t local=0;auto components=heat_intent_components(m,{},local,work.maximum-work.used,work.cancel);work.step(local);std::map<uint32_t,std::vector<uint32_t>>vertices;for(uint32_t i=0;i<components.size();++i){work.step(16);vertices[components[i]].push_back(i);}
        SkinGeometryWork geometry(work.maximum-work.used,work.cancel);const auto before=geometry.remaining();auto metric=skin_world_mesh(m,origin*scene.world_matrix(id),geometry);work.step(before-geometry.remaining());
        for(const auto&[root,rows]:vertices){const auto bytes=rows.size()*112+m.indices.size()*160+32768+m.positions.size()*32;if(bytes>maximum_bytes-planned)throw std::length_error("Ocular boundary workspace before subset copies");Mesh subset;std::vector<uint32_t>map(m.positions.size(),UINT32_MAX);subset.positions.reserve(rows.size());subset.indices.reserve(m.indices.size());for(auto i:rows){work.step(8);map[i]=uint32_t(subset.positions.size());subset.positions.push_back(metric.positions[i]);}for(size_t c=0;c<m.indices.size();c+=3){work.step(8);if(components[m.indices[c]]!=root)continue;for(size_t k=0;k<3;++k)subset.indices.push_back(map[m.indices[c+k]]);}
          auto patch=analyze_projected_ocular_patch(subset,maximum_bytes-planned,work);
          uint32_t joint=head;
          if(patch.disk&&patch.convex_boundary){uint32_t selected_eye=UINT32_MAX;for(auto p:subset.positions){const auto mask=nearest_eye.nearest({p.x,p.y,p.z},work);uint32_t selected_joint=UINT32_MAX;size_t count=0;for(size_t k=0;k<4;++k){work.step();auto bits=mask[k];count+=std::popcount(bits);if(bits)selected_joint=uint32_t(k*64+std::countr_zero(bits));}if(count!=1||(selected_eye!=UINT32_MAX&&selected_eye!=selected_joint))throw std::runtime_error("Convex ocular patch lacks one actual current eye control");selected_eye=selected_joint;}joint=selected_eye;}
          for(auto i:rows){work.step();part.fixed[i]=joint;}
        }
      }
    }
    result.parts.push_back(std::move(part));
  }
  work.stop();return result;
}
}
