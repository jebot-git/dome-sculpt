// SPDX-License-Identifier: GPL-3.0-or-later
#include "source-placement-archive.hpp"
#include "dome/scene.hpp"
#include "text-utf8.hpp"
#include <algorithm>
#include <nlohmann/json.hpp>
namespace dome::detail {
namespace {
constexpr size_t maximum_edits=4096,maximum_nodes=4096,maximum_capsules=512,maximum_groups=65536;
uint64_t number(const nlohmann::json &j) {
  if(!j.is_number_integer() || (!j.is_number_unsigned()&&j.get<int64_t>()<0))throw std::invalid_argument("Invalid source-placement integer");
  return j.get<uint64_t>();
}
CanvasTransform transform(const nlohmann::json &j) {
  for(const char *key:{"translation","rotation"})for(const auto &x:j.at(key))if(!x.is_number())throw std::invalid_argument("Captured native transform components must be numeric");
  if(!j.at("scale").is_number())throw std::invalid_argument("Captured native scale must be numeric");
  const auto p=j.at("translation").get<std::array<float,3>>();
  const auto q=j.at("rotation").get<std::array<float,4>>();
  CanvasTransform t{{p[0],p[1],p[2]},{q[0],q[1],q[2],q[3]},j.at("scale").get<float>()};validate_transform(t);return t;
}
nlohmann::json transform_json(CanvasTransform t) {
  return {{"translation",{t.position.x,t.position.y,t.position.z}},
          {"rotation",{t.orientation.x,t.orientation.y,t.orientation.z,t.orientation.w}},{"scale",t.scale}};
}
template<class T>uint32_t find(const std::vector<std::shared_ptr<const T>> &table,const std::shared_ptr<const T> &p) {
  for(size_t i=0;i<table.size();++i)if(table[i]==p)return uint32_t(i);
  throw std::logic_error("Source-placement dependency was not registered");
}
template<class T>uint32_t append(std::vector<std::shared_ptr<const T>>&table,std::shared_ptr<const T>p,size_t maximum) {
  if(!p)throw std::invalid_argument("Missing immutable source-placement owner");
  for(size_t i=0;i<table.size();++i)if(table[i]==p)return uint32_t(i);
  if(table.size()>=maximum)throw std::length_error("Source-placement immutable table quota exceeded");
  table.push_back(std::move(p));return uint32_t(table.size()-1);
}
void group_preflight(const nlohmann::json &g,size_t edits,size_t nodes) {
  if(!g.is_object() || g.size()<7 || g.size()>9)throw std::invalid_argument("Invalid captured source group shape");
  for(auto it=g.begin();it!=g.end();++it)if(it.key()!="id"&&it.key()!="parent"&&it.key()!="name"&&it.key()!="translation"&&it.key()!="rotation"&&it.key()!="scale"&&it.key()!="humanoid"&&it.key()!="source_placement"&&it.key()!="affine_edit")throw std::invalid_argument("Unknown captured source group field");
  if(!number(g.at("id")))throw std::invalid_argument("Captured source group ID must be positive");
  (void)number(g.at("parent"));(void)transform(g);
  const auto &name=g.at("name").get_ref<const std::string&>();
  if(name.empty()||name.size()>256||name.find('\0')!=name.npos||!text::valid_utf8(name))throw std::invalid_argument("Invalid captured source group name");
  if(g.contains("source_placement"))placement_index(g.at("source_placement"),nodes);
  if(g.contains("affine_edit"))placement_index(g.at("affine_edit"),edits);
  if(!g.at("humanoid").is_null())(void)HumanoidControls::decode(g.at("humanoid"));
}
}
uint32_t placement_index(const nlohmann::json &j,size_t count) {
  const auto id=number(j);if(id>=count || id>UINT32_MAX)throw std::invalid_argument("Source-placement reference is outside its table");return uint32_t(id);
}
uint32_t SourcePlacementArchive::add_edit(std::shared_ptr<const NativeAffineEdit> p){return append(edits_,std::move(p),maximum_edits);}
uint32_t SourcePlacementArchive::add_node(std::shared_ptr<const SourceNodePlacement> p){if(p)p->validate();return append(nodes_,std::move(p),maximum_nodes);}
uint32_t SourcePlacementArchive::add_capsule(std::shared_ptr<const SourcePlacementCapsule> p) {
  for(size_t i=0;i<capsules_.size();++i)if(capsules_[i]==p)return uint32_t(i);
  if(!p)throw std::invalid_argument("Missing source-placement capsule");
  p->validate();
  if(p->groups.size()>maximum_groups-captured_groups_)throw std::length_error("Captured source group quota exceeded");
  // Detached callers discard a failed archive; no Scene/source owner mutates.
  if(p->object_edit)add_edit(p->object_edit);
  for(const auto &g:p->groups){if(g.affine_edit)add_edit(g.affine_edit);if(g.source_placement)add_node(g.source_placement);}
  const auto result=append(capsules_,p,maximum_capsules);captured_groups_+=p->groups.size();return result;
}
std::shared_ptr<const NativeAffineEdit> SourcePlacementArchive::edit(uint32_t i)const{if(i>=edits_.size())throw std::out_of_range("Missing native affine edit");used_edits_.insert(i);return edits_[i];}
std::shared_ptr<const SourceNodePlacement> SourcePlacementArchive::node(uint32_t i)const{if(i>=nodes_.size())throw std::out_of_range("Missing source node placement");used_nodes_.insert(i);return nodes_[i];}
std::shared_ptr<const SourcePlacementCapsule> SourcePlacementArchive::capsule(uint32_t i)const{if(i>=capsules_.size())throw std::out_of_range("Missing source placement capsule");used_capsules_.insert(i);return capsules_[i];}
nlohmann::json SourcePlacementArchive::encode(PaintImageArchive &images,CreatorAssetArchive &models) const {
  nlohmann::json j={{"version",1},{"edits",nlohmann::json::array()},{"nodes",nlohmann::json::array()},{"capsules",nlohmann::json::array()}};
  for(const auto &p:edits_)j["edits"].push_back(p->matrix().matrix());
  for(const auto &p:nodes_)j["nodes"].push_back({{"model",models.add_model(*p->model,images)},{"node",p->source_node},{"instance",p->source_instance}});
  for(const auto &p:capsules_) {
    auto value=transform_json(p->object_transform);value["edit"]=p->object_edit?nlohmann::json(find(edits_,p->object_edit)):nlohmann::json(nullptr);value["groups"]=nlohmann::json::array();
    for(const auto &g:p->groups) {
      auto group=transform_json(g.transform);group["id"]=g.id;group["parent"]=g.parent;group["name"]=g.name;group["humanoid"]=g.humanoid?g.humanoid->encode():nlohmann::json(nullptr);
      if(g.source_placement)group["source_placement"]=find(nodes_,g.source_placement);
      if(g.affine_edit)group["affine_edit"]=find(edits_,g.affine_edit);
      value["groups"].push_back(std::move(group));
    }
    j["capsules"].push_back(std::move(value));
  }
  return j;
}
void SourcePlacementArchive::preflight(const nlohmann::json &j,size_t models) {
  if(!j.is_object()||j.size()!=4||!j.at("version").is_number_integer()||j.at("version")!=1 || !j.at("edits").is_array()||j.at("edits").size()>maximum_edits ||
      !j.at("nodes").is_array()||j.at("nodes").size()>maximum_nodes || !j.at("capsules").is_array()||j.at("capsules").size()>maximum_capsules)
    throw std::invalid_argument("Invalid Scene13 source-placement archive shape/quota");
  for(const auto &e:j.at("edits")){if(!e.is_array()||e.size()!=16)throw std::invalid_argument("Invalid native affine edit matrix shape");for(const auto &x:e)if(!x.is_number())throw std::invalid_argument("Native affine edit components must be numeric");(void)NativeAffineEdit(AffineTransform(e.get<std::array<double,16>>()));}
  for(const auto &n:j.at("nodes")) {
    if(!n.is_object()||n.size()!=3||number(n.at("model"))>=models || number(n.at("node"))>=100000)
      throw std::invalid_argument("Invalid source model/node placement reference");
    (void)number(n.at("instance"));
  }
  size_t total=0;
  for(const auto &c:j.at("capsules")) {
    if(!c.is_object()||c.size()!=5||!c.at("groups").is_array()||c.at("groups").size()>128 || c.at("groups").size()>maximum_groups-total)
      throw std::invalid_argument("Invalid captured source-placement shape/quota");
    total+=c.at("groups").size();(void)transform(c);
    if(!c.at("edit").is_null())placement_index(c.at("edit"),j.at("edits").size());
    std::set<uint64_t> ids;uint64_t parent=0;
    for(const auto &g:c.at("groups")){group_preflight(g,j.at("edits").size(),j.at("nodes").size());const auto id=number(g.at("id"));if(number(g.at("parent"))!=parent||!ids.insert(id).second)throw std::invalid_argument("Captured source group path is cyclic/incomplete");parent=id;}
  }
}
SourcePlacementArchive SourcePlacementArchive::decode(const nlohmann::json &j,const CreatorAssetArchive &models) {
  preflight(j,models.size());SourcePlacementArchive out;
  for(const auto &e:j.at("edits"))out.edits_.push_back(std::make_shared<const NativeAffineEdit>(AffineTransform(e.get<std::array<double,16>>())));
  for(const auto &n:j.at("nodes")){auto node=std::make_shared<SourceNodePlacement>();node->model=models.model(uint32_t(number(n.at("model"))));node->source_node=uint32_t(number(n.at("node")));node->source_instance=number(n.at("instance"));node->validate();out.nodes_.push_back(std::move(node));}
  for(const auto &c:j.at("capsules")) {
    auto capsule=std::make_shared<SourcePlacementCapsule>();capsule->object_transform=transform(c);
    if(!c.at("edit").is_null())capsule->object_edit=out.edit(placement_index(c.at("edit"),out.edits_.size()));
    for(const auto &g:c.at("groups")) {
      SceneGroup group;group.id=number(g.at("id"));group.parent=number(g.at("parent"));group.name=g.at("name").get<std::string>();group.transform=transform(g);
      if(!g.at("humanoid").is_null())group.humanoid=std::make_shared<const HumanoidControls>(HumanoidControls::decode(g.at("humanoid")));
      if(g.contains("source_placement"))group.source_placement=out.node(placement_index(g.at("source_placement"),out.nodes_.size()));
      if(g.contains("affine_edit"))group.affine_edit=out.edit(placement_index(g.at("affine_edit"),out.edits_.size()));
      capsule->groups.push_back(std::move(group));
    }
    capsule->validate();out.captured_groups_+=capsule->groups.size();out.capsules_.push_back(std::move(capsule));
  }
  return out;
}
void SourcePlacementArchive::require_reachable()const {
  if(used_capsules_.size()!=capsules_.size()||used_nodes_.size()!=nodes_.size()||used_edits_.size()!=edits_.size())
    throw std::invalid_argument("Source-placement archive contains orphan immutable records");
}
} // namespace dome::detail
