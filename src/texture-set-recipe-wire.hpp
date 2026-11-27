// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/bake-recipe.hpp"
#include <nlohmann/json.hpp>
namespace dome::detail {
// A member can refer to separately archived metadata. Preflight inspects
// the actual paint grid without copying its potentially large payload.
inline const nlohmann::json &texture_set_metadata_member(const nlohmann::json &record){
 if(!record.is_object()||(!record.contains("field_kind")&&!record.contains("field_recipe")&&
                         !record.contains("field_material")))return record;
 const auto valid_index=[](const nlohmann::json &value,uint64_t maximum){
  return value.is_number_integer()&&
      (value.is_number_unsigned()||value.get<int64_t>()>=0)&&value.get<uint64_t>()<=maximum;
 };
 const bool recipe=record.contains("field_recipe"),material=record.contains("field_material");
 if(record.size()!=4+size_t(recipe)+size_t(material)||record.contains("field_source")||
    !record.at("version").is_number_integer()||record.at("version")!=7||
    record.at("field_kind")!=(material?"mtoon":"bake_recipe")||(!material&&!recipe)||
    (material&&!valid_index(record.at("field_material"),255))||
    (recipe&&!record.at("field_recipe").is_object())||!valid_index(record.at("set"),UINT32_MAX)||
    !record.at("payload").is_object())throw std::invalid_argument("Incomplete texture-set metadata envelope");
 const auto &member=record.at("payload");
 if(member.size()!=3||!member.at("version").is_number_integer()||member.at("version")!=7||
    !valid_index(member.at("set"),UINT32_MAX)||member.at("set")!=record.at("set")||
    !member.at("payload").is_object())throw std::invalid_argument("Texture-set metadata lost its actual ordinary member record");
 if(recipe)(void)ObjectBakeRecipe::decode(record.at("field_recipe"));
 return member;
}
}
