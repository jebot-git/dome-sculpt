// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/baked-emission.hpp"
#include "native-read-scope.hpp"
#include "baked-emission-write-scope.hpp"
#include <nlohmann/json.hpp>

namespace dome { class Scene; class PaintImageArchive; }
namespace dome::detail {
// An explicit short-lived native reader capability, not a global/TLS switch.
// Only the actual Scene41 envelope creates it. Copies of existing decode
// contexts borrow it within the synchronous owning decode, never in snapshots.
class BakedEmissionReadCapability final {
  friend class dome::Scene;
  explicit BakedEmissionReadCapability(bool source_bake=false):source_bake_(source_bake) {}
  bool source_bake_=false;
  mutable size_t admitted_source_bake_=0;
  mutable size_t admitted_ = 0;
public:
  BakedEmissionReadCapability(const BakedEmissionReadCapability &) = delete;
  void note_validated_field() const { ++admitted_; }
  bool source_bake_allowed() const noexcept { return source_bake_; }
  void note_source_bake() const { if(!source_bake_)throw std::invalid_argument("Source Bake needs Scene42 ownership");++admitted_source_bake_; }
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
  for(const auto &entry:b.items()){
    const auto &key=entry.key();
    if(key!="version"&&key!="base_version"&&key!="float_emission"&&
       key!="color"&&key!="orm"&&key!="emission"&&key!="normal"&&key!="coverage"&&
       key!="color_interpretation"&&key!="pbr_color"&&key!="normal_interpretation"&&
       key!="mtoon_normal"&&key!="normal_authority"&&key!="normal_field")
      throw std::invalid_argument("Unknown FLOAT baked emission metadata");
  }
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
} // namespace dome::detail
