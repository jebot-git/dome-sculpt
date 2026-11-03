// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <nlohmann/json.hpp>
#include <array>
#include <limits>

namespace dome::detail {
struct SourceUvArchiveMeasure {size_t bytes=0;unsigned scene_version=0;};
// Preflight all serialized coordinate bytes before mesh/raster/owner allocation.
// Repeated serialized records are charged separately; actual archive sharing is
// represented by the existing owning paint references, not guessed by values.
inline SourceUvArchiveMeasure source_uv_archive_measure(const nlohmann::json &encoded,
    unsigned scene_version=std::numeric_limits<unsigned>::max()) {
  SourceUvArchiveMeasure out;
  if(!encoded.is_object())throw std::invalid_argument("Invalid original source-map record");
  const bool extra=encoded.contains("version");
  if(extra&&(!encoded.at("version").is_number_integer()||encoded.at("version").get<int64_t>()!=2||
      encoded.size()!=3||!encoded.at("payload").is_object()||!encoded.at("extra_uv_sets").is_array()||
      encoded.at("extra_uv_sets").empty()||encoded.at("extra_uv_sets").size()>6))
    throw std::invalid_argument("Invalid extra source UV envelope");
  if(extra){out.scene_version=26;if(scene_version<26)throw std::invalid_argument("Extra source UV coordinates require Scene26");}
  const auto &payload=extra?encoded.at("payload"):encoded;
  auto buffer=[&](const nlohmann::json &record){
    const auto &n=record.at("vertices"),&data=record.at("data");
    if(!n.is_number_integer()||n.get<int64_t>()<1||n.get<uint64_t>()>2'000'000||
        !data.is_binary()||data.get_binary().size()!=n.get<uint64_t>()*8)
      throw std::invalid_argument("Invalid source UV coordinate buffer");
    const auto bytes=data.get_binary().size();
    if(bytes>112*1024*1024-out.bytes)throw std::length_error("Source UV coordinate storage exceeded");
    out.bytes+=bytes;
  };
  if(payload.contains("source_uv1")){
    const auto &v=payload.at("source_uv1");
    if(!v.is_object()||v.size()!=3||!v.at("version").is_number_integer()||v.at("version").get<int64_t>()!=1)
      throw std::invalid_argument("Invalid source UV1 record");
    if(scene_version<18)throw std::invalid_argument("Source UV1 coordinates require Scene18");
    out.scene_version=std::max(out.scene_version,18u);buffer(v);
  }
  if(extra){std::array<bool,8> used{};
    for(const auto &v:encoded.at("extra_uv_sets")){
      if(!v.is_object()||v.size()!=3||!v.at("set").is_number_integer()||v.at("set").get<int64_t>()<2||v.at("set").get<uint64_t>()>7)
        throw std::invalid_argument("Invalid additional source UV set identity");
      const auto set=v.at("set").get<size_t>();if(used[set])throw std::invalid_argument("Duplicate additional source UV set");used[set]=true;buffer(v);
    }
  }
  return out;
}
} // namespace dome::detail
