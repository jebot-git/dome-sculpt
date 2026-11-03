// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-node-extras.hpp"
#include "dome/avatar.hpp"
#include <algorithm>
namespace dome::detail {
namespace {
using Json=nlohmann::json;
void require(bool value,const char *why){
  if(!value)throw std::invalid_argument(std::string("Source node extras: ")+why);
}
void stop(const std::function<bool()> &cancelled){
  if(cancelled&&cancelled())throw std::runtime_error("Source node extras remapping cancelled");
}
}
void remap_source_node_extras(Json &node,std::span<const int> mapping,
                             const std::function<bool()> &cancelled){
  stop(cancelled);
  require(node.is_object(),"node must be an object");
  if(!node.contains("extras") || !node.at("extras").is_object() ||
      !node.at("extras").contains("domeSculptHumanoid"))return;
  require(mapping.size()<=100000,"source node map exceeds100000 nodes");
  const auto &hint=node.at("extras").at("domeSculptHumanoid");
  require(hint.is_object()&&hint.size()==2&&hint.contains("version")&&hint.contains("humanBones"),
          "domeSculptHumanoid requires exactly version and humanBones");
  const auto &version=hint.at("version");
  require(version.is_number_integer() &&
      (version.is_number_unsigned()?version.get<uint64_t>()==1:version.get<int64_t>()==1),
      "unsupported domeSculptHumanoid version (expected integer1)");
  const auto &bones=hint.at("humanBones");
  require(bones.is_object()&&bones.size()<=55,"domeSculptHumanoid humanBones must be a bounded object");
  auto mapped=hint;
  for(const auto &entry:bones.items()){
    stop(cancelled);
    require(std::any_of(human_roles().begin(),human_roles().end(),[&](const auto &role){return entry.key()==role.name;}),
            "unsupported native humanoid role");
    const auto &binding=entry.value();
    require(binding.is_object()&&binding.size()==1&&binding.contains("node"),
            "native humanoid binding requires exactly node");
    const auto &id=binding.at("node");
    require(id.is_number_integer() &&
        (id.is_number_unsigned()?id.get<uint64_t>()<mapping.size():id.get<int64_t>()>=0&&uint64_t(id.get<int64_t>())<mapping.size()),
        "native humanoid source node is out of bounds");
    const auto target=mapping[size_t(id.get<uint64_t>())];
    require(target>=0 && target<200000,"native humanoid source node has no valid emitted identity");
    mapped["humanBones"][entry.key()]["node"]=target;
  }
  stop(cancelled);
  // The existing key's move assignment is nonallocating. Every possible
  // validation/Cancel/allocation failure above leaves the caller's node exact.
  node["extras"]["domeSculptHumanoid"]=std::move(mapped);
}
} // namespace dome::detail
