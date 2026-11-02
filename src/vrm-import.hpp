// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/material-bindings.hpp"
#include "dome/vrm.hpp"
#include "import-skin.hpp"
namespace dome::detail {
enum class VrmImportVersion { None, V0, V1 };
struct VrmImportMetadata {
  VrmImportVersion version = VrmImportVersion::None;
  nlohmann::json source;
  std::shared_ptr<const std::string> vrm0_json;
  std::map<std::string, uint32_t> human_nodes;
  std::vector<uint32_t> rig_nodes;
  // Native material slot0 denotes authenticated default absence.
  std::vector<std::optional<MToonMaterial>> materials;
  std::vector<std::string> material_properties;
};
VrmImportVersion vrm_import_version(const nlohmann::json &);
VrmImportMetadata vrm_import_metadata(const nlohmann::json &,
                                      const ImportedAsset &,
                                      const std::function<bool()> &);
HumanoidMapping vrm_import_humanoid(const VrmImportMetadata &,
                                    const ImportedRig &);
std::optional<AvatarExpressions> vrm_import_visemes(const VrmImportMetadata &,
                                                    int source_mesh,
                                                    int source_node,
                                                    const MorphSet &);
std::array<PaintSourceTexture, size_t(MToonSlot::Count)>
vrm_import_roles(const MToonMaterial &, const ImportedAsset &,
                 const std::vector<std::shared_ptr<const Image>> &);
} // namespace dome::detail
