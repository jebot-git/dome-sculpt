// SPDX-License-Identifier: GPL-3.0-or-later
#include "refined-role-sampling.hpp"
#include <cmath>
namespace dome::detail {
RefinedRoleField sample_refined_role_field(const KeyMirrorRefinement &r,
    std::shared_ptr<const NormalFrames> role,const KeyMirrorLimits &limits) {
  if(!role||!r.source||!limits.max_bytes||limits.max_bytes>256*1024*1024||
     !limits.max_work||limits.max_work>256*1024*1024)
    throw std::invalid_argument("Invalid detached refined-role input/limits");
  auto stop=[&]{if(limits.cancelled&&limits.cancelled())
    throw std::runtime_error("Refined role sampling cancelled");};
  stop();const size_t count=r.source->indices.size(),n=r.corners.size();
  if(!count||count%3||count>6'000'000||!n||n%3||n>6'000'000||
     r.opposite.size()!=n||role->normals.size()!=count||
     (!role->tangents.empty()&&role->tangents.size()!=count)||role->targets.size()>128)
    throw std::invalid_argument("Refined role needs explicit ordered source corners");
  size_t bytes=0,work=0;
  auto charge=[&](size_t amount,size_t unit){if(amount>(limits.max_bytes-bytes)/unit)
    throw std::length_error("Refined role working budget exceeded before allocation");bytes+=amount*unit;};
  auto operations=[&](size_t amount){if(amount>limits.max_work-work)
    throw std::length_error("Refined role work budget exceeded before allocation");work+=amount;};
  charge(1,sizeof(RefinedRoleField));charge(1,sizeof(NormalFrames));
  charge(role->origin_vertices.capacity()+role->source_vertices.capacity()+role->source_corners.capacity(),sizeof(uint32_t));
  charge(role->normals.capacity(),sizeof(Vec3));charge(role->tangents.capacity(),sizeof(std::array<float,4>));
  charge(role->targets.capacity(),sizeof(NormalFrameTarget));
  for(const auto &t:role->targets){stop();charge(t.normals.capacity()+t.tangents.capacity(),sizeof(Vec3));}
  const auto &mesh=*r.source;charge(1,sizeof(Mesh));
  charge(mesh.positions.capacity()+mesh.normals.capacity(),sizeof(Vec3));
  charge(mesh.uv.capacity(),sizeof(std::array<float,2>));charge(mesh.indices.capacity(),sizeof(uint32_t));
  charge(mesh.uv_pins.capacity()+mesh.uv_seams.capacity(),1);
  charge(1,sizeof(KeyMirrorRefinement));charge(r.corners.capacity(),sizeof(MirrorRefinementCorner));charge(r.opposite.capacity(),sizeof(uint32_t));
  charge(n,sizeof(Vec3));operations(n*6); // three barycentric/source-normal reads each
  if(!role->tangents.empty()){charge(n,sizeof(std::array<float,4>));operations(n*6);}
  charge(role->targets.size(),sizeof(NormalFrameTarget));
  for(const auto &t:role->targets){stop();for(const auto *v:{&t.normals,&t.tangents}){
    if(!v->empty()&&v->size()!=count)throw std::invalid_argument("Refined role target corner dimensions differ");
    if(!v->empty()){charge(n,sizeof(Vec3));operations(n*3);}
  }}
  // Both output dimensions and all resident source/provenance/output bytes are
  // reserved before the first output vector. This helper borrows all inputs.
  RefinedRoleField out;out.before=std::move(role);out.working_bytes=bytes;out.work_units=work;
  out.normals.resize(n);if(!out.before->tangents.empty())out.tangents.resize(n);
  out.targets.reserve(out.before->targets.size());
  for(const auto &t:out.before->targets){NormalFrameTarget sampled;sampled.id=t.id;
    if(!t.normals.empty())sampled.normals.resize(n);
    if(!t.tangents.empty())sampled.tangents.resize(n);out.targets.push_back(std::move(sampled));}
  for(size_t c=0;c<n;++c){stop();const auto &p=r.corners[c].current;
    if(p.triangle>=count/3||r.opposite[c]>=n||r.opposite[r.opposite[c]]!=c)
      throw std::invalid_argument("Refined role borrowed invalid logical corner provenance");
    double sum=0;for(auto v:p.barycentric){if(!std::isfinite(v)||v< -1e-12||v>1+1e-12)
      throw std::invalid_argument("Refined role has invalid barycentric coordinates");sum+=v;}
    if(std::abs(sum-1)>1e-12)throw std::invalid_argument("Refined role barycentrics are not affine");
    auto sample=[&](const std::vector<Vec3> &values){Vec3 result{};
      for(size_t k=0;k<3;++k)result+=values.at(size_t(p.triangle)*3+k)*float(p.barycentric[k]);
      if(!finite(result))throw std::invalid_argument("Refined role produced a nonfinite raw field");return result;};
    out.normals[c]=sample(out.before->normals);
    if(!out.tangents.empty()){
      Vec3 tangent{};const auto first=out.before->tangents.at(size_t(p.triangle)*3)[3];
      if(first!=1&&first!=-1)throw std::invalid_argument("Refined role has invalid tangent handedness");
      for(size_t k=0;k<3;++k){const auto t=out.before->tangents.at(size_t(p.triangle)*3+k);
        if(t[3]!=first)throw std::invalid_argument("Refined role triangle has conflicting handedness");
        tangent+=Vec3{t[0],t[1],t[2]}*float(p.barycentric[k]);}
      if(!finite(tangent))throw std::invalid_argument("Refined role produced a nonfinite tangent field");
      out.tangents[c]={tangent.x,tangent.y,tangent.z,first};
    }
    for(size_t t=0;t<out.targets.size();++t){const auto &before=out.before->targets[t];
      if(!out.targets[t].normals.empty())out.targets[t].normals[c]=sample(before.normals);
      if(!out.targets[t].tangents.empty())out.targets[t].tangents[c]=sample(before.tangents);}
  }
  stop();return out;
}
}
