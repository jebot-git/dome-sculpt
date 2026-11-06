// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
namespace dome::detail {
// HDR is a separate capability over an actual historical table shape. A higher
// wire number is not permission to interpret unknown key/frame/source layouts.
inline const nlohmann::json &material_archive_base_version(const nlohmann::json &input){
  if(!input.is_object()||!input.contains("version")||!input.at("version").is_number_integer())
    throw std::invalid_argument("Invalid material archive version");
  const auto &wire=input.at("version");
  if(wire!=21&&input.contains("polygon_surface_transfer"))throw std::invalid_argument("Polygon surface capability requires actual material21");
  if(wire==21){
    if(!input.contains("base_version")||!input.at("base_version").is_number_integer()||input.at("base_version").get<int64_t>()<1||input.at("base_version").get<int64_t>()>10)throw std::invalid_argument("Material21 requires its actual historical base layout");
    for(const char*field:{"physical_emission","explicit_mirror","canonical_normals","native_paint_sampling","layered_normals","retained_canonical_uv","native_paint_frames","canonical_paint_sampling","polygon_surface_transfer"})if(!input.contains(field)||!input.at(field).is_boolean())throw std::invalid_argument("Material21 actual capability flags are incomplete");
    if(!input.at("polygon_surface_transfer").get<bool>()||(input.at("native_paint_frames").get<bool>()&&!input.at("native_paint_sampling").get<bool>())||(input.at("retained_canonical_uv").get<bool>()&&!input.at("layered_normals").get<bool>())||(input.at("canonical_paint_sampling").get<bool>()&&(!input.at("layered_normals").get<bool>()||!input.at("retained_canonical_uv").get<bool>()))||(input.at("explicit_mirror").get<bool>()&&input.at("base_version")!=10))throw std::invalid_argument("Material21 declared capabilities do not compose their actual owners");
    for(const auto&entry:{std::pair{"native_paint_sampling","native_paints"},std::pair{"native_paint_frames","native_frame_sources"},std::pair{"canonical_normals","canonical_conversions"}}){const bool active=input.at(entry.first).get<bool>();if(input.contains(entry.second)!=active||(active&&(!input.at(entry.second).is_array()||input.at(entry.second).empty()||input.at(entry.second).size()>256)))throw std::invalid_argument("Material21 owning table differs from its actual feature");}
    if(!input.at("bindings").is_array()||input.at("bindings").empty()||input.at("bindings").size()>256||!std::any_of(input.at("bindings").begin(),input.at("bindings").end(),[](const auto&b){return b.contains("polygon_surface_transfer")&&!b.at("polygon_surface_transfer").is_null();}))throw std::invalid_argument("Material21 requires genuine polygon field references");
    return input.at("base_version");
  }

  if(wire!=19&&input.contains("canonical_paint_sampling"))throw std::invalid_argument("Canonical paint capability requires material19");
  if(wire==19){
    if(!input.contains("base_version")||!input.at("base_version").is_number_integer()||input.at("base_version").get<int64_t>()<1||input.at("base_version").get<int64_t>()>10)throw std::invalid_argument("Material19 requires its actual historical layout");
    for(const char *field:{"physical_emission","explicit_mirror","canonical_normals","native_paint_sampling","layered_normals","retained_canonical_uv","native_paint_frames","canonical_paint_sampling"})
      if(!input.contains(field)||!input.at(field).is_boolean())throw std::invalid_argument("Material19 capability flags are incomplete");
    if(!input.at("canonical_paint_sampling").get<bool>()||!input.at("layered_normals").get<bool>()||!input.at("retained_canonical_uv").get<bool>()||
       (input.at("native_paint_frames").get<bool>()&&!input.at("native_paint_sampling").get<bool>())||
       (input.at("explicit_mirror").get<bool>()&&input.at("base_version")!=10))throw std::invalid_argument("Material19 lacks actual canonical writable/normal context");
    for(const auto &entry:{std::pair{"native_paint_sampling","native_paints"},std::pair{"native_paint_frames","native_frame_sources"},std::pair{"canonical_normals","canonical_conversions"}}){
      const bool active=input.at(entry.first).get<bool>();
      if(input.contains(entry.second)!=active||(active&&(!input.at(entry.second).is_array()||input.at(entry.second).empty()||input.at(entry.second).size()>256)))throw std::invalid_argument("Material19 owning pool differs from its actual capability");
    }
    if(!input.at("bindings").is_array()||input.at("bindings").empty()||input.at("bindings").size()>256||
       !std::any_of(input.at("bindings").begin(),input.at("bindings").end(),[](const auto &b){return b.contains("canonical_paint_sampling")&&!b.at("canonical_paint_sampling").is_null();}))throw std::invalid_argument("Material19 requires genuine canonical paint references");
    return input.at("base_version");
  }
  if(wire!=18&&(input.contains("native_frame_sources")||input.contains("native_paint_frames")))throw std::invalid_argument("Native frame capture requires material18");
  if(wire==18){
    if(!input.contains("base_version")||!input.at("base_version").is_number_integer()||input.at("base_version").get<int64_t>()<1||input.at("base_version").get<int64_t>()>10)throw std::invalid_argument("Material18 requires its actual historical layout");
    for(const char *field:{"physical_emission","explicit_mirror","canonical_normals","native_paint_sampling","layered_normals","retained_canonical_uv","native_paint_frames"})if(!input.contains(field)||!input.at(field).is_boolean())throw std::invalid_argument("Material18 capability flags incomplete");
    if(!input.at("native_paint_frames").get<bool>()||!input.at("native_paint_sampling").get<bool>()||(input.at("retained_canonical_uv").get<bool>()&&!input.at("layered_normals").get<bool>())||(input.at("explicit_mirror").get<bool>()&&input.at("base_version")!=10))throw std::invalid_argument("Material18 lacks genuine native frame/paint context");
    for(const char *pool:{"native_paints","native_frame_sources"})if(!input.contains(pool)||!input.at(pool).is_array()||input.at(pool).empty()||input.at(pool).size()>128)throw std::invalid_argument("Material18 requires bounded owning native pools");
    if(input.contains("canonical_conversions")!=input.at("canonical_normals").get<bool>()||(input.at("canonical_normals").get<bool>()&&(!input.at("canonical_conversions").is_array()||input.at("canonical_conversions").empty()||input.at("canonical_conversions").size()>256)))throw std::invalid_argument("Material18 canonical capability differs from actual pool");
    return input.at("base_version");
  }
  if(wire==17){
    if(!input.contains("base_version")||!input.at("base_version").is_number_integer()||input.at("base_version").get<int64_t>()<1||input.at("base_version").get<int64_t>()>10)
      throw std::invalid_argument("Combined material17 requires its real historical base layout");
    for(const char *field:{"physical_emission","explicit_mirror","canonical_normals","native_paint_sampling","layered_normals","retained_canonical_uv"})
      if(!input.contains(field)||!input.at(field).is_boolean())throw std::invalid_argument("Combined material17 has incomplete capability flags");
    if(!input.at("native_paint_sampling").get<bool>()||(!input.at("layered_normals").get<bool>()&&!input.at("canonical_normals").get<bool>())||
       (input.at("retained_canonical_uv").get<bool>()&&!input.at("layered_normals").get<bool>())||
       (input.at("explicit_mirror").get<bool>()&&input.at("base_version")!=10))
      throw std::invalid_argument("Combined material17 requires genuine native paint and canonical ownership");
    if(!input.contains("native_paints")||!input.at("native_paints").is_array()||input.at("native_paints").empty()||input.at("native_paints").size()>128)
      throw std::invalid_argument("Combined material17 requires its bounded native paint owners");
    if(input.contains("canonical_conversions")!=input.at("canonical_normals").get<bool>()||
       (input.at("canonical_normals").get<bool>()&&(!input.at("canonical_conversions").is_array()||input.at("canonical_conversions").empty()||input.at("canonical_conversions").size()>256)))
      throw std::invalid_argument("Combined material17 canonical pool differs from its capability");
    return input.at("base_version");
  }
  if(wire==14){
    if(!input.contains("base_version")||!input.at("base_version").is_number_integer()||input.at("base_version").get<int64_t>()<1||input.at("base_version").get<int64_t>()>10)throw std::invalid_argument("Native paint material14 requires its actual historical base1..10");
    for(const char *field:{"physical_emission","explicit_mirror","canonical_normals","native_paint_sampling"})if(!input.contains(field)||!input.at(field).is_boolean())throw std::invalid_argument("Native paint material14 capability flags are incomplete");
    if(!input.at("native_paint_sampling").get<bool>()||input.at("canonical_normals").get<bool>()||input.contains("canonical_conversions")||input.contains("layered_normals")||input.contains("retained_canonical_uv"))throw std::invalid_argument("Native paint14 requires its exact independent native sampling capability");
    if(!input.contains("native_paints")||!input.at("native_paints").is_array()||input.at("native_paints").empty()||input.at("native_paints").size()>128)throw std::invalid_argument("Native paint14 lacks its actual bounded owning table");
    return input.at("base_version");
  }
  if(input.contains("native_paint_sampling")||input.contains("native_paints"))throw std::invalid_argument("Native paint capability requires material14 or composed17");
  if(wire==16){
    if(!input.contains("retained_canonical_uv")||input.at("retained_canonical_uv")!=true)throw std::invalid_argument("Material16 requires explicit retained canonical UV capability");
  }else if(input.contains("retained_canonical_uv"))throw std::invalid_argument("Retained canonical UV capability requires material16");
  if(wire==15||wire==16){
    if(!input.contains("base_version")||!input.at("base_version").is_number_integer()||
       input.at("base_version").get<int64_t>()<1||input.at("base_version").get<int64_t>()>10||
       !input.contains("layered_normals")||input.at("layered_normals")!=true||
       !input.contains("physical_emission")||!input.at("physical_emission").is_boolean()||
       !input.contains("explicit_mirror")||!input.at("explicit_mirror").is_boolean()||
       !input.contains("canonical_normals")||!input.at("canonical_normals").is_boolean()||
       (input.at("explicit_mirror").get<bool>()&&input.at("base_version")!=10)||
       input.contains("canonical_conversions")!=input.at("canonical_normals").get<bool>())
      throw std::invalid_argument("Layered material15 requires its actual base layout and explicit capabilities");
    if(input.at("canonical_normals").get<bool>()&&(!input.at("canonical_conversions").is_array()||
       input.at("canonical_conversions").empty()||input.at("canonical_conversions").size()>256))
      throw std::invalid_argument("Layered material15 legacy canonical pool is incomplete");
    return input.at("base_version");
  }
  if(input.contains("layered_normals"))throw std::invalid_argument("Independent normal capability requires material15");
  if(wire==13){
    if(!input.contains("base_version")||!input.at("base_version").is_number_integer()||
        input.at("base_version").get<int64_t>()<1||input.at("base_version").get<int64_t>()>10||
        !input.contains("physical_emission")||!input.at("physical_emission").is_boolean()||
        !input.contains("explicit_mirror")||!input.at("explicit_mirror").is_boolean()||
        !input.contains("canonical_normals")||input.at("canonical_normals")!=true||
        (input.at("explicit_mirror").get<bool>()&&input.at("base_version")!=10)||
        !input.contains("canonical_conversions")||!input.at("canonical_conversions").is_array()||
        input.at("canonical_conversions").empty()||input.at("canonical_conversions").size()>256)
      throw std::invalid_argument("Canonical material13 requires its actual conversion pool and explicit existing capabilities");
    return input.at("base_version");
  }
  for(const char *field:{"explicit_mirror","canonical_normals","canonical_conversions"})
    if(input.contains(field))throw std::invalid_argument("Canonical normal capability requires material13");
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
inline bool physical_material_archive(const nlohmann::json &input){if(input.is_object()&&input.contains("version")&&input.at("version")==21){material_archive_base_version(input);return input.at("physical_emission").get<bool>();}
  material_archive_base_version(input);return input.at("version")==11||((input.at("version")==12||input.at("version")==13||input.at("version")==14||(input.at("version")==15||input.at("version")==16||(input.at("version")==17||(input.at("version")==18||input.at("version")==19))))&&input.at("physical_emission").get<bool>());
}
inline bool explicit_mirror_material_archive(const nlohmann::json &input){if(input.is_object()&&input.contains("version")&&input.at("version")==21){material_archive_base_version(input);return input.at("explicit_mirror").get<bool>();}
  material_archive_base_version(input);return input.at("version")==12||((input.at("version")==13||input.at("version")==14||(input.at("version")==15||input.at("version")==16||(input.at("version")==17||(input.at("version")==18||input.at("version")==19))))&&input.at("explicit_mirror").get<bool>());
}
inline bool canonical_material_archive(const nlohmann::json &input){if(input.is_object()&&input.contains("version")&&input.at("version")==21){material_archive_base_version(input);return input.at("canonical_normals").get<bool>();}material_archive_base_version(input);return input.at("version")==13||((input.at("version")==15||input.at("version")==16||(input.at("version")==17||(input.at("version")==18||input.at("version")==19)))&&input.at("canonical_normals").get<bool>());}
inline bool canonical_uv_material_archive(const nlohmann::json &input){if(input.is_object()&&input.contains("version")&&input.at("version")==21){material_archive_base_version(input);return input.at("retained_canonical_uv").get<bool>();}material_archive_base_version(input);return input.at("version")==16||((input.at("version")==17||(input.at("version")==18||input.at("version")==19))&&input.at("retained_canonical_uv").get<bool>());}
inline bool layered_material_archive(const nlohmann::json &input){if(input.is_object()&&input.contains("version")&&input.at("version")==21){material_archive_base_version(input);return input.at("layered_normals").get<bool>();}material_archive_base_version(input);return input.at("version")==15||input.at("version")==16||((input.at("version")==17||(input.at("version")==18||input.at("version")==19))&&input.at("layered_normals").get<bool>());}
inline bool native_paint_material_archive(const nlohmann::json &input){if(input.is_object()&&input.contains("version")&&input.at("version")==21){material_archive_base_version(input);return input.at("native_paint_sampling").get<bool>();}material_archive_base_version(input);return input.at("version")==14||input.at("version")==17||input.at("version")==18||(input.at("version")==19&&input.at("native_paint_sampling").get<bool>());}
inline bool native_frame_material_archive(const nlohmann::json &input){if(input.is_object()&&input.contains("version")&&input.at("version")==21){material_archive_base_version(input);return input.at("native_paint_frames").get<bool>();}material_archive_base_version(input);return input.at("version")==18||(input.at("version")==19&&input.at("native_paint_frames").get<bool>());}
inline bool canonical_paint_material_archive(const nlohmann::json &input){if(input.is_object()&&input.contains("version")&&input.at("version")==21){material_archive_base_version(input);return input.at("canonical_paint_sampling").get<bool>();}material_archive_base_version(input);return input.at("version")==19;}
inline bool polygon_surface_material_archive(const nlohmann::json&input){material_archive_base_version(input);return input.at("version")==21;}
inline const nlohmann::json &normal_preparation_base_version(const nlohmann::json &p,const nlohmann::json &archive){
  if(!p.is_object()||!p.contains("version")||!p.at("version").is_number_integer())throw std::invalid_argument("Invalid normal preparation version");
  if(p.at("version")==7){
    if(!canonical_material_archive(archive)||!p.contains("base_version")||!p.at("base_version").is_number_integer()||
        p.at("base_version").get<int64_t>()<1||p.at("base_version").get<int64_t>()>6||!p.contains("canonical_conversion"))
      throw std::invalid_argument("Normal entry7 requires its actual owning canonical conversion context");
    return p.at("base_version");
  }
  if(p.contains("canonical_conversion")||p.contains("base_version"))throw std::invalid_argument("Normal conversion reference requires entry7");
  return p.at("version");
}
} // namespace dome::detail
