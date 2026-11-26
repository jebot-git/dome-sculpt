// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/import-authoring.hpp"
#include "dome/humanoid-creator.hpp"
#include <fstream>
#include <nlohmann/json.hpp>

namespace vrm_derivative_fixture {
using namespace dome;

inline void word(std::vector<uint8_t> &out,uint32_t value) {
  for(unsigned k=0;k<4;++k)out.push_back(uint8_t(value>>(k*8)));
}

struct Source {
  nlohmann::json json;
  std::vector<uint8_t> bin;

  explicit Source(const std::vector<uint8_t> &base) {
    const auto at=[&](size_t p){
      return uint32_t(base.at(p))|uint32_t(base.at(p+1))<<8|
             uint32_t(base.at(p+2))<<16|uint32_t(base.at(p+3))<<24;
    };
    const size_t tail=20+at(12);
    if(at(0)!=0x46546c67||at(4)!=2||at(16)!=0x4e4f534a||
       at(tail+4)!=0x004e4942||tail+8+at(tail)!=base.size())
      throw std::runtime_error("Expected a verified VRM0 source container");
    json=nlohmann::json::parse(base.begin()+20,base.begin()+tail);
    bin.assign(base.begin()+tail+8,base.end());
  }

  ImportedAuthoring import(const std::filesystem::path &path) {
    while(bin.size()%4)bin.push_back(0);
    json.at("buffers").at(0)["byteLength"]=bin.size();
    auto text=json.dump();while(text.size()%4)text.push_back(' ');
    std::vector<uint8_t> bytes;
    for(auto tag:{0x46546c67u,2u,uint32_t(28+text.size()+bin.size()),
                  uint32_t(text.size()),0x4e4f534au})word(bytes,tag);
    bytes.insert(bytes.end(),text.begin(),text.end());
    word(bytes,uint32_t(bin.size()));word(bytes,0x004e4942);
    bytes.insert(bytes.end(),bin.begin(),bin.end());
    if(path.has_parent_path())std::filesystem::create_directories(path.parent_path());
    std::ofstream stream(path,std::ios::binary);
    stream.write(reinterpret_cast<const char *>(bytes.data()),bytes.size());
    stream.close();
    if(!stream)throw std::runtime_error("Could not write the derived VRM0 fixture");
    Scene empty;empty.active().visible=false;
    ImportAuthoringOptions options;options.paint_resolution=64;
    return prepare_authoring_import(empty,path,options);
  }
};

inline uint64_t root(const ImportedAuthoring &result) {
  return result.scene.group_path(result.scene.object(result.objects.front()).group).front().id;
}

inline bool in_avatar(const Scene &scene,const SculptObject &object,uint64_t instance) {
  auto group=object.group;
  while(group&&group!=instance)group=scene.group(group).parent;
  return group==instance;
}

inline bool in_avatar(const ObjectSurface &object,uint64_t instance) {
  return !object.groups.empty()&&object.groups.front().id==instance;
}

inline void presentation(Scene &scene,uint64_t instance) {
  auto &group=scene.group(instance);
  group.transform.scale=.8f;
  group.transform.position.y=-.7f;
  group.transform.orientation={0,1,0,0};
}
} // namespace vrm_derivative_fixture
