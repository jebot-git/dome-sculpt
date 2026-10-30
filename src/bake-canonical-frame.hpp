// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/bake.hpp"
#include "dome/material-bindings.hpp"
#include "dome/tangents.hpp"
#include "normal-frame-runtime.hpp"
namespace dome::detail {
// Sufficient conservative subset shared with the existing export contract.
// No replacement of curved/artist direction channels or mixed-joint skin.
inline bool constant_bake_frame(const NormalFrames &frame,const Mesh &mesh,
                                const DeformationState *deformation,bool check_skin=true,
                                const std::function<bool()> &cancelled={}) {
  if(frame.normals.size()!=mesh.indices.size()||frame.tangents.size()!=mesh.indices.size())return false;
  size_t work=0;auto check=[&]{if((++work&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Canonical bake validation cancelled");};
  for(const auto &target:frame.targets)for(const auto *channel:{&target.normals,&target.tangents})
    for(auto v:*channel){check();if(v.x!=0||v.y!=0||v.z!=0)return false;}
  for(size_t c=0;c<mesh.indices.size();c+=3){auto n=frame.normals[c];auto t=frame.tangents[c];
    check();
    if(std::abs(dot(n,Vec3{t[0],t[1],t[2]}))>1e-6f)return false;
    for(size_t k=1;k<3;++k){auto v=frame.normals[c+k];if(v.x!=n.x||v.y!=n.y||v.z!=n.z||frame.tangents[c+k]!=t)return false;}
    if(check_skin&&deformation&&deformation->skin){std::optional<uint32_t> joint;
      for(size_t k=0;k<3;++k){const auto &w=deformation->skin->vertices.at(mesh.indices[c+k]);if(w.size()!=1||w[0].weight!=1)return false;if(joint&&*joint!=w[0].joint)return false;joint=w[0].joint;}
    }
  }
  return true;
}
inline bool capture_new_bake_frame(SculptObject &target,const std::function<bool()> &cancelled) {
  auto binding=target.material_bindings;
  if(!binding||!binding->normal_frames||binding->normal_frames->canonical||
     (target.painted->baked()&&!target.painted->baked()->maps.normal.rgba.empty()))return false;
  normal_frame_current_authority(*binding,target.painted->mesh(),target.deformation.get(),cancelled);
  const auto selected=binding->normal_frames->pbr;const auto &mesh=target.painted->mesh();
  if(!selected||!constant_bake_frame(*selected,mesh,target.deformation.get(),true,cancelled))return false;
  auto tangents=tangent_corners(mesh,cancelled);
  for(size_t c=0;c<mesh.indices.size();++c){if((c&1023)==0&&cancelled&&cancelled())throw std::runtime_error("Canonical bake capture cancelled");
    auto n=mesh.normals[mesh.indices[c]],a=selected->normals[c];
    if(std::abs(dot(n,n)-1)>1e-6f||n.x!=a.x||n.y!=a.y||n.z!=a.z||tangents[c]!=selected->tangents[c])return false;
  }
  auto canonical=std::make_shared<NormalFrames>(*selected);canonical->transport=NormalFrameTransport::CanonicalMikk;
  canonical->generated_tangents=false;canonical->generated_uv.reset();
  // The copied directions have just been verified against actual Mikk output;
  // keep this captured basis, not a reinterpretation of any historical payload.
  auto set=std::make_shared<NormalFrameSet>(*binding->normal_frames);set->canonical=std::move(canonical);
  auto next=std::make_shared<SourceMaterialBindings>(*binding);next->normal_frames=std::move(set);next->validate();
  target.material_bindings=std::move(next);return true;
}
inline std::optional<Mesh> retained_bake_basis(const SculptObject &target,BakeOptions &options) {
  const auto &mesh=target.painted->mesh();auto binding=target.material_bindings;
  if(!binding||!binding->normal_frames||!binding->normal_frames->canonical)return {};
  const auto &set=*binding->normal_frames;const auto &frame=*set.canonical;
  if(!constant_bake_frame(frame,mesh,target.deformation.get(),false,options.cancelled))
    throw std::invalid_argument("Retained canonical directions require an explicit supported bake basis; original data retained");
  auto layout=normal_frame_layout(mesh,set,options.cancelled);Mesh low;low.indices=std::move(layout.indices);
  for(size_t v=0;v<layout.vertices.size();++v){auto source=layout.vertices[v],corner=layout.corners[v];low.positions.push_back(mesh.positions[source]);low.normals.push_back(frame.normals[corner]);low.uv.push_back(mesh.uv[source]);options.low_tangents.push_back(frame.tangents[corner]);}
  low.validate();return std::optional<Mesh>(std::move(low));
}
} // namespace dome::detail
