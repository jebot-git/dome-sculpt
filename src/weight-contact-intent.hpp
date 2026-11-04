// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "heat-component-intent.hpp"
namespace dome::detail {
// Explicit graph construction, not a retry or a field normalization policy.
// Identical masks preserve label-forbidden zeros on every connected support
// block. Shared-label-only variational coupling is a different construction.
enum class DeclaredContactSupport { CommonMask, IdenticalMask, SharedSimplex };
inline bool compatible_weight_contact(const HeatJointMask &a,
                                      const HeatJointMask &b,
                                      DeclaredContactSupport policy) {
  if (policy != DeclaredContactSupport::CommonMask &&
      policy != DeclaredContactSupport::IdenticalMask &&
      policy != DeclaredContactSupport::SharedSimplex)
    throw std::invalid_argument("Unknown weight contact intent policy");
  bool common = false;
  for (size_t i = 0; i < a.size(); ++i)
    common |= (a[i] & b[i]) != 0;
  return common && (policy != DeclaredContactSupport::IdenticalMask || a == b);
}
} // namespace dome::detail
