// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/affine.hpp"
#include "dome/import.hpp"
namespace dome::detail {
struct ImportedAffineNode {
  uint32_t source_node=0;
  int parent=-1; // selected hierarchy slot, not a source node index
  AffineTransform local,world;
};
// Detached immutable source graph authority. Keeps source node identities and
// exact local float matrices; computed world shear is never an export node.
// This foundation does NOT grant Scene affine adoption or remove import guards.
class ImportedAffineHierarchy {
public:
  ImportedAffineHierarchy(std::shared_ptr<const ImportedAsset>,
      std::span<const uint32_t> roots,size_t maximum_nodes=128,
      const std::function<bool()> &cancelled={});
  const std::shared_ptr<const ImportedAsset> &source() const noexcept{return source_;}
  const std::vector<ImportedAffineNode> &nodes() const noexcept{return nodes_;}
  const ImportedAffineNode &node(uint32_t source_node) const;
  // Root-to-leaf original locals, exact bit patterns and source identities.
  std::vector<std::pair<uint32_t,ImportedMatrix>> local_chain(uint32_t) const;
  // Reverse original local inverses, avoiding decomposition of world shear.
  AffineTransform inverse_world(uint32_t) const;
  // Preserve the original factorization for normals too. Extreme anisotropy
  // can make flattened world axes nearly parallel despite valid local TRS.
  AffineVector normal_world(uint32_t,AffineVector) const;
  AffineOrientation orientation_world(uint32_t) const;
private:
  std::shared_ptr<const ImportedAsset> source_;
  std::vector<ImportedAffineNode> nodes_;
};
} // namespace dome::detail
