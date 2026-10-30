// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/source-placement.hpp"
#include <set>
namespace dome::detail {
class SourcePlacementArchive {
public:
  uint32_t add_edit(std::shared_ptr<const NativeAffineEdit>);
  uint32_t add_node(std::shared_ptr<const SourceNodePlacement>);
  uint32_t add_capsule(std::shared_ptr<const SourcePlacementCapsule>);
  std::shared_ptr<const NativeAffineEdit> edit(uint32_t) const;
  std::shared_ptr<const SourceNodePlacement> node(uint32_t) const;
  std::shared_ptr<const SourcePlacementCapsule> capsule(uint32_t) const;
  bool empty()const noexcept{return edits_.empty()&&nodes_.empty()&&capsules_.empty();}
  const auto &nodes()const noexcept{return nodes_;}
  nlohmann::json encode(PaintImageArchive &,CreatorAssetArchive &) const;
  static void preflight(const nlohmann::json &,size_t models);
  static SourcePlacementArchive decode(const nlohmann::json &,const CreatorAssetArchive &);
  void require_reachable()const;
private:
  size_t captured_groups_=0;
  std::vector<std::shared_ptr<const NativeAffineEdit>> edits_;
  std::vector<std::shared_ptr<const SourceNodePlacement>> nodes_;
  std::vector<std::shared_ptr<const SourcePlacementCapsule>> capsules_;
  mutable std::set<uint32_t> used_edits_,used_nodes_,used_capsules_;
};
uint32_t placement_index(const nlohmann::json &,size_t count);
} // namespace dome::detail
