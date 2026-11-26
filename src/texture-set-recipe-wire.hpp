// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/bake-recipe.hpp"
#include <nlohmann/json.hpp>
namespace dome::detail {
// Metadata wraps an ordinary member record by move. Readers inspect its grid
// through this const reference; stripping metadata must never clone a large
// paint JSON tree. The recipe cannot mint source/normal/material capability.
inline const nlohmann::json &texture_set_recipe_member(const nlohmann::json &record){
 if(!record.is_object()||(!record.contains("field_kind")&&!record.contains("field_recipe")))return record;
 if(record.size()!=5||record.contains("field_source")||!record.at("version").is_number_integer()||record.at("version")!=7||
    !record.contains("field_kind")||record.at("field_kind")!="bake_recipe"||!record.contains("field_recipe")||
    !record.at("field_recipe").is_object()||!record.at("set").is_number_integer()||record.at("set").get<int64_t>()<0||
    !record.at("payload").is_object())throw std::invalid_argument("Incomplete texture-set recipe field envelope");
 const auto &member=record.at("payload");
 if(member.size()!=3||!member.at("version").is_number_integer()||member.at("version")!=7||
    !member.at("set").is_number_integer()||member.at("set").get<int64_t>()<0||member.at("set")!=record.at("set")||
    !member.at("payload").is_object())throw std::invalid_argument("Texture-set recipe lost its actual ordinary member record");
 (void)ObjectBakeRecipe::decode(record.at("field_recipe"));
 return member;
}
}
