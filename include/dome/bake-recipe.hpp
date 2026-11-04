// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake-settings.hpp"
#include <memory>
#include <optional>
#include <string>
#include <nlohmann/json_fwd.hpp>
namespace dome {
// Scene IDs are strictly less than UINT64_MAX. Missing dependencies retain an
// explicit unavailable binding and their original ID/name, never ID0 (auto).
inline constexpr uint64_t unavailable_bake_object=UINT64_MAX;
struct BakeObjectReference {
  uint64_t object=unavailable_bake_object,original_object=0;
  std::string name;
  bool operator==(const BakeObjectReference &)const=default;
};
struct ObjectBakeRecipe {
  BakeObjectReference source;
  std::optional<BakeObjectReference> cage;
  ObjectBakeSettings settings;
  void validate()const;
  nlohmann::json encode()const;
  static std::shared_ptr<const ObjectBakeRecipe> decode(const nlohmann::json &);
};
class Scene;
std::shared_ptr<const ObjectBakeRecipe> capture_bake_recipe(
    const Scene &,uint64_t source,const ObjectBakeSettings &);
} // namespace dome
