// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/import.hpp"
#include <optional>
#include <functional>
#include <vector>
#include <nlohmann/json.hpp>
namespace dome::detail {
struct VrmSourceGraph;
struct VrmMetadataFrame {
 uint32_t source=0;
 ImportedMatrix local{};
};
struct VrmRestMetadata {
 nlohmann::json first_person;
 // Raw groups retain their source order. A frame replaces only the output node
 // reference; authored offsets/radii/force parameters remain unchanged.
 std::vector<std::optional<VrmMetadataFrame>> colliders,centers;
 bool camera_changed=false;
};
VrmRestMetadata prepare_vrm0_rest_metadata(const VrmSourceGraph &,
 const nlohmann::json &vrm,const std::function<bool()> &cancelled={});
}
