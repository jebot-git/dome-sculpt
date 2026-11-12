// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/rig-fit.hpp"
#include "cgltf.h"
#include <nlohmann/json.hpp>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
namespace {
using dome::HumanoidMapping;
using Json=nlohmann::json;
struct ArtistRow {std::map<size_t,double> weights;double unmapped=0;};
std::vector<ArtistRow> artist_rows(const std::filesystem::path &path,const HumanoidMapping &mapping){
 cgltf_options options{};cgltf_data *raw=nullptr;
 if(cgltf_parse_file(&options,path.c_str(),&raw)!=cgltf_result_success)throw std::runtime_error("Artist parse failed");
 std::unique_ptr<cgltf_data,decltype(&cgltf_free)> data(raw,cgltf_free);
 if(cgltf_load_buffers(&options,raw,path.c_str())!=cgltf_result_success||cgltf_validate(raw)!=cgltf_result_success)throw std::runtime_error("Artist source invalid");
 std::ifstream file(path,std::ios::binary);std::vector<char> bytes((std::istreambuf_iterator<char>(file)),{});uint32_t length=0;memcpy(&length,bytes.data()+12,4);
 auto json=Json::parse(bytes.data()+20,bytes.data()+20+length);std::map<const cgltf_node*,size_t> roles;
 for(const auto &entry:json.at("extensions").at("VRM").at("humanoid").at("humanBones")){
  std::string role=entry.at("bone");if(role.ends_with("ThumbProximal"))role.replace(role.size()-8,8,"Metacarpal");else if(role.ends_with("ThumbIntermediate"))role.replace(role.size()-12,12,"Proximal");
  auto found=mapping.joints.find(role);if(found==mapping.joints.end())continue;
  size_t node=entry.at("node");if(node>=raw->nodes_count||!roles.emplace(raw->nodes+node,found->second).second)throw std::runtime_error("Artist role identity invalid");
 }
 std::vector<ArtistRow> result;
 for(size_t node=0;node<raw->nodes_count;++node)if(auto *mesh=raw->nodes[node].mesh){
  auto *skin=raw->nodes[node].skin;
  for(size_t primitive=0;primitive<mesh->primitives_count;++primitive){auto &p=mesh->primitives[primitive];auto *position=cgltf_find_accessor(&p,cgltf_attribute_type_position,0);if(!position)throw std::runtime_error("Artist POSITION missing");
   const auto start=result.size();result.resize(start+position->count);
   for(size_t vertex=0;vertex<position->count;++vertex){auto &row=result[start+vertex];double total=0;
    for(size_t set=0;set<2;++set){auto *joints=cgltf_find_accessor(&p,cgltf_attribute_type_joints,int(set));auto *weights=cgltf_find_accessor(&p,cgltf_attribute_type_weights,int(set));
     if(!joints&&!weights)continue;if(!joints||!weights||!skin||joints->count!=position->count||weights->count!=position->count)throw std::runtime_error("Artist influence domain invalid");
     cgltf_uint indices[4]{};float values[4]{};if(!cgltf_accessor_read_uint(joints,vertex,indices,4)||!cgltf_accessor_read_float(weights,vertex,values,4))throw std::runtime_error("Artist influence read failed");
     for(size_t i=0;i<4;++i){const double value=values[i];if(!std::isfinite(value)||value<0||indices[i]>=skin->joints_count)throw std::runtime_error("Artist influence invalid");if(value==0)continue;total+=value;auto found=roles.find(skin->joints[indices[i]]);if(found==roles.end())row.unmapped+=value;else row.weights[found->second]+=value;}
    }
    if(total==0){row.unmapped=1;continue;}if(std::abs(total-1)>.005)throw std::runtime_error("Artist weights not normalized");
    // Quantized source accessor sums may differ from1. Normalize only the
    // independent reference; never pass it to automatic_bind or fit_humanoid.
    row.unmapped/=total;for(auto &[joint,value]:row.weights){(void)joint;value/=total;}
   }
  }
 }
 return result;
}

}
