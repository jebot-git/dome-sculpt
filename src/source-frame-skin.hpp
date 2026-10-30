// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "source-tangent-frames.hpp"
#include "dome/rig.hpp"
namespace dome::detail {
// The caller selects a versioned consumer convention. In particular, a weighted
// sum of joint inverse transposes is not inverse-transpose(weighted joints).
// These primitives expose that choice instead of silently declaring it universal.
inline Vec3 source_frame_direction(const RigMatrix &m,Vec3 v) {
  return {m[0]*v.x+m[4]*v.y+m[8]*v.z,
          m[1]*v.x+m[5]*v.y+m[9]*v.z,
          m[2]*v.x+m[6]*v.y+m[10]*v.z};
}
inline RigMatrix source_frame_inverse_transpose(const RigMatrix &m) {
  for(float value:m)if(!std::isfinite(value))throw std::invalid_argument("Source frame matrix is nonfinite");
  const double a=m[0],b=m[4],c=m[8],d=m[1],e=m[5],f=m[9],g=m[2],h=m[6],i=m[10];
  const double determinant=a*(e*i-f*h)-b*(d*i-f*g)+c*(d*h-e*g);
  const double magnitude=std::sqrt(a*a+d*d+g*g)*std::sqrt(b*b+e*e+h*h)*std::sqrt(c*c+f*f+i*i);
  if(!std::isfinite(magnitude) || magnitude==0 || std::abs(determinant)<=magnitude*1e-12)
    throw std::invalid_argument("Source frame matrix is singular or ill-conditioned");
  // Column-major cofactor matrix /det =inverse-transpose; no translation.
  RigMatrix result{float((e*i-f*h)/determinant),float((c*h-b*i)/determinant),float((b*f-c*e)/determinant),0,
                   float((f*g-d*i)/determinant),float((a*i-c*g)/determinant),float((c*d-a*f)/determinant),0,
                   float((d*h-e*g)/determinant),float((b*g-a*h)/determinant),float((a*e-b*d)/determinant),0,
                   0,0,0,1};
  for(float value:result)if(!std::isfinite(value))throw std::invalid_argument("Source frame inverse transpose overflowed");
  return result;
}
inline SourceLocalFrame source_skin_frame(const SourceLocalFrame &local,
    std::span<const Influence> influences,std::span<const RigMatrix> directions,
    std::span<const RigMatrix> normal_matrices) {
  if(directions.empty() || directions.size()>1024 || normal_matrices.size()!=directions.size() ||
     influences.empty() || influences.size()>32 || !finite(local.tangent) || !finite(local.normal) ||
     (local.handedness!=-1 && local.handedness!=1))
    throw std::invalid_argument("Source skin frame identities are invalid");
  SourceLocalFrame result{{},{},local.handedness};double sum=0;
  std::array<uint8_t,1024> seen{};
  for(const auto &influence:influences) {
    if(influence.joint>=directions.size() || !std::isfinite(influence.weight) || influence.weight<=0 || influence.weight>1 || seen[influence.joint])
      throw std::invalid_argument("Source skin frame influences must be positive and unique");
    seen[influence.joint]=1;sum+=influence.weight;
    const auto &linear=directions[influence.joint],&normal=normal_matrices[influence.joint];
    for(float value:linear)if(!std::isfinite(value))throw std::invalid_argument("Source joint direction matrix is nonfinite");
    for(float value:normal)if(!std::isfinite(value))throw std::invalid_argument("Source joint normal matrix is nonfinite");
    result.tangent+=source_frame_direction(linear,local.tangent)*influence.weight;
    result.normal+=source_frame_direction(normal,local.normal)*influence.weight;
  }
  const float tangent_length=length(result.tangent),normal_length=length(result.normal);
  if(std::abs(sum-1)>1e-4 || !finite(result.tangent) || !finite(result.normal) ||
     !std::isfinite(tangent_length) || !std::isfinite(normal_length) || tangent_length<1e-10f || normal_length<1e-10f)
    throw std::invalid_argument("Source skinned frame is singular or weights are not normalized");
  // Do not normalize, orthogonalize or invent a reflection sign here. The
  // selected vertex/fragment convention owns those operations downstream.
  return result;
}
} // namespace dome::detail
