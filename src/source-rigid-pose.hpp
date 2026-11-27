// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "dome/source-placement.hpp"
namespace dome::detail {
struct AccessoryRig {
  uint64_t object, instance;
  std::string name;
  std::shared_ptr<const DeformationState> deformation;
};
std::vector<AccessoryRig> accessory_rigs(const Scene &);

struct AvatarAttachmentView {
  uint64_t generation, revision, object, edit_authority;
  uint32_t owner;
  std::shared_ptr<const std::vector<AccessoryRig>> rigs;
  size_t avatar = 0, bone = 0;
};

bool native_texture_set_accessory(const SculptObject &) noexcept;
bool rigid_accessory_deformation(const std::shared_ptr<const DeformationState> &) noexcept;
// Recover the exact imported endpoint from a native child's placement path.
std::shared_ptr<const SourceNodePlacement>
source_accessory_attachment(std::span<const SceneGroup>);
bool attach_source_accessory(Scene &, uint64_t accessory, uint64_t rig,
                             uint32_t joint, const std::function<bool()> &);
// Immutable imported identity, never reconstructed from names or JSON text.
std::shared_ptr<const SourceNodePlacement>
source_rig_instance(const Scene &, const SculptObject &);
// Worker-time source attachment frame before/after an atomic linked rig edit.
// The caller persists its exact world delta on the real rigid object; no frame
// depends on another live skin at render/pick time or after that skin is deleted.
std::vector<SceneGroup> source_rigid_groups(
    const Scene &, const SculptObject &, std::vector<SceneGroup>,
    const std::function<bool()> &cancelled = {});
} // namespace dome::detail
