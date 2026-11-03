// SPDX-License-Identifier: GPL-3.0-or-later
#include "shapekey-attributes.hpp"
#include "normal-bake-frames.hpp"
#include "dome/deformation.hpp"
#include "dome/tangents.hpp"
namespace dome::detail {
namespace {void stop(const std::function<bool()> &cancelled){if(cancelled&&cancelled())throw std::runtime_error("Shapekey authoring cancelled");}}
NormalFrameTarget author_key_attributes(const Mesh &rest,const NormalFrames &artist,
    const NormalFrameTarget *old_key,std::span<const Vec3> old_positions,
    std::span<const Vec3> new_positions,uint64_t id,const std::function<bool()> &cancelled){
 // Rotate the previous artist endpoint by the geometric frame's change. This
 // preserves smoothing/normal offsets; it does not label generated N/T as the
 // original authored channels. REST and every other key remain immutable.
 MorphSet before(rest);before.set_target(rest,"geometric-reference",old_positions);
 auto mesh=before.evaluate(rest,std::array<float,1>{1});
 NormalFrames geometric;geometric.origin_vertex_count=uint32_t(rest.positions.size());geometric.origin_vertices=rest.indices;
 geometric.normals.reserve(rest.indices.size());for(auto v:rest.indices)geometric.normals.push_back(mesh.normals[v]);
 if(!artist.tangents.empty()){
  geometric.tangents=tangent_corners(mesh,cancelled);geometric.generated_tangents=true;geometric.generated_uv=0;
 }
 auto after=detail::normal_bake_author_native_target(rest,geometric,new_positions,id,{128*1024*1024,128*1024*1024,cancelled}).attributes;
 NormalFrameTarget result;result.id=id;result.normals.reserve(rest.indices.size());if(!artist.tangents.empty())result.tangents.reserve(rest.indices.size());
 for(size_t c=0;c<rest.indices.size();++c){if((c&1023)==0)stop(cancelled);
  auto n0=geometric.normals[c],n1=n0+after.normals[c];Vec3 t0,t1;float handedness=1;
  if(!artist.tangents.empty()){
   const auto &t=geometric.tangents[c];t0={t[0],t[1],t[2]};t1=t0+after.tangents[c];handedness=t[3];
  }else{
   auto old_edge=old_positions[rest.indices[(c/3)*3+1]]-old_positions[rest.indices[(c/3)*3]];
   auto new_edge=new_positions[rest.indices[(c/3)*3+1]]-new_positions[rest.indices[(c/3)*3]];
   auto project=[](Vec3 edge,Vec3 n){return edge-n*dot(edge,n);};
   t0=project(old_edge,n0);t1=project(new_edge,n1);
   if(length(t0)<1e-10f||length(t1)<1e-10f){auto a=rest.indices[(c/3)*3],b=rest.indices[(c/3)*3+2];t0=project(old_positions[b]-old_positions[a],n0);t1=project(new_positions[b]-new_positions[a],n1);}
   if(length(t0)<1e-10f||length(t1)<1e-10f)throw std::invalid_argument("Shapekey edit collapses a geometric frame; reduce the adjustment");
   t0=normalized(t0);t1=normalized(t1);
  }
  auto b0=cross(n0,t0)*handedness,b1=cross(n1,t1)*handedness;
  auto transport=[&](Vec3 v){return t1*dot(v,t0)+b1*dot(v,b0)+n1*dot(v,n0);};
  auto previous=artist.normals[c];if(old_key&&!old_key->normals.empty())previous+=old_key->normals[c];
  result.normals.push_back(transport(previous)-artist.normals[c]);
  if(!artist.tangents.empty()){
   const auto &base=artist.tangents[c];Vec3 previous_t{base[0],base[1],base[2]};
   if(old_key&&!old_key->tangents.empty())previous_t+=old_key->tangents[c];
   result.tangents.push_back(transport(previous_t)-Vec3{base[0],base[1],base[2]});
  }
 }
 return result;
}

} // namespace dome::detail
