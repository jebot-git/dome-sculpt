// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "vrm-source-fixtures.hpp"
#include "dome/import.hpp"
#include "dome/rig.hpp"
#include <bit>
#include <fstream>
namespace vrm_rest_fixture {
using namespace dome;
inline uint32_t with_joint_mesh(const std::filesystem::path &input,const std::filesystem::path &output){std::ifstream in(input,std::ios::binary);std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in),{}};const auto length=vrm_source_fixture::word(bytes,12);auto j=nlohmann::json::parse(bytes.begin()+20,bytes.begin()+20+length);size_t head=0;for(const auto &bone:j["extensions"]["VRM"]["humanoid"]["humanBones"])if(bone["bone"]=="head")head=bone["node"].get<size_t>();j["nodes"][head]["mesh"]=j["nodes"][j["nodes"].size()-3]["mesh"];
 // Genuine existing node becomes a rigid mesh instance as well as a real
 // weighted joint. No dummy skin or metadata-only synthetic node is created.
 ImportLimits limits;limits.mode=ImportMode::PreserveGltf;limits.allow_vrm_metadata=true;limits.allow_legacy_vrm_skin_hint=true;const auto asset=import_glb(bytes,limits);size_t neck=SIZE_MAX;for(size_t n=0;n<j["nodes"].size();++n)for(const auto &child:j["nodes"][n].value("children",nlohmann::json::array()))if(child.get<size_t>()==head)neck=n;if(neck==SIZE_MAX)throw std::runtime_error("Head fixture lacks actual neck");const auto spacer=uint32_t(j["nodes"].size());j["nodes"].push_back({{"name","Actual head fitting spacer"},{"translation",{.03f,.02f,0.f}},{"rotation",{0.f,0.f,std::sin(.1f),std::cos(.1f)}}});j["nodes"][neck]["children"].push_back(spacer);
 std::vector<uint8_t> binary(bytes.begin()+28+length,bytes.end());while(binary.size()%4)binary.push_back(0);const auto offset=binary.size();auto matrices=asset.skins.at(0).inverse_bind_matrices;const auto original_count=j["skins"][0]["joints"].size();if(!asset.skins[0].inverse_bind_matrices_authored)matrices.assign(original_count,RigMatrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});matrices.insert(matrices.begin()+original_count,RigMatrix{1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1});for(const auto &m:matrices)for(float v:m){const auto word=std::bit_cast<uint32_t>(v);for(int k=0;k<4;++k)binary.push_back(uint8_t(word>>(8*k)));}const auto view=j["bufferViews"].size();j["bufferViews"].push_back({{"buffer",0},{"byteOffset",offset},{"byteLength",binary.size()-offset}});const auto accessor=j["accessors"].size();j["accessors"].push_back({{"bufferView",view},{"componentType",5126},{"count",matrices.size()},{"type","MAT4"}});j["skins"][0]["joints"].push_back(spacer);j["skins"][0]["inverseBindMatrices"]=accessor;j["buffers"][0]["byteLength"]=binary.size();
 auto text=j.dump();while(text.size()%4)text+=' ';std::vector<uint8_t> out;auto put=[&](uint32_t v){for(int k=0;k<4;++k)out.push_back(uint8_t(v>>(8*k)));};put(0x46546c67);put(2);put(uint32_t(28+text.size()+binary.size()));put(uint32_t(text.size()));put(0x4e4f534a);out.insert(out.end(),text.begin(),text.end());put(uint32_t(binary.size()));put(0x004e4942);out.insert(out.end(),binary.begin(),binary.end());std::ofstream file(output,std::ios::binary);file.write(reinterpret_cast<const char*>(out.data()),out.size());return spacer;}
}
