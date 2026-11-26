// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/bake-recipe.hpp"
#include "dome/texture-sets.hpp"
#include "canonical-paint-history.hpp"
#include "texture-set-field-source.hpp"
#include "native-read-scope.hpp"
#include <set>
namespace dome::detail {
// Keep the source's real live namespace dependencies, not every unrelated
// destination raster in its caller's Scene. Snapshot-only IDs (polygon source
// labels, imported node indices, retopology provenance) are never live lookups.
// Materials/origins/texture fields still own their complete detached histories.
inline Scene emission_source_closure(const Scene &scene,uint64_t source,
    const std::function<bool()> &cancelled={}) {
  size_t work=0;
  auto stop=[&]{NativeReadScope::check();if(++work>64*1024*1024)
      throw std::length_error("HDR source dependency traversal exceeds work budget");
    if(cancelled&&cancelled())throw std::runtime_error("HDR source dependency capture cancelled");};
  stop();scene.object(source);
  std::set<uint64_t> available,selected{source};std::vector<uint64_t> pending{source};
  for(const auto &object:scene.objects())available.insert(object.id);
  SourceMaterialArchive materials;PolygonSceneArchive polygons;
  std::set<const TextureSetCollection*> collections;
  std::set<const TextureSetFieldSource*> fields;
  auto reference=[&](uint64_t id){
    stop();if(!id||id==unavailable_bake_object)return;
    if(!available.contains(id))throw std::invalid_argument("HDR source has a missing live provenance dependency");
    if(selected.insert(id).second){if(selected.size()>256)throw std::length_error("HDR source object closure exceeds limit");pending.push_back(id);}
  };
  auto recipe=[&](const std::shared_ptr<const ObjectBakeRecipe> &value){
    stop();if(!value)return;value->validate();reference(value->source.object);reference(value->settings.cage_object);
  };
  auto field=[&](const std::shared_ptr<const TextureSetFieldSource> &value){
    stop();if(!value||!fields.insert(value.get()).second)return;
    if(fields.size()>256)throw std::length_error("HDR source texture-field closure exceeds limit");
    value->authenticate_owner(value);const auto &view=value->field();recipe(view.bake_recipe);
    if(view.material_bindings)materials.add(view.material_bindings,false);
    if(value->polygon_owner())polygons.add_origin(value->polygon_owner()->before(),false);
  };
  auto sets=[&](const std::shared_ptr<const TextureSetCollection> &value){
    stop();if(!value||!collections.insert(value.get()).second)return;
    if(collections.size()>256)throw std::length_error("HDR source texture-set closure exceeds limit");
    field(value->primary_source());
    for(const auto &member:value->additional()){
      stop();recipe(member.bake_recipe);field(member.source);
      if(member.material_bindings)materials.add(member.material_bindings,false);
    }
  };
  size_t object_cursor=0,origin_cursor=0;
  for(;;){
    while(object_cursor<pending.size()){
      stop();const auto &object=scene.object(pending[object_cursor++]);recipe(object.bake_recipe);sets(object.texture_sets);
      if(object.material_bindings)materials.add(object.material_bindings,false);
      if(object.polygon)polygons.add(object.polygon,false);
      if(object.shape_source)polygons.add_origin(object.shape_source,false);
      if(object.rig_source)polygons.add_rig_origin(object.rig_source,false);
    }
    collect_canonical_paint_controls(materials,polygons,false,cancelled);
    while(origin_cursor<polygons.origins().size()){
      stop();const auto before=polygons.origins()[origin_cursor++];recipe(before->bake_recipe);sets(before->texture_sets);
      if(before->material_bindings)materials.add(before->material_bindings,false);
    }
    if(materials.bindings().size()>512||polygons.origins().size()>512)
      throw std::length_error("HDR source retained provenance closure exceeds limit");
    const auto old_materials=materials.bindings().size(),old_origins=polygons.origins().size();
    collect_canonical_paint_controls(materials,polygons,false,cancelled);
    if(object_cursor==pending.size()&&origin_cursor==polygons.origins().size()&&
       old_materials==materials.bindings().size()&&old_origins==polygons.origins().size())break;
  }
  // A shallow Scene copy does not clone the unrelated voxel/raster payloads.
  // Every actual retained recipe edge is in selected, so erase must leave the
  // kept typed histories unchanged; enforce that instead of silently remapping.
  auto result=scene;
  for(const auto &object:scene.objects())if(!selected.contains(object.id)){stop();result.erase(object.id);}
  std::set<uint64_t> groups;
  for(const auto &object:result.objects())for(const auto &group:result.group_path(object.group)){stop();groups.insert(group.id);}
  std::vector<uint64_t> discard;
  for(const auto &group:result.groups())if(!groups.contains(group.id)&&(!group.parent||groups.contains(group.parent)))discard.push_back(group.id);
  for(auto id:discard){stop();result.erase_group(id,false);}
  result.select(source);
  for(auto id:selected){
    stop();const auto &before=scene.object(id),&after=result.object(id);
    if(before.bake_recipe!=after.bake_recipe||before.polygon!=after.polygon||before.shape_source!=after.shape_source||
       before.rig_source!=after.rig_source||before.texture_sets!=after.texture_sets||before.painted!=after.painted||
       before.material_bindings!=after.material_bindings||before.deformation!=after.deformation||
       scene.world_matrix(id).float_matrix()!=result.world_matrix(id).float_matrix())
      throw std::invalid_argument("HDR dependency capture changed genuine source provenance or placement");
  }
  stop();result.validate(false);stop();return result;
}
} // namespace dome::detail
