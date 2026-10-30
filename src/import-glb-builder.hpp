// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/import.hpp"
#include <bit>
#include <nlohmann/json.hpp>
namespace dome::detail {
struct ImportGlbBuilder {
  nlohmann::json json={{"asset",{{"version","2.0"},{"generator","Dome Sculpt native source adapter"}}},
      {"bufferViews",nlohmann::json::array()},{"accessors",nlohmann::json::array()}};
  std::vector<uint8_t> binary;
  ImportLimits limits;
  explicit ImportGlbBuilder(ImportLimits l):limits(std::move(l)){}
  void stop() const {if(limits.cancelled&&limits.cancelled())throw std::runtime_error("Native import cancelled");}
  static void word(std::vector<uint8_t>&v,uint32_t n){for(int i=0;i<4;++i)v.push_back(uint8_t(n>>(8*i)));}
  size_t bytes(std::span<const uint8_t> value) {
    stop();while(binary.size()%4)binary.push_back(0);
    if(binary.size()>limits.max_file_bytes || value.size()>limits.max_file_bytes-binary.size())
      throw std::length_error("Native import canonical binary budget exceeded");
    size_t at=binary.size();binary.insert(binary.end(),value.begin(),value.end());return at;
  }
  uint32_t floats(std::span<const float> value,uint32_t width,bool bounds=false) {
    stop();if(!width || value.size()%width)throw std::invalid_argument("Invalid native adapter attribute width");
    std::vector<uint8_t> data;data.reserve(value.size()*4);
    std::vector<float> low(width,std::numeric_limits<float>::infinity()),high(width,-std::numeric_limits<float>::infinity());
    for(size_t i=0;i<value.size();++i){if((i&4095)==0)stop();float v=value[i];if(!std::isfinite(v))throw std::runtime_error("Native source attribute is not finite");word(data,std::bit_cast<uint32_t>(v));low[i%width]=std::min(low[i%width],v);high[i%width]=std::max(high[i%width],v);}
    auto view=json["bufferViews"].size();json["bufferViews"].push_back({{"buffer",0},{"byteOffset",bytes(data)},{"byteLength",data.size()}});
    auto id=json["accessors"].size();nlohmann::json a={{"bufferView",view},{"componentType",5126},{"count",value.size()/width},{"type",width==2?"VEC2":width==3?"VEC3":"VEC4"}};
    if(bounds){a["min"]=low;a["max"]=high;}json["accessors"].push_back(std::move(a));return uint32_t(id);
  }
  std::shared_ptr<const std::vector<uint8_t>> finish() {
    stop();json["buffers"]=nlohmann::json::array({{{"byteLength",binary.size()}}});
    auto text=json.dump();while(text.size()%4)text+=' ';while(binary.size()%4)binary.push_back(0);
    if(text.size()>16*1024*1024 || text.size()+binary.size()+28>limits.max_file_bytes || text.size()+binary.size()+28>UINT32_MAX)
      throw std::length_error("Native canonical GLB budget exceeded");
    auto out=std::make_shared<std::vector<uint8_t>>();out->reserve(text.size()+binary.size()+28);
    word(*out,0x46546c67);word(*out,2);word(*out,uint32_t(text.size()+binary.size()+28));word(*out,uint32_t(text.size()));word(*out,0x4e4f534a);out->insert(out->end(),text.begin(),text.end());word(*out,uint32_t(binary.size()));word(*out,0x004e4942);out->insert(out->end(),binary.begin(),binary.end());return out;
  }
};
} // namespace dome::detail
