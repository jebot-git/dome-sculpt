// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake-raster-device.hpp"
#include "paint-composite-kernel.hpp"
#include <cfloat>

#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_CAGE_INLINE __host__ __device__ inline
#else
#define DOME_CAGE_INLINE inline
#endif

namespace dome::detail::bake_raster {
DOME_CAGE_INLINE Vec3 interpolate(const Vec3 (&values)[3],const float (&weights)[3]){
  Vec3 result;
  for(unsigned i=0;i<3;++i){
    result.x=result.x+values[i].x*weights[i];
    result.y=result.y+values[i].y*weights[i];
    result.z=result.z+values[i].z*weights[i];
  }
  return result;
}

DOME_CAGE_INLINE float cage_length(Vec3 value){
  return paint_compute::normal_length(value.x*value.x+value.y*value.y+value.z*value.z);
}

DOME_CAGE_INLINE Vec3 scale(Vec3 value,float factor){
  return {value.x*factor,value.y*factor,value.z*factor};
}

DOME_CAGE_INLINE bool prepare_ray(const ProjectionFace &face,const Sample &sample,
    ProjectionSettings settings,bake_rays::ProjectionRay &ray){
  const auto position=interpolate(face.positions,sample.barycentric);
  const auto blended=interpolate(face.normals,sample.barycentric);
  const float length=cage_length(blended);
  const float normal_limit=settings.supplied_tangents?1e-8f:1e-12f;
  const auto normal=length>normal_limit?scale(blended,paint_compute::normal_reciprocal(length)):
                         settings.supplied_tangents?face.geometric_normal:Vec3{0,1,0};
  auto offset=scale(normal,settings.front);
  if(settings.explicit_cage){
    const auto cage=interpolate(face.cage,sample.barycentric);
    offset={cage.x-position.x,cage.y-position.y,cage.z-position.z};
  }
  const float front=cage_length(offset);
  const auto direction=scale(front>1e-8f?scale(offset,paint_compute::normal_reciprocal(front)):normal,-1.f);
  const float magnitude=cage_length(direction),maximum=front+settings.back;
  const Vec3 origin{position.x+offset.x,position.y+offset.y,position.z+offset.z};
  if(!(magnitude>=1e-12f&&magnitude<=FLT_MAX&&maximum>=0&&maximum<=1000))return false;
  if(!(origin.x>=-1000&&origin.x<=1000&&origin.y>=-1000&&origin.y<=1000&&origin.z>=-1000&&origin.z<=1000))
    return false;
  ray={origin,magnitude>1e-12f?scale(direction,paint_compute::normal_reciprocal(magnitude)):Vec3{0,1,0},
       face.radius_bits,maximum};
  return true;
}
} // namespace dome::detail::bake_raster
#undef DOME_CAGE_INLINE
