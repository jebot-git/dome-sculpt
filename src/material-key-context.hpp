// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <nlohmann/json.hpp>
#include <algorithm>
namespace dome::detail {
// Structural permission only, before bulk allocation. Material/Frame/Key
// decoders must still replay every real source and reject orphan records.
inline bool encoded_key_context(const nlohmann::json &m){
  if(!m.is_object()||!m.contains("version")||!m.at("version").is_number_integer()||m.at("version")!=10)return false;
  const auto &keys=m.at("key_derivations"),&bindings=m.at("bindings"),&frames=m.at("normal_frames");
  if(!keys.is_array()||keys.empty()||keys.size()>256||!bindings.is_array()||bindings.size()>256||!frames.is_object()||frames.at("version")!=2||!frames.at("sets").is_array()||frames.at("sets").size()>256||!frames.at("position_domains").is_array()||frames.at("position_domains").empty()||frames.at("position_domains").size()>256)return false;
  auto index=[](const nlohmann::json &v,size_t n)->size_t{if(!v.is_number_unsigned()&&(!v.is_number_integer()||v.get<int64_t>()<0))throw std::invalid_argument("Invalid encoded key capability reference");const auto id=v.get<uint64_t>();if(id>=n)throw std::invalid_argument("Encoded key capability reference outside owning context");return size_t(id);};
  bool found=false;
  for(size_t i=0;i<bindings.size();++i){const auto &b=bindings[i];if(!b.contains("key_derivation")||b.at("key_derivation").is_null())continue;const auto id=index(b.at("key_derivation"),keys.size());const auto &k=keys[id];if(!k.is_object()||k.size()!=7||(k.at("version")!=1&&k.at("version")!=2&&k.at("version")!=3))return false;index(k.at("before_binding"),i);const auto f=index(k.at("frames"),frames.at("sets").size());if(k.at("frames")!=b.at("frames")||frames.at("sets")[f].at("position_domain").is_null())return false;index(frames.at("sets")[f].at("position_domain"),frames.at("position_domains").size());
    if(!b.at("normal_preparation").is_null()){const auto &p=m.at("normal_preparations").at(index(b.at("normal_preparation"),m.at("normal_preparations").size()));if(p.at("version")!=5||p.at("key_derivation")!=b.at("key_derivation"))return false;}
    found=true;
  }
  return found;
}
inline bool encoded_key_feature(const nlohmann::json &m,const char *table,const char *field){
  if(!encoded_key_context(m))return false;const auto &records=m.at(table);if(!records.is_array()||records.size()>256)throw std::length_error("Invalid encoded key feature table");return std::any_of(records.begin(),records.end(),[&](const auto&r){return r.contains(field);});
}
}
