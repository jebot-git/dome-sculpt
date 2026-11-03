// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <functional>
#include <map>
#include <nlohmann/json.hpp>
namespace dome::detail {
// Each source mesh NODE can emit multiple primitive leaves. VRM1 expression
// and first-person binds address nodes, unlike VRM0's mesh identities.
struct Vrm1SurfaceIdentity {
  int node = -1;
  std::vector<int> morph_targets;
};
struct Vrm1IdentityMap {
  std::vector<int> nodes, images;
  std::vector<std::vector<Vrm1SurfaceIdentity>> surfaces;
  std::vector<std::vector<int>> materials;
};
void validate_vrm1_metadata(const nlohmann::json &source,
                            const std::function<bool()> &cancelled = {},
                            bool standard_export = false);
// Returns the versioned root extensions. Node constraints are remapped onto
// the emitted original endpoints separately, never onto generated mesh leaves.
nlohmann::json remap_vrm1_metadata(const nlohmann::json &source,
                                   const Vrm1IdentityMap &,
                                   const std::function<bool()> &cancelled = {});
void remap_vrm1_constraints(nlohmann::json &output,
                            const nlohmann::json &source,
                            const Vrm1IdentityMap &,
                            const std::function<bool()> &cancelled = {});
} // namespace dome::detail
