// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/source-placement.hpp"
#include "dome/scene.hpp"
#include "dome/tangents.hpp"
#include "affine-interaction.hpp"
#include <cmath>
namespace dome::detail {
inline AffineInteractionFrame placement_frame(std::span<const SceneGroup> groups,
    CanvasTransform local,const std::shared_ptr<const NativeAffineEdit> &edit) {
  std::vector<AffineTransform> factors;factors.reserve(groups.size()*3+2);
  for(const auto &group:groups) {
    if(group.affine_edit)factors.push_back(group.affine_edit->matrix());
    factors.push_back(affine_similarity(group.transform));
    if(group.source_placement)factors.push_back(group.source_placement->local());
  }
  if(edit)factors.push_back(edit->matrix());
  factors.push_back(affine_similarity(local));
  return AffineInteractionFrame::factored(factors);
}
inline AffineInteractionFrame placement_frame(const ObjectSurface &surface) {
  return placement_frame(surface.groups,surface.transform,surface.affine_edit);
}
inline AffineInteractionFrame placement_frame(const Scene &scene,uint64_t id) {
  const auto &object=scene.object(id);return placement_frame(scene.group_path(object.group),object.transform,object.affine_edit);
}
inline AffineVector affine_vector(Vec3 p){return {p.x,p.y,p.z};}
inline Vec3 affine_position(AffineVector p) {
  Vec3 out{float(p[0]),float(p[1]),float(p[2])};
  if(!finite(out))throw std::overflow_error("Placed geometry exceeds the float rendering coordinate range");
  return out;
}
inline Vec3 affine_unit(AffineVector p) {
  const auto length=std::hypot(p[0],p[1],p[2]);
  if(!std::isfinite(length)||length==0)throw std::invalid_argument("Placed normal or tangent is undefined");
  for(double &x:p)x/=length;
  return affine_position(p);
}
inline Vec3 placed_position(const AffineInteractionFrame &frame,Vec3 p){return affine_position(frame.world().point(affine_vector(p)));}
inline Vec3 placed_normal(const AffineInteractionFrame &frame,Vec3 p){return affine_unit(frame.normal(affine_vector(p)));}
inline Tangent placed_tangent(const AffineInteractionFrame &frame,Tangent t) {
  auto v=affine_unit(frame.world().direction({t[0],t[1],t[2]}));
  return {v.x,v.y,v.z,t[3]*(frame.orientation()==AffineOrientation::Reversing?-1.f:1.f)};
}
inline void place_mesh(Mesh &mesh,const AffineInteractionFrame &frame,const std::function<bool()> &cancelled={}) {
  for(size_t i=0;i<mesh.positions.size();++i){
    if((i&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Placed geometry cancelled");
    mesh.positions[i]=placed_position(frame,mesh.positions[i]);mesh.normals[i]=placed_normal(frame,mesh.normals[i]);
  }
}
} // namespace dome::detail
