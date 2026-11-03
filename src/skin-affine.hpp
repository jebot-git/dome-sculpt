// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/skin-authoring.hpp"
#include "affine-interaction.hpp"
namespace dome::detail {
class SkinGeometryWork {
  size_t remaining_;
  const std::function<bool()> &cancelled_;
public:
  SkinGeometryWork(size_t remaining,const std::function<bool()> &cancelled):remaining_(remaining),cancelled_(cancelled){}
  void visit(){
    if(!remaining_)throw std::length_error("Skin placement exceeds world geometry work budget");
    --remaining_;
    if(cancelled_&&cancelled_())throw std::runtime_error("Skin authoring cancelled");
  }
  size_t remaining()const{return remaining_;}
};
inline Vec3 skin_affine_point(const AffineTransform &frame,Vec3 p){
  const auto q=frame.point({p.x,p.y,p.z});Vec3 result{float(q[0]),float(q[1]),float(q[2])};
  if(!finite(result)||(q[0]!=0&&result.x==0)||(q[1]!=0&&result.y==0)||(q[2]!=0&&result.z==0))
    throw std::overflow_error("Skin placement exceeds native geometry range");
  return result;
}
// Preserve local rest/pose and joint IDs. W'=C*W and I'=I*C^-1 make
// W'*I'=C*(W*I)*C^-1, including explicit and implicit identity bind spaces.
inline Skeleton skin_reframe(const Skeleton &source,const AffineTransform &change,SkinGeometryWork &work){
  auto result=source;
  result.space_matrix=(change*(source.space_matrix?AffineTransform(*source.space_matrix):AffineTransform{})).float_matrix();
  if(source.inverse_bind_override){
    const auto inverse=change.inverse();
    for(size_t i=0;i<result.inverse_bind_override->size();++i){work.visit();
      result.inverse_bind_override->at(i)=(AffineTransform(source.inverse_bind_override->at(i))*inverse).float_matrix();}
  }else for(size_t i=0;i<source.bones.size();++i)work.visit();
  result.validate();return result;
}
inline Mesh skin_world_mesh(const Mesh &source,const AffineTransform &world,SkinGeometryWork &work){
  Mesh result=source;AffineInteractionFrame frame(world);
  for(size_t i=0;i<result.positions.size();++i){work.visit();result.positions[i]=skin_affine_point(world,source.positions[i]);
    if(!source.normals.empty()){
      const auto n=frame.normal({source.normals[i].x,source.normals[i].y,source.normals[i].z});
      const auto magnitude=std::hypot(n[0],n[1],n[2]);
      if(!(magnitude>0)||!std::isfinite(magnitude))throw std::runtime_error("Skin world normal collapsed");
      result.normals[i]={float(n[0]/magnitude),float(n[1]/magnitude),float(n[2]/magnitude)};
    }
  }
  if(frame.orientation()==AffineOrientation::Reversing)for(size_t i=0;i<result.indices.size();i+=3){
    work.visit();std::swap(result.indices[i+1],result.indices[i+2]);
    if(!result.uv_seams.empty())std::swap(result.uv_seams[i],result.uv_seams[i+2]);
  }
  result.validate();return result;
}
inline HumanLandmarks skin_reframe_landmarks(const HumanLandmarks &source,const AffineTransform &change,SkinGeometryWork &work){
  auto result=source;
  for(auto &[name,p]:result.points){(void)name;work.visit();p=skin_affine_point(change,p);}
  auto direction=[&](Vec3 p){const auto v=change.direction({p.x,p.y,p.z});return Vec3{float(v[0]),float(v[1]),float(v[2])};};
  // These are authoring axes, not affine/skin authority. Keep transformed
  // left/up semantics in a valid orthonormal frame; front is their cross.
  result.left=normalized(direction(source.left));auto up=direction(source.up);
  result.up=normalized(up-result.left*dot(up,result.left));result.front=normalized(cross(result.left,result.up));
  if(!finite(result.left)||!finite(result.up)||!finite(result.front)||length(cross(result.left,result.up))<.99f)
    throw std::runtime_error("Skin landmark authoring frame is degenerate");
  return result;
}
} // namespace dome::detail
