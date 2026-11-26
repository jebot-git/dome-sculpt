// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/bake-recipe.hpp"
#include "dome/polygon-scene.hpp"
#include "texture-set-field-source.hpp"
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
// Recipes share live object references across current and retained fields.
class BakeRecipeRemap {
public:
  BakeRecipeRemap(const std::map<uint64_t, uint64_t> &objects,
                  bool missing_unavailable, const std::function<bool()> &cancelled)
      : objects_(objects), missing_unavailable_(missing_unavailable), cancelled_(cancelled) {}

  std::shared_ptr<const ObjectBakeRecipe>
  recipe(const std::shared_ptr<const ObjectBakeRecipe> &original) {
    if (!original)
      return {};
    if (const auto found = recipes_.find(original.get()); found != recipes_.end())
      return found->second;
    auto result = remap_bake_recipe(original, objects_, missing_unavailable_);
    recipes_.emplace(original.get(), result);
    return result;
  }

  std::shared_ptr<const TextureSetCollection>
  collection(const std::shared_ptr<const TextureSetCollection> &original) {
    if (!original)
      return {};
    if (const auto found = collections_.find(original.get()); found != collections_.end())
      return found->second;
    std::shared_ptr<TextureSetCollection> changed;
    const auto primary = field_source(original->primary_source_);
    if (primary != original->primary_source_) {
      changed.reset(new TextureSetCollection(*original));
      changed->primary_source_ = primary;
    }
    for (size_t i = 0; i < original->additional_.size(); ++i) {
      if (cancelled_ && cancelled_())
        throw std::runtime_error("Texture-set bake recipe remapping cancelled");
      const auto &field = original->additional_[i];
      const auto remapped = recipe(field.bake_recipe);
      if (remapped == field.bake_recipe)
        continue;
      if (!changed)
        changed.reset(new TextureSetCollection(*original));
      changed->additional_[i].bake_recipe = remapped;
      changed->additional_[i].source = field_source(field.source);
    }
    auto result = changed ? std::shared_ptr<const TextureSetCollection>(std::move(changed)) : original;
    collections_.emplace(original.get(), result);
    return result;
  }

private:
  std::shared_ptr<const TextureSetFieldSource>
  field_source(const std::shared_ptr<const TextureSetFieldSource> &original) {
    if (!original)
      return {};
    original->authenticate_owner(original);
    const auto remapped = recipe(original->field_.bake_recipe);
    if (remapped == original->field_.bake_recipe)
      return original;
    if (const auto found = fields_.find(original.get()); found != fields_.end())
      return found->second;

    // Geometry, raster and the validated read view all keep their owners.
    auto copy = std::shared_ptr<TextureSetFieldSource>(new TextureSetFieldSource(*original));
    copy->field_.bake_recipe = remapped;
    std::shared_ptr<const TextureSetFieldSource> result = std::move(copy);
    result->self_ = result;
    fields_.emplace(original.get(), result);
    return result;
  }

  const std::map<uint64_t, uint64_t> &objects_;
  bool missing_unavailable_;
  const std::function<bool()> &cancelled_;
  std::map<const ObjectBakeRecipe *, std::shared_ptr<const ObjectBakeRecipe>> recipes_;
  std::map<const TextureSetCollection *, std::shared_ptr<const TextureSetCollection>> collections_;
  std::map<const TextureSetFieldSource *, std::shared_ptr<const TextureSetFieldSource>> fields_;
};

// Applied to detached Scenes only. Shared retained histories are copied only
// when one of their current-namespace recipe references actually changes.
inline void remap_scene_bake_recipes(Scene &scene,const std::map<uint64_t,uint64_t> &objects,
    bool missing_unavailable,const std::function<bool()> &cancelled={}){
  BakeRecipeRemap remap(objects, missing_unavailable, cancelled);
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
    const auto recipe=remap.recipe(p->bake_recipe);
    const auto sets=remap.collection(p->texture_sets);
    const auto polygon=authority(p->polygon,depth+1);
    const auto shapes=origin(p->shape_source,depth+1),rig=origin(p->rig_source,depth+1);
    std::shared_ptr<const PolygonOrigin> result=p;
    if(recipe!=p->bake_recipe||sets!=p->texture_sets||polygon!=p->polygon||shapes!=p->shape_source||rig!=p->rig_source){
      auto copy=std::make_shared<PolygonOrigin>(*p);copy->bake_recipe=recipe;copy->texture_sets=sets;copy->polygon=polygon;
      copy->shape_source=shapes;copy->rig_source=rig;result=std::move(copy);
    }
    origins.emplace(p.get(),result);return result;
  };
  for(const auto &entry:scene.objects()){
    stop();auto &o=scene.object(entry.id);o.bake_recipe=remap.recipe(o.bake_recipe);
    o.texture_sets=remap.collection(o.texture_sets);
    o.polygon=authority(o.polygon,1);o.shape_source=origin(o.shape_source,1);o.rig_source=origin(o.rig_source,1);
  }
}
} // namespace dome::detail
