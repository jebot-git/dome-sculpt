// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/import.hpp"
#include "dome/rig.hpp"
#include <nlohmann/json.hpp>
namespace dome::detail {
// Node-local editor property, not a VRM humanoid limit, end-site or collider.
// Source identities index this table; each native rig uses its actual slot map.
struct SourceBoneMetadata {
  std::vector<std::optional<nlohmann::json>> extras;
  std::vector<std::optional<float>> lengths;
};
std::optional<float> bone_length_record(const nlohmann::json &extras);
SourceBoneMetadata source_bone_metadata(const ImportedAsset &,
                                       const std::function<bool()> & = {});
// Preserves unrelated extras. A matching record stays verbatim, while an
// incompatible existing namespace/type refuses before modifying the node.
bool write_bone_length(nlohmann::json &node, float length);
// Detached import only; failure/Cancel leaves the caller's Skeleton exact.
size_t apply_bone_lengths(Skeleton &, std::span<const uint32_t>,
                         const SourceBoneMetadata &,
                         const std::function<bool()> & = {});
} // namespace dome::detail
