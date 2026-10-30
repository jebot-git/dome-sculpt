// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/csg.hpp"
#include <stdexcept>
#if defined(__CUDACC__) || defined(__HIPCC__)
#define DOME_CSG_DEVICE __host__ __device__
#else
#define DOME_CSG_DEVICE
#endif
namespace dome::csg_detail {
inline constexpr size_t maximum_samples=4'000'000, maximum_boxes=2048,
                        maximum_operations=128'000'000;
inline void checkpoint(const std::function<bool()> &cancelled) {
  if(cancelled && cancelled())throw std::runtime_error("Procedural sculpt cancelled");
}
inline void validate(std::span<const Sample> samples,std::span<const CsgBox> boxes,
                     float band,const std::function<bool()> &cancelled) {
  checkpoint(cancelled);
  if(!std::isfinite(band) || band<=0 || band>1e6f)throw std::invalid_argument("Invalid CSG distance band");
  if(samples.size()>maximum_samples || boxes.size()>maximum_boxes ||
     (!boxes.empty() && samples.size()>maximum_operations/boxes.size()))
    throw std::length_error("Procedural field evaluation exceeds the work budget");
  for(const auto &b:boxes) {
    checkpoint(cancelled);
    const auto extent=b.maximum-b.minimum;
    if(!finite(b.minimum)||!finite(b.maximum)||extent.x<=0||extent.y<=0||extent.z<=0 ||
       std::max({std::abs(b.minimum.x),std::abs(b.minimum.y),std::abs(b.minimum.z),
                 std::abs(b.maximum.x),std::abs(b.maximum.y),std::abs(b.maximum.z)})>1e6f ||
       !std::isfinite(b.rounding)||b.rounding<0 ||
       b.rounding>std::min({extent.x,extent.y,extent.z})*.5f ||
       uint32_t(b.operation)>uint32_t(CsgOperation::Difference))
      throw std::invalid_argument("Invalid procedural rounded box");
  }
  for(size_t n=0;n<samples.size();++n) {
    if((n&1023)==0)checkpoint(cancelled);
    const auto &s=samples[n];
    if(!finite(s.position)||!std::isfinite(s.distance)||std::abs(s.distance)>band ||
       !std::isfinite(s.influence)||s.influence<0||s.influence>1)
      throw std::invalid_argument("Invalid procedural field sample");
  }
}
DOME_CSG_DEVICE inline float box_distance(Vec3 p,const CsgBox &b) {
  float x=fabsf(p.x-(b.minimum.x+b.maximum.x)*.5f)-((b.maximum.x-b.minimum.x)*.5f-b.rounding);
  float y=fabsf(p.y-(b.minimum.y+b.maximum.y)*.5f)-((b.maximum.y-b.minimum.y)*.5f-b.rounding);
  float z=fabsf(p.z-(b.minimum.z+b.maximum.z)*.5f)-((b.maximum.z-b.minimum.z)*.5f-b.rounding);
  float ox=fmaxf(x,0),oy=fmaxf(y,0),oz=fmaxf(z,0);
  return sqrtf(ox*ox+oy*oy+oz*oz)+fminf(fmaxf(x,fmaxf(y,z)),0)-b.rounding;
}
DOME_CSG_DEVICE inline float sample(Sample input,const CsgBox *boxes,size_t count,float band) {
  if(input.influence==0)return input.distance;
  float value=input.distance;
  for(size_t n=0;n<count;++n) {
    float distance=box_distance(input.position,boxes[n]);
    value=boxes[n].operation==CsgOperation::Union?fminf(value,distance):fmaxf(value,-distance);
  }
  value=fmaxf(-band,fminf(band,value));
  return input.influence==1?value:input.distance+(value-input.distance)*input.influence;
}
// Bound each launch by arithmetic work as well as device storage. Cancellation
// is observed between chunks; long lists do not become one unbounded kernel.
inline size_t chunk_limit(size_t boxes) {
  return std::min(size_t(2'000'000),std::max(size_t(1),size_t(1'000'000)/boxes));
}
} // namespace dome::csg_detail
