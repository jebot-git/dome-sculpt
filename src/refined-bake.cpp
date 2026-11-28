// SPDX-License-Identifier: GPL-3.0-or-later
#include "refined-bake.hpp"
#include "dome/bake-recipe.hpp"
#include "dome/polygon-scene.hpp"
#include "refined-mirror-topology.hpp"

namespace dome::detail {
namespace {
void stop(const std::function<bool()> &cancelled){
  if(cancelled&&cancelled())throw std::runtime_error("Refined Bake correspondence cancelled");
}
[[noreturn]] void stale(){
  throw std::invalid_argument("Saved Bake refinement no longer matches the target or the selected mesh's ordered topology");
}
bool same_geometry(const Mesh &a,const Mesh &b,const std::function<bool()> &cancelled){
  if(!same_bake_order(a,b))return false;
  for(size_t v=0;v<a.positions.size();++v){
    if((v&1023)==0)stop(cancelled);
    const auto p=a.positions[v],q=b.positions[v];
    if(p.x!=q.x||p.y!=q.y||p.z!=q.z)return false;
  }
  return true;
}
bool references(const ObjectBakeRecipe *recipe,uint64_t dependency,bool cage){
  if(!recipe)return false;
  return cage?recipe->cage&&recipe->cage->object==dependency:
      recipe->settings.corresponding_topology&&recipe->source.object==dependency;
}
struct Step {
  std::shared_ptr<const Mesh> before,after;
  BakeTopologyRefinement refinement;
};
}

std::optional<RefinedBakeMap> replay_refined_bake(const SculptObject &target,
    const SculptObject &dependency,bool cage,const std::function<bool()> &cancelled){
  const auto *recipe=target.bake_recipe.get();
  if(!references(recipe,dependency.id,cage)||!recipe->refinement)return {};
  const auto &dependency_mesh=dependency.painted->mesh();
  const auto *polygon_origin=cage&&dependency.polygon?dependency.polygon->origin.get():nullptr;
  auto after=target.painted->mesh_owner();
  auto prior=target.shape_source;
  std::vector<Step> steps;
  do{
    stop(cancelled);
    if(!references(recipe,dependency.id,cage)||!recipe->refinement||
       recipe->refinement->after!=morph_topology_id(*after))stale();
    const auto refinement=*recipe->refinement;
    while(prior&&prior->mesh_active&&prior->paint&&same_geometry(prior->paint->mesh(),*after,cancelled))
      prior=prior->shape_source;
    if(!prior||!prior->mesh_active||!prior->paint||!references(prior->bake_recipe.get(),dependency.id,cage))stale();
    auto before=prior->paint->mesh_owner();
    if(refinement.before!=morph_topology_id(*before))stale();
    steps.push_back({before,after,refinement});
    after=std::move(before);
    recipe=prior->bake_recipe.get();
    prior=prior->shape_source;
  }while(!same_bake_order(*after,dependency_mesh)&&
      !(polygon_origin&&polygon_origin->paint&&same_geometry(*after,polygon_origin->paint->mesh(),cancelled)));

  RefinedBakeMap result;
  result.original=after;
  KeyMirrorLimits limits;
  limits.cancelled=cancelled;
  for(const auto &step:steps){
    constexpr size_t max_bytes=128*1024*1024;
    limits.max_bytes=max_bytes-result.corners.capacity()*sizeof(MirrorSurfacePoint);
    const auto replay=refine_authoring_key(step.before,{step.refinement.plane},step.refinement.tolerance,limits);
    limits.max_work-=replay.work_units;
    const size_t detached=replay.corners.capacity()*sizeof(MirrorRefinementCorner)+
        replay.opposite.capacity()*sizeof(uint32_t);
    if(detached>=limits.max_bytes)throw std::length_error("Refined Bake replay exceeds its working budget");
    if(!same_geometry(index_mirror_refinement(replay,cancelled,limits.max_bytes-detached).mesh,*step.after,cancelled))stale();
    if(result.corners.empty()){
      if(replay.corners.size()>(limits.max_bytes-detached)/sizeof(MirrorSurfacePoint))
        throw std::length_error("Refined Bake map exceeds its working budget");
      result.corners.reserve(replay.corners.size());
      for(const auto &corner:replay.corners){
        if((result.corners.size()&1023)==0)stop(cancelled);
        result.corners.push_back(corner.current);
      }
    }else for(size_t c=0;c<result.corners.size();++c){
      if((c&1023)==0)stop(cancelled);
      auto &output=result.corners[c];
      const auto base=size_t(output.triangle)*3;
      MirrorSurfacePoint lifted{replay.corners.at(base).current.triangle,{}};
      for(size_t k=0;k<3;++k){
        const auto &point=replay.corners.at(base+k).current;
        if(lifted.triangle!=point.triangle)stale();
        for(size_t axis=0;axis<3;++axis)
          lifted.barycentric[axis]+=output.barycentric[k]*point.barycentric[axis];
      }
      output=lifted;
    }
  }
  return result;
}
}
