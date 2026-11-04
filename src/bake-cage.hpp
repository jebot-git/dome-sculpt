// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/bake.hpp"
#include "dome/source-placement.hpp"
#include <limits>
namespace dome::detail {
// Worker-only: no nearest-position/weld inference. Cage IDs are the explicit
// ordered target vertex IDs, transported through the existing low-basis corners.
inline std::vector<Vec3> object_bake_cage(const Scene &scene,uint64_t target_id,
    uint64_t cage_id,const Mesh &target,const Mesh &low,
    const std::function<bool()> &cancelled) {
  auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Bake cage cancelled");};
  stop();
  if(cage_id==target_id)throw std::invalid_argument("Choose a different editable mesh for the bake cage");
  if(low.positions.size()>2'000'000||low.indices.size()/3>2'000'000)
    throw std::length_error("Low bake mesh exceeds surface limits");
  const auto &object=scene.object(cage_id);
  if(!object.mesh_active||!object.painted)
    throw std::invalid_argument("The selected bake cage is no longer a prepared mesh; reselect it");
  const auto &rest=object.painted->mesh();
  if(rest.positions.size()!=target.positions.size()||rest.indices.size()!=target.indices.size())
    throw std::invalid_argument("Bake cage must share the target's ordered vertex and triangle identities");
  for(size_t c=0;c<target.indices.size();++c){
    if((c&1023)==0)stop();
    if(rest.indices[c]!=target.indices[c])
      throw std::invalid_argument("Bake cage triangle identities differ from the target; duplicate the target before editing cage vertices");
  }
  if(low.indices.size()!=target.indices.size())
    throw std::invalid_argument("Low bake basis lost the ordered target corners");
  std::optional<Mesh> posed;
  if(object.deformation){stop();posed=object.deformation->preview(rest);stop();}
  const auto &visible=posed?*posed:rest;
  if(visible.positions.size()!=rest.positions.size()||visible.indices!=rest.indices)
    throw std::invalid_argument("Evaluated bake cage changed its vertex identities");
  const auto relative=scene.world_matrix(target_id).inverse()*scene.world_matrix(cage_id);
  std::vector<uint32_t> identity(low.positions.size(),UINT32_MAX);
  for(size_t c=0;c<target.indices.size();++c){
    if((c&1023)==0)stop();
    const auto original=target.indices[c],v=low.indices[c];
    if(v>=low.positions.size()||original>=target.positions.size())
      throw std::invalid_argument("Bake cage basis contains an invalid corner identity");
    if(identity[v]!=UINT32_MAX&&identity[v]!=original)
      throw std::invalid_argument("Low bake basis merges distinct target cage identities");
    const auto a=low.positions[v],b=target.positions[original];
    if(a.x!=b.x||a.y!=b.y||a.z!=b.z)
      throw std::invalid_argument("Low bake basis moved the target cage rest positions");
    identity[v]=original;
  }
  // An unchanged target may retain unused vertices. Their identities are also
  // explicit; a split/retained basis must instead witness every output vertex.
  const bool unchanged=&low==&target;
  std::vector<Vec3> positions;positions.reserve(low.positions.size());
  for(size_t v=0;v<low.positions.size();++v){
    if((v&1023)==0)stop();
    const auto original=unchanged?uint32_t(v):identity[v];
    if(original==UINT32_MAX||original>=visible.positions.size())
      throw std::invalid_argument("Low bake basis has an unwitnessed cage vertex");
    const auto p=visible.positions[original];
    const auto q=relative.point({p.x,p.y,p.z});Vec3 converted;
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
