// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/bake.hpp"
#include "dome/source-placement.hpp"
#include "bake-cage-polygon.hpp"
#include "refined-bake.hpp"
#include <limits>
namespace dome::detail {
// Worker-only: no nearest-position/weld inference. Cage IDs are the explicit
// ordered target vertex IDs, transported through the existing low-basis corners.
inline std::vector<Vec3> object_bake_cage(const Scene &scene,uint64_t target_id,
    uint64_t cage_id,const Mesh &target,const Mesh &low,
    const std::function<bool()> &cancelled,const AffineTransform *metric_from_world=nullptr,
    const Mesh *evaluated_target=nullptr,bool *used_refinement=nullptr) {
  auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Bake cage cancelled");};
  stop();
  if(cage_id==target_id)throw std::invalid_argument("Choose a different editable mesh for the bake cage");
  if(low.positions.size()>2'000'000||low.indices.size()/3>2'000'000)
    throw std::length_error("Low bake mesh exceeds surface limits");
  const auto &object=scene.object(cage_id);
  if(!object.mesh_active||!object.painted)
    throw std::invalid_argument("The selected bake cage is no longer a prepared mesh; reselect it");
  const auto &rest=object.painted->mesh();
  if(low.indices.size()!=target.indices.size())
    throw std::invalid_argument("Low bake basis lost the ordered target corners");
  const auto &sample_target=evaluated_target?*evaluated_target:target;
  if(evaluated_target&&(sample_target.positions.size()!=target.positions.size()||
                       sample_target.indices!=target.indices))
    throw std::invalid_argument("Evaluated bake target changed its cage vertex identities");
  std::optional<Mesh> posed;
  if(object.deformation){stop();posed=object.deformation->preview(rest);stop();}
  const auto &visible=posed?*posed:rest;
  if(visible.positions.size()!=rest.positions.size()||visible.indices!=rest.indices)
    throw std::invalid_argument("Evaluated bake cage changed its vertex identities");
  const bool ordered=same_bake_order(rest,target);
  auto refinement=ordered?std::nullopt:replay_refined_bake(scene.object(target_id),object,true,cancelled);
  if(used_refinement)*used_refinement=bool(refinement);
  const auto &cage_basis=refinement?*refinement->original:target;
  const bool ordered_basis=same_bake_order(rest,cage_basis);
  std::vector<uint32_t> polygon_identity;
  if(!ordered_basis)polygon_identity=polygon_bake_cage_vertices(scene.object(target_id),object,cage_basis,visible,cancelled);
  std::vector<uint32_t> refined_corner;
  if(refinement)refined_corner.resize(target.positions.size(),UINT32_MAX);
  const auto relative=(metric_from_world?*metric_from_world:scene.world_matrix(target_id).inverse())*
      scene.world_matrix(cage_id);
  std::vector<uint32_t> identity(low.positions.size(),UINT32_MAX);
  for(size_t c=0;c<target.indices.size();++c){
    if((c&1023)==0)stop();
    const auto original=target.indices[c],v=low.indices[c];
    if(v>=low.positions.size()||original>=target.positions.size())
      throw std::invalid_argument("Bake cage basis contains an invalid corner identity");
    if(identity[v]!=UINT32_MAX&&identity[v]!=original)
      throw std::invalid_argument("Low bake basis merges distinct target cage identities");
    const auto a=low.positions[v],b=sample_target.positions[original];
    if(a.x!=b.x||a.y!=b.y||a.z!=b.z)
      throw std::invalid_argument("Low bake basis changed the target positions used for the cage");
    identity[v]=original;
    if(refinement&&refined_corner[original]==UINT32_MAX)refined_corner[original]=uint32_t(c);
  }
  // An unchanged target may retain unused vertices. Their identities are also
  // explicit; a split/retained basis must instead witness every output vertex.
  const bool unchanged=&low==&sample_target;
  std::vector<Vec3> positions;positions.reserve(low.positions.size());
  auto coordinate=[&](uint32_t original){
    const auto index=ordered_basis?original:
      (original<polygon_identity.size()?polygon_identity[original]:UINT32_MAX);
    if(index>=visible.positions.size())
      throw std::invalid_argument("Low bake basis has an unwitnessed cage vertex");
    return visible.positions[index];
  };
  for(size_t v=0;v<low.positions.size();++v){
    if((v&1023)==0)stop();
    const auto original=unchanged?uint32_t(v):identity[v];
    if(original==UINT32_MAX)
      throw std::invalid_argument("Low bake basis has an unwitnessed cage vertex");
    AffineVector p;
    if(refinement){
      const auto corner=refined_corner[original];
      if(corner>=refinement->corners.size())throw std::invalid_argument("Refined cage vertex lacks an original corner");
      const auto &sample=refinement->corners[corner];
      p={};
      for(size_t k=0;k<3;++k){
        if(sample.barycentric[k]==0)continue;
        const auto q=coordinate(cage_basis.indices[sample.triangle*3+k]);
        p[0]+=sample.barycentric[k]*q.x;p[1]+=sample.barycentric[k]*q.y;p[2]+=sample.barycentric[k]*q.z;
      }
    }else{
      const auto q=coordinate(original);
      p={q.x,q.y,q.z};
    }
    const auto q=relative.point(p);Vec3 converted;
    float *axes[]{&converted.x,&converted.y,&converted.z};
    for(size_t axis=0;axis<3;++axis){
      if(!std::isfinite(q[axis])||std::abs(q[axis])>std::numeric_limits<float>::max())
        throw std::invalid_argument("Bake cage placement exceeds finite target coordinates");
      *axes[axis]=float(q[axis]);
      if(q[axis]!=0&&*axes[axis]==0)
        throw std::invalid_argument("Bake cage placement underflows target coordinates");
    }
    positions.push_back(converted);
  }
  stop();return positions;
}
} // namespace dome::detail
