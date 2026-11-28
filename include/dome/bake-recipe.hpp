// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "bake-settings.hpp"
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <vector>
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
struct BakeTopologyRefinement {
  uint64_t before=0, after=0;
  double plane=0;
  float tolerance=0;
  bool operator==(const BakeTopologyRefinement &)const=default;
};
struct ObjectBakeRecipe {
  BakeObjectReference source;
  std::optional<BakeObjectReference> cage;
  ObjectBakeSettings settings;
  std::vector<BakeObjectReference> additional_sources;
  // Replay on the actual Shape Restore mesh before using its ordered cage or
  // donor. The recipe stores parameters, never arbitrary correspondence rows.
  std::optional<BakeTopologyRefinement> refinement;
  void validate()const;
  nlohmann::json encode()const;
  static std::shared_ptr<const ObjectBakeRecipe> decode(const nlohmann::json &);
};
class Scene;
std::shared_ptr<const ObjectBakeRecipe> capture_bake_recipe(
    const Scene &,uint64_t source,const ObjectBakeSettings &);
std::shared_ptr<const ObjectBakeRecipe> capture_bake_recipe(
    const Scene &,std::span<const uint64_t> sources,const ObjectBakeSettings &);
} // namespace dome
