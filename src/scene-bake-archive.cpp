// SPDX-License-Identifier: GPL-3.0-or-later
#include "scene-bake-archive.hpp"

namespace dome::detail {
namespace {
uint32_t index(const nlohmann::json &value){
  if((!value.is_number_unsigned()&&(!value.is_number_integer()||value.get<int64_t>()<0))||
      value.get<uint64_t>()>UINT32_MAX)
    throw std::invalid_argument("Invalid shared baked field identity");
  return value.get<uint32_t>();
}
}
SharedBakeReader::SharedBakeReader(const nlohmann::json &bank,
    const BakedEmissionReadCapability &capability){
  if(!bank.is_array()||bank.empty()||bank.size()>256)
    throw std::invalid_argument("Invalid shared baked field table");
  entries_.reserve(bank.size());
  for(const auto &record:bank){
    NativeReadScope::check();
    if(!record.is_object()||record.size()!=3)
      throw std::invalid_argument("Invalid shared baked field entry");
    const auto r=index(record.at("resolution")),domain=index(record.at("domain"));
    if(r<64||r>2048||(r&(r-1)))
      throw std::invalid_argument("Invalid shared baked field resolution");
    const auto &bake=record.at("bake");
    const auto base=baked_legacy_version(bake,&capability);
    if(bake.at("version")!=6||(base!=1&&base!=3)||
        bake.size()!=(base==1?8:10)||
        (base==3&&(bake.at("normal_interpretation")!="native-attributes-v1"||
         !bake.at("mtoon_normal").is_binary()||!bake.at("mtoon_normal").get_binary().empty())))
      throw std::invalid_argument("Shared baked field requires plain native FLOAT paint");
    const size_t pixels=size_t(r)*r;
    size_t retained=baked_float_preflight(bake,r,&capability);
    if(retained>=pixels*sizeof(BakedEmissionSample))
      throw std::invalid_argument("Shared baked field requires its compact FLOAT layout");
    for(const auto *key:{"color","orm","normal","coverage"}){
      const auto &data=bake.at(key);
      if(!data.is_binary())throw std::invalid_argument("Invalid shared baked channel");
      const size_t count=data.get_binary().size();
      const std::string_view channel(key);
      if(count!=(channel=="coverage"?pixels:pixels*4)&&!(channel=="normal"&&count==0))
        throw std::invalid_argument("Shared baked channel differs from its paint grid");
      retained+=count;
    }
    if(retained>128*1024*1024||retained>scene_paint_byte_limit-bytes_)
      throw std::length_error("Shared baked fields exceed scene or per-paint storage budget");
    bytes_+=retained;
    entries_.push_back({&bake,r,domain,{},{}});
  }
}
const SharedBakeReader::Entry &SharedBakeReader::at(const nlohmann::json &reference) const {
  if(!shared_bake_reference(reference)||!reference.at("version").is_number_integer()||reference.size()!=2)
    throw std::invalid_argument("Invalid shared baked field reference");
  const auto id=index(reference.at("id"));
  if(id>=entries_.size())throw std::invalid_argument("Shared baked field identity is outside its table");
  return entries_[id];
}
const SharedBakeReader::Entry *SharedBakeReader::resolve(const nlohmann::json &record,
    const nlohmann::json &paint,uint32_t resolution) const {
  const auto &entry=at(paint.at("baked"));
  if(!record.contains("version")||record.at("version")!=3||
      !record.contains("domain")||index(record.at("domain"))!=entry.domain||
      resolution!=entry.resolution||paint.contains("uv_policy")||paint.contains("source_maps")||
      paint.contains("normal_projection")||paint.contains("color_projection"))
    throw std::invalid_argument("Shared baked field borrowed another native paint domain");
  return &entry;
}
void SharedBakeReader::require_complete() const {
  for(const auto &entry:entries_)if(!entry.owner||entry.users.size()<2)
    throw std::invalid_argument("Shared baked field requires two actual paint owners");
}
}
