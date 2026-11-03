// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <nlohmann/json.hpp>
namespace dome::detail {
// HDR is a separate capability over an actual historical table shape. A higher
// wire number is not permission to interpret unknown key/frame/source layouts.
inline const nlohmann::json &material_archive_base_version(const nlohmann::json &input){
  if(!input.is_object()||!input.contains("version")||!input.at("version").is_number_integer())
    throw std::invalid_argument("Invalid material archive version");
  const auto &wire=input.at("version");
  if(wire==11){
    if(!input.contains("base_version")||!input.at("base_version").is_number_integer()||
       (input.at("base_version").is_number_unsigned()?input.at("base_version").get<uint64_t>()<1||input.at("base_version").get<uint64_t>()>9:input.at("base_version").get<int64_t>()<1||input.at("base_version").get<int64_t>()>9))
      throw std::invalid_argument("HDR material11 requires an explicitly supported base layout");
    return input.at("base_version");
  }
  if(input.contains("base_version"))throw std::invalid_argument("Material base layout requires HDR capability11");
  return wire;
}
inline bool physical_material_archive(const nlohmann::json &input){
  material_archive_base_version(input);return input.at("version")==11;
}
} // namespace dome::detail
