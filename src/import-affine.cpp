// SPDX-License-Identifier: GPL-3.0-or-later
#include "import-affine.hpp"
#include "affine-matrix.hpp"
#include <algorithm>
#include <stdexcept>
namespace dome::detail {
ImportedAffineHierarchy::ImportedAffineHierarchy(std::shared_ptr<const ImportedAsset> source,
    std::span<const uint32_t> roots,size_t maximum,const std::function<bool()> &cancelled)
    :source_(std::move(source)) {
  if(!source_ || !maximum || maximum>128 || roots.empty() || roots.size()>maximum)
    throw std::invalid_argument("Invalid bounded imported affine hierarchy");
  auto stop=[&]{if(cancelled && cancelled())throw std::runtime_error("Imported affine hierarchy cancelled");};
  auto visit=[&](auto &&self,uint32_t id,int parent)->void {
    stop();
    if(id>=source_->nodes.size())throw std::invalid_argument("Imported affine node identity is missing");
    if(std::any_of(nodes_.begin(),nodes_.end(),[&](const auto&n){return n.source_node==id;}))
      throw std::invalid_argument("Imported affine hierarchy repeats a node or contains a cycle");
    if(nodes_.size()>=maximum)throw std::length_error("Imported affine hierarchy exceeds selected node budget");
    const auto &original=source_->nodes[id];
    const int expected_parent=parent<0?-1:int(nodes_.at(size_t(parent)).source_node);
    if(original.parent!=expected_parent || original.children.size()>maximum)
      throw std::invalid_argument("Imported affine hierarchy parent/children authority disagrees");
    if(!gltf_trs_matrix(original.local))
      throw std::invalid_argument("Imported local node is not a finite glTF TRS-decomposable matrix");
    AffineTransform local(original.local);
    auto world=parent<0?local:nodes_.at(size_t(parent)).world*local;
    const int slot=int(nodes_.size());
    nodes_.push_back({id,parent,local,std::move(world)});
    for(uint32_t child:original.children)self(self,child,slot);
  };
  for(uint32_t root:roots)visit(visit,root,-1);
  stop();
}
const ImportedAffineNode &ImportedAffineHierarchy::node(uint32_t id) const {
  auto found=std::find_if(nodes_.begin(),nodes_.end(),[&](const auto&n){return n.source_node==id;});
  if(found==nodes_.end())throw std::out_of_range("Node is not in the selected imported affine hierarchy");
  return *found;
}
std::vector<std::pair<uint32_t,ImportedMatrix>> ImportedAffineHierarchy::local_chain(uint32_t id) const {
  std::vector<std::pair<uint32_t,ImportedMatrix>> out;
  for(auto *n=&node(id);;) {
    out.emplace_back(n->source_node,source_->nodes.at(n->source_node).local);
    if(n->parent<0)break;
    n=&nodes_.at(size_t(n->parent));
  }
  std::reverse(out.begin(),out.end());return out;
}
AffineTransform ImportedAffineHierarchy::inverse_world(uint32_t id) const {
  AffineTransform out;
  for(auto *n=&node(id);;) {
    out=out*n->local.inverse();
    if(n->parent<0)break;
    n=&nodes_.at(size_t(n->parent));
  }
  return out;
}
AffineVector ImportedAffineHierarchy::normal_world(uint32_t id,AffineVector v) const {
  for(auto *n=&node(id);;) {
    v=n->local.normal(v);
    if(n->parent<0)break;
    n=&nodes_.at(size_t(n->parent));
  }
  return v;
}
AffineOrientation ImportedAffineHierarchy::orientation_world(uint32_t id) const {
  int sign=1;
  for(auto *n=&node(id);;) {
    sign*=int(n->local.orientation());
    if(n->parent<0)break;
    n=&nodes_.at(size_t(n->parent));
  }
  return static_cast<AffineOrientation>(sign);
}
} // namespace dome::detail
