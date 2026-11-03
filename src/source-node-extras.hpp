// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include <nlohmann/json.hpp>
#include <functional>
#include <span>
namespace dome::detail {
// Remap only known current-node references. All unrelated source extras retain
// their JSON values; the immutable source archive is never rewritten.
void remap_source_node_extras(nlohmann::json &,std::span<const int>,
                             const std::function<bool()> &cancelled={});
} // namespace dome::detail
