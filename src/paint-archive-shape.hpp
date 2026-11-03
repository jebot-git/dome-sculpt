// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <nlohmann/json.hpp>
#include "dome/source-paint.hpp"
namespace dome::detail {
// Preflight only. This does not grant geometry, field or source authority.
inline const nlohmann::json &paint_archive_payload(const nlohmann::json &input,bool owning_domain=false){
  if(!input.is_object())throw std::invalid_argument("Paint archive must be an object");
  if(!input.contains("version")){
    if(input.contains("domain")||input.contains("payload"))throw std::invalid_argument("Paint domain requires an actual version2 envelope");
    return input;
  }
  if(!owning_domain||input.size()!=3||!input.at("version").is_number_integer()||input.at("version")!=2||
     !input.contains("payload")||!input.at("payload").is_object()||input.at("payload").contains("version")||
     input.at("payload").contains("domain")||input.at("payload").contains("payload")||!input.contains("domain")||
     (!input.at("domain").is_number_unsigned()&&(!input.at("domain").is_number_integer()||input.at("domain").get<int64_t>()<0))||
     input.at("domain").get<uint64_t>()>UINT32_MAX)
    throw std::invalid_argument("Retained Paint2 requires a complete owning domain context");
  return input.at("payload");
}
inline const nlohmann::json &paint_archive_payload(const nlohmann::json &input,bool owning_domain,
                                                  const SourcePaintDecodeContext &context){
  if(!input.is_object()||!input.contains("version")||input.at("version")!=4)
    return paint_archive_payload(input,owning_domain);
  if(!context.independent_source||input.size()!=4||!input.at("version").is_number_integer()||
     !input.contains("payload")||!input.at("payload").is_object()||input.at("payload").contains("version")||
     !input.contains("before")||!input.at("before").is_object()||input.at("before").contains("version")||
     !input.contains("vertices")||!input.at("vertices").is_binary()||
     input.at("vertices").get_binary().size()%4||input.at("vertices").get_binary().size()>8'000'000||
     input.at("payload").contains("source_images")||input.at("before").contains("source_images"))
    throw std::invalid_argument("Paint4 preflight requires its explicit owning source context");
  // Reject nested domain/envelope keys in both actual legacy payloads.
  paint_archive_payload(input.at("before"),false);
  return paint_archive_payload(input.at("payload"),false);
}
} // namespace dome::detail
