// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/bake-recipe.hpp"
#include "dome/scene.hpp"
#include <nlohmann/json.hpp>
namespace bake_fixture {
// Encode the actual independently owned source scene, so a target's new outer
// capability cannot change which optional fields are written on its neighbours.
// No source field is filtered or normalized by name/version.
inline dome::Scene isolated_scene(dome::Scene scene,uint64_t id){
 std::vector<uint64_t> remove;for(const auto &object:scene.objects())if(object.id!=id)remove.push_back(object.id);
 for(auto other:remove)scene.erase(other);scene.select(id);return scene;
}
inline nlohmann::json isolated_object(dome::Scene scene,uint64_t id){return isolated_scene(std::move(scene),id).encode().at("objects").at(0);}
}
