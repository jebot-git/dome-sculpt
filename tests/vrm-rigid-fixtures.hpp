// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "vrm-source-fixtures.hpp"
#include <bit>
namespace vrm_source_fixture {
struct RigidNodes {size_t head, moving, fixed, instance;};
inline RigidNodes with_rigid_accessories(const std::filesystem::path &input,
                                         const std::filesystem::path &output) {
  std::ifstream file(input,std::ios::binary);
  std::vector<uint8_t> raw{std::istreambuf_iterator<char>(file),{}};
  auto size=word(raw,12);
  auto j=nlohmann::json::parse(raw.begin()+20,raw.begin()+20+size);
  std::vector<uint8_t> binary(raw.begin()+28+size,raw.end());
  auto put=[](std::vector<uint8_t>&out,uint32_t v){for(int k=0;k<4;++k)out.push_back(uint8_t(v>>(8*k)));};
  auto floats=[&](std::vector<float> values,int dimensions,bool bounds){
    while(binary.size()%4)binary.push_back(0);
    auto offset=binary.size();
    for(float x:values)put(binary,std::bit_cast<uint32_t>(x));
    auto view=j["bufferViews"].size();
    j["bufferViews"].push_back({{"buffer",0},{"byteOffset",offset},{"byteLength",binary.size()-offset},{"target",34962}});
    auto accessor=j["accessors"].size();
    nlohmann::json a={{"bufferView",view},{"componentType",5126},{"count",values.size()/dimensions},{"type",dimensions==3?"VEC3":"VEC2"}};
    if(bounds){std::vector<float> lo(values.begin(),values.begin()+dimensions),hi=lo;for(size_t k=0;k<values.size();++k){lo[k%dimensions]=std::min(lo[k%dimensions],values[k]);hi[k%dimensions]=std::max(hi[k%dimensions],values[k]);}a["min"]=lo;a["max"]=hi;}
    j["accessors"].push_back(std::move(a));return accessor;
  };
  auto positions=floats({-.15f,-.10f,0, .15f,-.10f,0, .15f,.10f,0, -.15f,.10f,0},3,true);
  auto normals=floats({0,0,1,0,0,1,0,0,1,0,0,1},3,false);
  auto uv=floats({.1f,.1f,.9f,.1f,.9f,.9f,.1f,.9f},2,false);
  auto morph=floats({0,0,0,0,0,0,.025f,.04f,0,0,0,0},3,true);
  auto offset=binary.size();for(uint32_t v:{0,1,2,0,2,3})put(binary,v);
  auto view=j["bufferViews"].size();j["bufferViews"].push_back({{"buffer",0},{"byteOffset",offset},{"byteLength",24},{"target",34963}});
  auto indices=j["accessors"].size();j["accessors"].push_back({{"bufferView",view},{"componentType",5125},{"count",6},{"type","SCALAR"}});
  auto first_mesh=j["meshes"].size();
  nlohmann::json primitive={{"attributes",{{"POSITION",positions},{"NORMAL",normals},{"TEXCOORD_0",uv}}},{"indices",indices},{"material",0},{"mode",4}};
  auto moving=primitive;moving["targets"]={{{"POSITION",morph}}};
  j["meshes"].push_back({{"name","Rigid head badge"},{"primitives",{moving}},{"weights",{0.f}},{"extras",{{"targetNames",{"Rigid edge"}}}}});
  j["meshes"].push_back({{"name","Rigid independent badge"},{"primitives",{primitive}}});
  size_t head=SIZE_MAX;for(const auto &bone:j["extensions"]["VRM"]["humanoid"]["humanBones"])if(bone["bone"]=="head")head=bone["node"].get<size_t>();
  if(head==SIZE_MAX)throw std::runtime_error("Source lacks head attachment");
  auto node=j["nodes"].size();
  j["nodes"].push_back({{"name","Rigid attached badge"},{"mesh",first_mesh},{"translation",{.35f,.12f,.1f}},{"rotation",{0.f,0.f,std::sin(.12f),std::cos(.12f)}},{"scale",{.8f,1.1f,1.f}}});
  j["nodes"][head]["children"].push_back(node);
  j["nodes"].push_back({{"name","Rigid independent badge"},{"mesh",first_mesh+1},{"translation",{-.45f,.8f,.1f}}});
  j["scenes"][0]["nodes"].push_back(node+1);
  j["nodes"].push_back({{"name","Rigid shared mesh instance"},{"mesh",first_mesh+1},{"translation",{.5f,.6f,.1f}}});
  j["scenes"][0]["nodes"].push_back(node+2);
  j["extensions"]["VRM"]["firstPerson"]["meshAnnotations"].push_back({{"mesh",first_mesh},{"firstPersonFlag","Both"}});
  j["buffers"][0]["byteLength"]=binary.size();auto text=j.dump();while(text.size()%4)text+=' ';
  std::vector<uint8_t> bytes;put(bytes,0x46546c67);put(bytes,2);put(bytes,uint32_t(28+text.size()+binary.size()));put(bytes,uint32_t(text.size()));put(bytes,0x4e4f534a);bytes.insert(bytes.end(),text.begin(),text.end());put(bytes,uint32_t(binary.size()));put(bytes,0x004e4942);bytes.insert(bytes.end(),binary.begin(),binary.end());
  std::ofstream out(output,std::ios::binary);out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());
  return {head,node,node+1,node+2};
}
} // namespace vrm_source_fixture
