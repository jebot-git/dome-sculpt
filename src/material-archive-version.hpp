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
  if(wire==14){
    if(!input.contains("base_version")||!input.at("base_version").is_number_integer()||input.at("base_version").get<int64_t>()<1||input.at("base_version").get<int64_t>()>10)throw std::invalid_argument("Native paint material14 requires an actual historical base1..10");
    for(const char *field:{"physical_emission","explicit_mirror","canonical_normals","native_paint_sampling"})if(!input.contains(field)||!input.at(field).is_boolean())throw std::invalid_argument("Native paint material14 capability flags are incomplete");
    if(!input.at("native_paint_sampling").get<bool>()||input.at("canonical_normals").get<bool>())throw std::invalid_argument("Native paint14 requires genuine native sampling; canonical31 is not yet composed");
    if(!input.contains("native_paints")||!input.at("native_paints").is_array()||input.at("native_paints").empty()||input.at("native_paints").size()>128)throw std::invalid_argument("Native paint14 lacks its actual bounded owning table");
    return input.at("base_version");
  }
  if(input.contains("native_paint_sampling")||input.contains("native_paints")||input.contains("canonical_normals")||input.contains("explicit_mirror"))throw std::invalid_argument("Native paint capability requires material14");
  if(wire==12){
    if(!input.contains("base_version")||input.at("base_version")!=10||!input.contains("physical_emission")||!input.at("physical_emission").is_boolean())throw std::invalid_argument("Explicit Mirror material12 requires owning base10 and physical capability boolean");
    return input.at("base_version");
  }
  if(input.contains("physical_emission"))throw std::invalid_argument("Physical capability boolean requires material12");
  if(wire==11){
    if(!input.contains("base_version")||!input.at("base_version").is_number_integer()||
       (input.at("base_version").is_number_unsigned()?input.at("base_version").get<uint64_t>()<1||input.at("base_version").get<uint64_t>()>10:input.at("base_version").get<int64_t>()<1||input.at("base_version").get<int64_t>()>10))
      throw std::invalid_argument("HDR material11 requires an explicitly supported base layout");
    return input.at("base_version");
  }
  if(input.contains("base_version"))throw std::invalid_argument("Material base layout requires HDR capability11");
  return wire;
}
inline bool physical_material_archive(const nlohmann::json &input){
  material_archive_base_version(input);return input.at("version")==11||((input.at("version")==12||input.at("version")==14)&&input.at("physical_emission").get<bool>());
}
} // namespace dome::detail

namespace dome::detail {
inline bool explicit_mirror_material_archive(const nlohmann::json &input){material_archive_base_version(input);return input.at("version")==12||(input.at("version")==14&&input.at("explicit_mirror").get<bool>());}
inline bool native_paint_material_archive(const nlohmann::json &input){material_archive_base_version(input);return input.at("version")==14;}
}
