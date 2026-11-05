// SPDX-License-Identifier: GPL-3.0-or-later
#include "refined-mirror-draft.hpp"
#include <algorithm>
#include <cmath>
namespace dome::detail {
namespace {
struct D {
  double x, y, z;
};
D operator+(D a, D b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
D operator-(D a, D b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
D operator*(D a, double b) { return {a.x * b, a.y * b, a.z * b}; }
double dot(D a, D b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
D cross(D a, D b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
double norm(D a) { return std::sqrt(dot(a, a)); }
D vector(Vec3 a) { return {a.x, a.y, a.z}; }
D reflect(D a) { return {-a.x, a.y, a.z}; }
Vec3 stored(D a) {
  Vec3 v{float(a.x), float(a.y), float(a.z)};
  if (!finite(v))
    throw std::invalid_argument("Relative Mirror produced an unrepresentable artist direction");
  return v;
}
struct Basis {
  D x, y, z;
  double determinant;
  D apply(D a) const {
    const double u = dot(a, cross(y, z)) / determinant, v = dot(x, cross(a, z)) / determinant,
                 w = dot(x, cross(y, a)) / determinant;
    return {u, v, w};
  }
  D compose(D a) const { return x * a.x + y * a.y + z * a.z; }
};
Basis basis(D x, D y, D z) {
  const double d = dot(x, cross(y, z)), scale = norm(x) * norm(y) * norm(z);
  if (!std::isfinite(d) || !std::isfinite(scale) || scale == 0 || std::abs(d) <= scale * 1e-8)
    throw std::invalid_argument("Relative Mirror has no invertible actual artist basis");
  return {x, y, z, d};
}
D normal_only(D source, D target, D delta) {
  // Unique shortest proper rotation is a declared N-only conversion. Antipodal
  // bases have no supplied tangent/axis authority and cannot invent one.
  source = source * (1 / norm(source));
  target = target * (1 / norm(target));
  const double cosine = std::clamp(dot(source, target), -1., 1.);
  if (cosine <= -1 + 1e-12)
    throw std::invalid_argument("N-only relative Mirror has an ambiguous antipodal base");
  const auto axis = cross(source, target);
  return delta + cross(axis, delta) + cross(axis, cross(axis, delta)) * (1 / (1 + cosine));
}

NormalFrameTarget relative_target(const FrameAttributeSampling &anchor,size_t role,uint64_t source_id,uint64_t id,const std::function<bool()> &cancelled){
 const auto &before=*anchor.role(SampledFrameRole(role));const auto &uv=anchor.role_coordinates(SampledFrameRole(role));const auto &map=anchor.refinement();
 const auto selected=std::find_if(before.targets.begin(),before.targets.end(),[&](const auto&t){return t.id==source_id;});
 if(selected==before.targets.end())throw std::invalid_argument("Refined Mirror lost its actual current named role target");
 const auto &source=*selected;const size_t count=map.corners.size();NormalFrameTarget result;result.id=id;
 if(!source.normals.empty())result.normals.resize(count);if(!source.tangents.empty())result.tangents.resize(count);
 for(size_t c=0;c<count;c+=3){if(cancelled&&cancelled())throw std::runtime_error("Refined Mirror draft cancelled");int sigma=0,tau=0;
  if(!before.tangents.empty()){
   double u[3],v[3],su[3],sv[3];for(size_t k=0;k<3;++k){const auto at=map.opposite[c+k];u[k]=uv[c+k][0];v[k]=uv[c+k][1];su[k]=uv[at][0];sv[k]=uv[at][1];}
   const double u1=u[1]-u[0],u2=u[2]-u[0],v1=v[1]-v[0],v2=v[2]-v[0],a1=su[1]-su[0],a2=su[2]-su[0],b1=sv[1]-sv[0],b2=sv[2]-sv[0],det=u1*v2-u2*v1,a=a1*v2-a2*v1,b=a2*u1-a1*u2,d=b2*u1-b1*u2,e=b1*v2-b2*v1,j=a*d-b*e;
   if(!std::isfinite(det)||!std::isfinite(j)||!det||!j||!a||!d||(a>0)==(d>0)||j>=0||std::max(std::abs(b),std::abs(e))>1e-5*std::max(std::abs(a),std::abs(d)))throw std::invalid_argument("Refined Mirror needs genuine diagonal role-chart parity; cross-axis transport remains separate");
   sigma=(a>0)==(det>0)?1:-1;tau=(d>0)==(det>0)?1:-1;
  }
  for(size_t k=0;k<3;++k){const auto at=c+k,s=map.opposite[at];const auto ns=vector(before.normals[s]),nt=vector(before.normals[at]),rn=reflect(ns);
   if(before.tangents.empty()){if(!source.normals.empty())result.normals[at]=stored(normal_only(rn,nt,reflect(vector(source.normals[s]))));continue;}
   const auto ts=before.tangents[s],tt=before.tangents[at];if(ts[3]!=tt[3])throw std::invalid_argument("Refined Mirror cannot invent tangent handedness morphs");const D st{ts[0],ts[1],ts[2]},dt{tt[0],tt[1],tt[2]};const auto sb=cross(ns,st)*ts[3],db=cross(nt,dt)*tt[3];const auto input=basis(reflect(st)*sigma,reflect(sb)*tau,rn),output=basis(dt,db,nt);auto convert=[&](D delta){return output.compose(input.apply(delta));};
   if(!source.normals.empty())result.normals[at]=stored(convert(reflect(vector(source.normals[s]))));if(!source.tangents.empty())result.tangents[at]=stored(convert(reflect(vector(source.tangents[s]))*sigma));
  }
 }
 return result;
}
}
RefinedMirrorDraft derive_refined_mirror(std::shared_ptr<const FrameAttributeSampling> anchor,size_t slot,std::string name,const PaintSamplingLimits &limits){
 auto stop=[&]{if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Refined Mirror draft cancelled");};stop();
 if(!anchor||!anchor->valid_owner(anchor)||!limits.max_working_bytes||limits.max_working_bytes>512*1024*1024||!limits.max_work||limits.max_work>256*1024*1024)throw std::invalid_argument("Refined Mirror draft requires its genuine captured anchor and bounded policy");
 anchor->validate(limits);const auto &before=*anchor->before();const auto &model=before.deformation;const auto &frames=*before.material_bindings->normal_frames;const auto &map=anchor->refinement();const auto n=map.corners.size();
 if(name.empty()||name.size()>1024||name.find('\0')!=std::string::npos||!model||!model->morphs||slot>=model->morphs->targets.size()||slot>=frames.current_targets.size())throw std::invalid_argument("Refined Mirror needs an actual selected current target and bounded name");
 if(model->morphs->targets.size()>=128||frames.next_target==UINT64_MAX||!frames.next_target)throw std::length_error("Refined Mirror target identity/count budget exceeded");
 for(const auto &target:model->morphs->targets)if(target.name==name)throw std::invalid_argument("Refined Mirror target name already exists");
 size_t bytes=anchor->working_bytes();auto charge=[&](size_t count,size_t size){if(count>(limits.max_working_bytes-bytes)/size)throw std::length_error("Refined Mirror aggregate working quota exceeded before output allocation");bytes+=count*size;};charge(1,sizeof(RefinedMirrorDraft));charge(name.capacity()+1,1);charge(n,sizeof(Vec3));
 const uint64_t source_id=frames.current_targets[slot];for(size_t role=0;role<3;++role)if(const auto *field=anchor->role(SampledFrameRole(role))){const auto selected=std::find_if(field->targets.begin(),field->targets.end(),[&](const auto&t){return t.id==source_id;});if(selected==field->targets.end())throw std::invalid_argument("Refined Mirror source stable ID lost its actual role endpoint");if(!selected->normals.empty())charge(n,sizeof(Vec3));if(!selected->tangents.empty())charge(n,sizeof(Vec3));}
 if(n>limits.max_work/64)throw std::length_error("Refined Mirror draft work quota exceeded before output allocation");
 // Existing baseline targets remain immutable in anchor.deformation. Only the
 // new endpoint is allocated here; full Scene adoption needs recursive caps.
 RefinedMirrorDraft result;result.anchor=std::move(anchor);result.source_target=source_id;result.target=frames.next_target;result.name=std::move(name);result.working_bytes=bytes;result.positions.reserve(n);const auto &delta=model->morphs->targets[slot].delta;const auto &mesh=before.paint->mesh();
 for(const auto &corner:map.corners){stop();Vec3 value{};for(size_t k=0;k<3;++k)value+=delta.at(mesh.indices.at(corner.reflected.triangle*3+k))*float(corner.reflected.barycentric[k]);value.x=-value.x;if(!finite(value))throw std::invalid_argument("Refined Mirror target POSITION is not representable");result.positions.push_back(value);}
 for(size_t role=0;role<3;++role)if(result.anchor->role(SampledFrameRole(role)))result.roles[role]=relative_target(*result.anchor,role,source_id,result.target,limits.cancelled);
 stop();return result;
}
}
