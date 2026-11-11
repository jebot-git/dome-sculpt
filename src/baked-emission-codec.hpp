// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/baked-emission.hpp"
#include "native-read-scope.hpp"
#include <nlohmann/json.hpp>

namespace dome { class Scene; }
namespace dome::detail {
// An explicit short-lived native reader capability, not a global/TLS switch.
// Only the actual Scene41 envelope creates it. Copies of existing decode
// contexts borrow it within the synchronous owning decode, never in snapshots.
class BakedEmissionReadCapability final {
  friend class dome::Scene;
  BakedEmissionReadCapability() = default;
  mutable size_t admitted_ = 0;
public:
  BakedEmissionReadCapability(const BakedEmissionReadCapability &) = delete;
  void note_validated_field() const { ++admitted_; }
};
inline unsigned baked_legacy_version(const nlohmann::json &b,
                                     const BakedEmissionReadCapability *capability) {
  if(!b.is_object()||!b.at("version").is_number_integer())
    throw std::invalid_argument("Invalid baked payload version");
  const auto v=b.at("version").get<int64_t>();
  if(v!=6){
    if(v<1||v>5||b.contains("base_version")||b.contains("float_emission"))
      throw std::invalid_argument("Unexpected FLOAT baked emission capability");
    return unsigned(v);
  }
  if(!capability||!b.at("base_version").is_number_integer()||
     b.at("base_version").get<int64_t>()<1||b.at("base_version").get<int64_t>()>5||
     !b.at("float_emission").is_binary()||!b.at("emission").is_binary()||
     !b.at("emission").get_binary().empty())
    throw std::invalid_argument("Baked6 needs genuine Scene41 ownership and no RGB8 emission authority");
  return b.at("base_version").get<unsigned>();
}
inline size_t baked_float_preflight(const nlohmann::json &b, uint32_t r,
                                     const BakedEmissionReadCapability *capability,
                                     const std::function<bool()> &cancelled={}) {
  (void)baked_legacy_version(b,capability);
  if(b.at("version")!=6)return 0;
  const auto &bytes=b.at("float_emission").get_binary();
  auto check=[cancelled]{NativeReadScope::check();return cancelled&&cancelled();};
  if(BakedEmissionField::encoded_resolution(bytes,{128*1024*1024,check})!=r)
    throw std::invalid_argument("FLOAT baked emission differs from actual paint grid");
  return BakedEmissionField::required_bytes(r,{128*1024*1024,check});
}
// Used only on the writer's own completed native records, never as reader
// authority. Skip independently enveloped child scenes and geometric arrays.
inline bool encoded_baked_float(const nlohmann::json &value, size_t depth=0) {
  if(depth>256)throw std::length_error("Baked emission scene metadata depth exceeded");
  if(value.is_object()){
    if(value.contains("format")&&value.at("format")=="dome-sculpt"&&
       value.contains("version")&&value.at("version")==41)return false;
    if(value.contains("baked")&&value.at("baked").is_object()&&
       value.at("baked").contains("version")&&value.at("baked").at("version")==6)return true;
    for(const auto &item:value.items()){
      if(item.key()=="samples"||item.key()=="sculpt_layers"||item.key()=="sculpt_mask")continue;
      if(encoded_baked_float(item.value(),depth+1))return true;
    }
  }else if(value.is_array())for(const auto &child:value)if(encoded_baked_float(child,depth+1))return true;
  return false;
}
} // namespace dome::detail
