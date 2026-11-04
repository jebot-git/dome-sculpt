// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/bake-recipe.hpp"
#include "dome/polygon-scene.hpp"
#include <map>
#include <nlohmann/json.hpp>
namespace bake_recipe_fixture {
// Independent test-only COW value projection. Retain every native geometry,
// material/paint/normal/key/rig owner and alias; remove only typed bake recipes
// from live objects and their complete polygon/shape/rig history graph. Encoding
// that real value proves the older capability instead of relabeling Scene30.
struct WithoutRecipes {
 std::map<const dome::PolygonOrigin*,std::shared_ptr<const dome::PolygonOrigin>> origins;
 std::map<const dome::PolygonAuthority*,std::shared_ptr<const dome::PolygonAuthority>> polygons;
 size_t removed=0;
 void recipe(std::shared_ptr<const dome::ObjectBakeRecipe> &value){if(value){value->validate();auto encoded=value->encode();if(dome::ObjectBakeRecipe::decode(encoded)->encode()!=encoded)throw std::runtime_error("Independent bake recipe roundtrip differs");++removed;value.reset();}}
 std::shared_ptr<const dome::PolygonOrigin> origin(const std::shared_ptr<const dome::PolygonOrigin> &value){
  if(!value)return {};if(auto found=origins.find(value.get());found!=origins.end())return found->second;
  auto out=std::make_shared<dome::PolygonOrigin>(*value);origins.emplace(value.get(),out);recipe(out->bake_recipe);out->polygon=polygon(value->polygon);out->shape_source=origin(value->shape_source);out->rig_source=origin(value->rig_source);return out;
 }
 std::shared_ptr<const dome::PolygonAuthority> polygon(const std::shared_ptr<const dome::PolygonAuthority> &value){
  if(!value)return {};if(auto found=polygons.find(value.get());found!=polygons.end())return found->second;
  auto out=std::make_shared<dome::PolygonAuthority>(*value);polygons.emplace(value.get(),out);out->origin=origin(value->origin);return out;
 }
 dome::Scene scene(dome::Scene value){for(const auto &object:value.objects()){auto &out=value.object(object.id);recipe(out.bake_recipe);out.polygon=polygon(out.polygon);out.shape_source=origin(out.shape_source);out.rig_source=origin(out.rig_source);}value.validate();return value;}
};
inline nlohmann::json legacy(const dome::Scene &value,unsigned expected){WithoutRecipes omit;auto control=omit.scene(value);auto encoded=control.encode();if(!omit.removed||encoded.at("version")!=expected)throw std::runtime_error("Actual recipe-only omission removed="+std::to_string(omit.removed)+" expected="+std::to_string(expected)+" actual="+encoded.at("version").dump());return encoded;}
inline bool scene30(const dome::Scene &value,unsigned previous,const nlohmann::json &encoded){return encoded.at("version")==30&&legacy(value,previous).at("version")==previous;}
inline bool scene30(const dome::Scene &value,unsigned previous){return scene30(value,previous,value.encode());}
}
