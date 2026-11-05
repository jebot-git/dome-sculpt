// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "weight-surface-system.hpp"
#include "dome/avatar.hpp"
#include "dome/source-placement.hpp"
namespace dome::detail {
// Explicit role-aware HANDLE construction, not original artist skin authority.
// It is not selected in response to a failed fit/solve. Unmapped/secondary
// controls retain their actual authored finite geometry and joint identities.
inline std::vector<HeatSourceSegment> humanoid_heat_sources(const Skeleton&rig,
    const HumanoidMapping&mapping,weight_surface::Budget&work){
  if(rig.bones.empty()||rig.bones.size()>256||mapping.joints.size()>55)
    throw std::length_error("Humanoid heat handle dimensions");
  work.step_items(rig.bones.size(),rig.bones.size()*32+128);mapping.validate(rig,false);
  const auto world=rig.world();auto actual=rig.segments();
  auto matrix=[](const RigMatrix&m){std::array<double,16>a;for(size_t k=0;k<16;++k)a[k]=m[k];return AffineTransform(a);};
  for(const auto&[role,joint]:mapping.joints){work.step(64);auto&segment=actual.at(joint);
    if(role=="leftEye"||role=="rightEye"){segment[1]=segment[0];continue;}
    const std::string side=role.starts_with("left")?"left":role.starts_with("right")?"right":"";
    const std::string next=role.ends_with("Hand")?side+"MiddleProximal":role.ends_with("Foot")?side+"Toes":"";
    if(!next.empty())if(auto found=mapping.joints.find(next);found!=mapping.joints.end()){
      const uint32_t child=found->second;int parent=rig.bones[child].parent;bool related=false;
      for(size_t depth=0;parent>=0&&depth<rig.bones.size();++depth){work.step();if(uint32_t(parent)==joint){related=true;break;}parent=rig.bones[size_t(parent)].parent;}
      if(!related)throw std::invalid_argument("Humanoid heat continuation is not an actual descendant");
      segment[1]={world[child][12],world[child][13],world[child][14]};continue;
    }
    if(role.ends_with("Toes")||role.ends_with("Distal")){
      const int parent=rig.bones[joint].parent;if(parent<0)throw std::invalid_argument("Terminal heat handle needs actual parent geometry");
      const auto own=matrix(world[joint]);auto local=own.inverse().point({world[size_t(parent)][12],world[size_t(parent)][13],world[size_t(parent)][14]});
      const double norm=std::hypot(local[0],local[1],local[2]);if(!(norm>0)||!std::isfinite(norm))throw std::invalid_argument("Collapsed terminal heat continuation");
      // Construct in the actual joint's authored frame, then transport once.
      // A common full affine placement cancels in inverse(world)*parentHead;
      // no world-axis normalization or native length used as world metres.
      const double length=rig.bones[joint].length;auto tip=own.point({-local[0]*length/norm,-local[1]*length/norm,-local[2]*length/norm});segment[1]={float(tip[0]),float(tip[1]),float(tip[2])};
    }
  }
  std::vector<HeatSourceSegment>out;out.reserve(rig.bones.size());
  for(uint32_t b=0;b<rig.bones.size();++b)if(rig.bones[b].deform){work.step(8);auto a=actual[b][0],z=actual[b][1];out.push_back({{a.x,a.y,a.z},{z.x,z.y,z.z},b});}
  return out;
}
}
