// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "vrm-expression.hpp"
namespace dome {
class Document;
struct VrmExpressionEntry {
  std::string name;
  bool custom=false,binary=false;
  float weight=0;
};
// Immutable ephemeral editor state. The original owner and requested weights
// remain private; frontend callers configure the current Document session only.
class VrmExpressionView {
  std::shared_ptr<const Scene> baseline_;
  Vrm1ExpressionWeights weights_;
  friend class Document;
public:
  uint64_t generation=0,revision=0,object=0,edit_authority=0,session=0,request=0;
  std::vector<VrmExpressionEntry> entries;
  size_t selected=0;
  bool ready=false;
  std::string error;
};
inline constexpr int vrm_expression_page=29;
}
