// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <nlohmann/json.hpp>
#include "dome/source-paint.hpp"
namespace dome::detail {
// Independently authored logical UV1 has no image-role/carrier number. Its
// source-domain count is preflighted before any surface/coordinate allocation.
inline size_t source_paint_authored_uv1_bytes(const nlohmann::json &input){
  if(!input.contains("authored_uv1"))return 0;
  const auto &uv=input.at("authored_uv1"),&positions=input.at("before").at("mesh").at("positions");
  if(input.at("before").contains("source_maps")){
    const auto &encoded=input.at("before").at("source_maps");
    const auto &roles=encoded.contains("version")?encoded.at("payload"):encoded;
    if(roles.contains("source_uv1"))throw std::invalid_argument("Redundant authored UV1 is not a canonical original field");
  }
  if(!uv.is_binary()||uv.get_binary().empty()||uv.get_binary().size()%8||uv.get_binary().size()>16'000'000||
     !positions.is_binary()||positions.get_binary().size()%12||uv.get_binary().size()/8!=positions.get_binary().size()/12)
    throw std::invalid_argument("Independent authored UV1 differs from its before vertex domain");
  return uv.get_binary().size();
}
// Preflight only. This does not grant geometry, field or source authority.
inline const nlohmann::json &paint_archive_payload(const nlohmann::json &input,bool owning_domain=false,bool physical_emission=false){
  if(!input.is_object())throw std::invalid_argument("Paint archive must be an object");
  if(!input.contains("version")){
    if(input.contains("domain")||input.contains("payload")||input.contains("authored_uv1"))throw std::invalid_argument("Paint domain requires an actual version2 envelope");
    return input;
  }
  if(!input.at("version").is_number_integer())throw std::invalid_argument("Invalid paint envelope version");
  const bool physical=input.at("version")==3;
  if(physical&&!physical_emission)throw std::invalid_argument("Physical emission Paint3 requires Scene27 capability");
  if((!physical&&(!owning_domain||input.at("version")!=2))||input.size()!=3||
     !input.contains("payload")||!input.at("payload").is_object()||input.at("payload").contains("version")||
     input.at("payload").contains("domain")||input.at("payload").contains("payload")||!input.contains("domain"))
    throw std::invalid_argument("Retained Paint2 requires a complete owning domain context");
  const auto &domain=input.at("domain");
  if(!(physical&&domain.is_null())&&(!owning_domain||
     (!domain.is_number_unsigned()&&(!domain.is_number_integer()||domain.get<int64_t>()<0))||
     domain.get<uint64_t>()>UINT32_MAX))throw std::invalid_argument("Invalid owning paint domain reference");
  return input.at("payload");
}
inline const nlohmann::json &paint_archive_payload(const nlohmann::json &input,bool owning_domain,
                                                  const SourcePaintDecodeContext &context){
  if(!input.is_object()||!input.contains("version")||input.at("version")!=4)
    return paint_archive_payload(input,owning_domain,context.physical_emission);
  if(!context.independent_source||input.size()!=4+size_t(input.contains("authored_uv1"))||!input.at("version").is_number_integer()||
     !input.contains("payload")||!input.at("payload").is_object()||input.at("payload").contains("version")||
     !input.contains("before")||!input.at("before").is_object()||input.at("before").contains("version")||
     !input.contains("vertices")||!input.at("vertices").is_binary()||
     input.at("vertices").get_binary().size()%4||input.at("vertices").get_binary().size()>8'000'000||
     input.at("payload").contains("source_images")||input.at("before").contains("source_images"))
    throw std::invalid_argument("Paint4 preflight requires its explicit owning source context");
  (void)source_paint_authored_uv1_bytes(input);
  // Reject nested domain/envelope keys in both actual legacy payloads.
  paint_archive_payload(input.at("before"),false);
  return paint_archive_payload(input.at("payload"),false);
}
inline bool physical_paint_authority(const nlohmann::json &input,const SourcePaintDecodeContext &context) {
  return context.physical_emission&&input.is_object()&&input.contains("version")&&
    (input.at("version")==3||(context.independent_source&&input.at("version")==4));
}
template<class F> void visit_paint_archive_payloads(const nlohmann::json &input,bool owning_domain,
                                                    const SourcePaintDecodeContext &context,F &&visit){
  const auto &payload=paint_archive_payload(input,owning_domain,context);
  if(input.contains("version")&&input.at("version")==4)visit(input.at("before"));
  visit(payload);
}
} // namespace dome::detail
