// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/bake-recipe.hpp"
#include "dome/polygon-scene.hpp"
#include <map>
#include <functional>
namespace dome::detail {
class CanonicalPaintSampling;
class PolygonSurfaceOwner;
class PolygonSurfacePreview;
class TextureSetFieldSource;
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
  using OriginRemap = std::function<std::shared_ptr<const PolygonOrigin>(
      const std::shared_ptr<const PolygonOrigin> &)>;
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
  collection(const std::shared_ptr<const TextureSetCollection> &original, const OriginRemap &origin);

  std::shared_ptr<const SourceMaterialBindings>
  binding(const std::shared_ptr<const SourceMaterialBindings> &original, const OriginRemap &origin);

private:
  friend struct TextureSetAccess;
  std::shared_ptr<const TextureSetFieldSource>
  field_source(const std::shared_ptr<const TextureSetFieldSource> &original, const OriginRemap &origin);

  std::shared_ptr<const PolygonSurfacePreview>
  preview(const std::shared_ptr<const PolygonSurfacePreview> &original,
          const std::shared_ptr<const SourceMaterialBindings> &binding);

  const std::map<uint64_t, uint64_t> &objects_;
  bool missing_unavailable_;
  const std::function<bool()> &cancelled_;
  std::map<const ObjectBakeRecipe *, std::shared_ptr<const ObjectBakeRecipe>> recipes_;
  std::map<const TextureSetCollection *, std::shared_ptr<const TextureSetCollection>> collections_;
  std::map<const TextureSetFieldSource *, std::shared_ptr<const TextureSetFieldSource>> fields_;
  std::map<const SourceMaterialBindings *, std::shared_ptr<const SourceMaterialBindings>> bindings_;
  std::map<const PolygonSurfaceOwner *, std::shared_ptr<const PolygonSurfaceOwner>> polygon_fields_;
  std::map<const CanonicalPaintSampling *, std::shared_ptr<const CanonicalPaintSampling>> canonical_fields_;
  std::map<const PolygonSurfacePreview *, std::shared_ptr<const PolygonSurfacePreview>> previews_;
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
    const auto polygon=authority(p->polygon,depth+1);
    const BakeRecipeRemap::OriginRemap nested=[&](const auto &before){return origin(before,depth+1);};
    const auto binding=remap.binding(p->material_bindings,nested);
    const auto sets=remap.collection(p->texture_sets,nested);
    const auto shapes=origin(p->shape_source,depth+1),rig=origin(p->rig_source,depth+1);
    std::shared_ptr<const PolygonOrigin> result=p;
    if(recipe!=p->bake_recipe||sets!=p->texture_sets||binding!=p->material_bindings||polygon!=p->polygon||shapes!=p->shape_source||rig!=p->rig_source){
      auto copy=std::make_shared<PolygonOrigin>(*p);copy->bake_recipe=recipe;copy->texture_sets=sets;copy->material_bindings=binding;copy->polygon=polygon;
      copy->shape_source=shapes;copy->rig_source=rig;result=std::move(copy);
    }
    origins.emplace(p.get(),result);return result;
  };
  const BakeRecipeRemap::OriginRemap retained=[&](const auto &before){return origin(before,1);};
  for(const auto &entry:scene.objects()){
    stop();auto &o=scene.object(entry.id);o.bake_recipe=remap.recipe(o.bake_recipe);
    o.material_bindings=remap.binding(o.material_bindings,retained);
    o.texture_sets=remap.collection(o.texture_sets,retained);
    o.polygon=authority(o.polygon,1);o.shape_source=origin(o.shape_source,1);o.rig_source=origin(o.rig_source,1);
  }
}
} // namespace dome::detail
