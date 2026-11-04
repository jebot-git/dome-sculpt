// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/bake-recipe.hpp"
#include "dome/polygon-scene.hpp"
#include <map>
#include <functional>
namespace dome::detail {
inline std::shared_ptr<const ObjectBakeRecipe> remap_bake_recipe(
    const std::shared_ptr<const ObjectBakeRecipe> &recipe,
    const std::map<uint64_t,uint64_t> &objects,bool missing_unavailable){
  if(!recipe)return {};
  auto remap=[&](uint64_t id){
    if(id==unavailable_bake_object)return id;
    const auto found=objects.find(id);
    return found==objects.end()?(missing_unavailable?unavailable_bake_object:id):found->second;
  };
  const auto source=remap(recipe->source.object);
  const auto cage=recipe->cage?remap(recipe->cage->object):0;
  if(source==recipe->source.object&&cage==recipe->settings.cage_object)return recipe;
  auto result=std::make_shared<ObjectBakeRecipe>(*recipe);result->source.object=source;
  if(result->cage)result->cage->object=cage;
  result->settings.cage_object=cage;result->validate();return result;
}
// Applied to detached Scenes only. Shared retained histories are copied only
// when one of their current-namespace recipe references actually changes.
inline void remap_scene_bake_recipes(Scene &scene,const std::map<uint64_t,uint64_t> &objects,
    bool missing_unavailable,const std::function<bool()> &cancelled={}){
  std::map<const PolygonOrigin*,std::shared_ptr<const PolygonOrigin>> origins;
  std::map<const PolygonAuthority*,std::shared_ptr<const PolygonAuthority>> authorities;
  auto stop=[&]{if(cancelled&&cancelled())throw std::runtime_error("Bake recipe remapping cancelled");};
  std::function<std::shared_ptr<const PolygonOrigin>(std::shared_ptr<const PolygonOrigin>,size_t)> origin;
  std::function<std::shared_ptr<const PolygonAuthority>(std::shared_ptr<const PolygonAuthority>,size_t)> authority;
  authority=[&](auto p,size_t depth)->std::shared_ptr<const PolygonAuthority>{
    stop();if(!p)return {};if(depth>32||authorities.size()>512)throw std::length_error("Bake recipe history exceeds native bounds");
    if(auto found=authorities.find(p.get());found!=authorities.end())return found->second;
    auto before=origin(p->origin,depth+1);std::shared_ptr<const PolygonAuthority> result=p;
    if(before!=p->origin){auto copy=std::make_shared<PolygonAuthority>(*p);copy->origin=std::move(before);result=std::move(copy);}
    authorities.emplace(p.get(),result);return result;
  };
  origin=[&](auto p,size_t depth)->std::shared_ptr<const PolygonOrigin>{
    stop();if(!p)return {};if(depth>32||origins.size()>512)throw std::length_error("Bake recipe history exceeds native bounds");
    if(auto found=origins.find(p.get());found!=origins.end())return found->second;
    const auto recipe=remap_bake_recipe(p->bake_recipe,objects,missing_unavailable);
    const auto polygon=authority(p->polygon,depth+1);
    const auto shapes=origin(p->shape_source,depth+1),rig=origin(p->rig_source,depth+1);
    std::shared_ptr<const PolygonOrigin> result=p;
    if(recipe!=p->bake_recipe||polygon!=p->polygon||shapes!=p->shape_source||rig!=p->rig_source){
      auto copy=std::make_shared<PolygonOrigin>(*p);copy->bake_recipe=recipe;copy->polygon=polygon;
      copy->shape_source=shapes;copy->rig_source=rig;result=std::move(copy);
    }
    origins.emplace(p.get(),result);return result;
  };
  for(const auto &entry:scene.objects()){
    stop();auto &o=scene.object(entry.id);o.bake_recipe=remap_bake_recipe(o.bake_recipe,objects,missing_unavailable);
    o.polygon=authority(o.polygon,1);o.shape_source=origin(o.shape_source,1);o.rig_source=origin(o.rig_source,1);
  }
}
} // namespace dome::detail
