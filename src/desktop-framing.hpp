// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/document.hpp"
#include "dome/source-placement.hpp"
#include <limits>
namespace dome::detail {
struct DesktopFraming { Vec3 pivot,pan;float distance; };
inline bool desktop_range_eligible(const EditorState &state,const SurfaceRange &range,bool selected){
  auto object=std::find_if(state.objects.begin(),state.objects.end(),[&](const auto&o){return o.id==range.object;});
  if(object!=state.objects.end()&&!object->visible)return false;
  return !selected||(state.selected_objects.empty()?range.object==state.active_object:
      std::find(state.selected_objects.begin(),state.selected_objects.end(),range.object)!=state.selected_objects.end());
}
inline bool desktop_frameable(const EditorSnapshot &frame,bool selected){
  return frame.surface&&frame.state&&frame.surface->generation==frame.state->generation&&
      std::any_of(frame.surface->ranges.begin(),frame.surface->ranges.end(),[&](const auto&r){return r.count&&desktop_range_eligible(*frame.state,r,selected);});
}
// Read the same immutable packed geometry and live placement deltas as the
// renderer. This never extracts geometry, copies a Scene, or reads GPU buffers.
inline std::optional<DesktopFraming> desktop_framing(const EditorSnapshot &frame,
    Quat rotation,float aspect,bool selected) {
  if(!frame.surface||!frame.state||frame.surface->generation!=frame.state->generation||
      !std::isfinite(aspect)||aspect<=0)return {};
  rotation=rotation.unit();
  Vec3 low{INFINITY,INFINITY,INFINITY},high{-INFINITY,-INFINITY,-INFINITY};bool found=false;
  auto add=[&](Vec3 p){
    p=rotation.rotate(p);if(!finite(p))throw std::overflow_error("View framing exceeds finite coordinates");
    low={std::min(low.x,p.x),std::min(low.y,p.y),std::min(low.z,p.z)};
    high={std::max(high.x,p.x),std::max(high.y,p.y),std::max(high.z,p.z)};found=true;
  };
  const auto &mesh=frame.surface->mesh;const auto &state=*frame.state;
  for(const auto &range:frame.surface->ranges){
    auto object=std::find_if(state.objects.begin(),state.objects.end(),[&](const auto&o){return o.id==range.object;});
    if(!desktop_range_eligible(state,range,selected))continue;
    if(range.first>mesh.indices.size()||range.count>mesh.indices.size()-range.first)
      throw std::out_of_range("Invalid packed geometry while framing view");
    std::optional<AffineTransform> delta;
    if(object!=state.objects.end()&&object->affine_world&&range.affine_world&&
        object->affine_world->matrix()!=range.affine_world->matrix()){
      delta=(*object->affine_world)*range.affine_world->inverse();
    }else if(object!=state.objects.end()&&object->world_transform_valid&&range.world_frame_valid)
      delta=affine_similarity(ob::compose(object->world_transform,ob::inverse(range.object_world)));
    for(size_t c=range.first;c<range.first+range.count;++c){
      auto p=mesh.positions.at(mesh.indices[c]);
      if(delta){auto q=delta->point({p.x,p.y,p.z});p={float(q[0]),float(q[1]),float(q[2])};}
      add(p);
    }
  }
  if(!found)return {};
  const auto center=low*.5f+high*.5f,extent=(high-low)*.5f;
  constexpr float tangent=.41421356237309505f; // Same 45-degree vertical field of view as Canvas.
  const float distance=std::max(.01f,std::max(extent.x/(tangent*aspect),extent.y/tangent)*1.12f+extent.z+std::max(.0001f,length(extent)*.01f));
  if(!finite(center)||!std::isfinite(distance)||distance>std::numeric_limits<float>::max()/16)
    throw std::overflow_error("View framing exceeds camera range");
  return DesktopFraming{rotation.inverse().rotate(center),center*-1.f,distance};
}
inline Vec3 orbit_pan(Vec3 pan,Vec3 pivot,Quat before,Quat after){
  return pan+before.rotate(pivot)-after.rotate(pivot);
}
} // namespace dome::detail
