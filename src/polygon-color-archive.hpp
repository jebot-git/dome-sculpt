// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/polygon-color.hpp"
namespace dome::detail {
// Shared immutable color bases are stored once, independently of the bounded
// current-corner fields using them. The outer Scene authenticates each field
// against its own exact editable topology before publication.
class PolygonColorArchive {
public:
  uint32_t add(std::shared_ptr<const PolygonColorField>);
  const std::shared_ptr<const PolygonColorField> &at(uint32_t) const;
  nlohmann::json encode() const;
  static void preflight(const nlohmann::json &bases,
                        const nlohmann::json &fields, size_t &entries);
  static PolygonColorArchive decode(const nlohmann::json &bases,
                                    const nlohmann::json &fields);
  bool empty() const { return fields_.empty(); }
  size_t entries() const { return entries_; }
  const auto &fields() const { return fields_; }

private:
  size_t entries_ = 0;
  using Basis = std::vector<PolygonCornerRgba>;
  std::vector<std::shared_ptr<const Basis>> bases_;
  std::vector<std::shared_ptr<const PolygonColorField>> fields_;
};
} // namespace dome::detail
