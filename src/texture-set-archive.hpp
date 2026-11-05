// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/texture-sets.hpp"
#include "dome/creator-archive.hpp"
#include <nlohmann/json.hpp>
#include <set>
namespace dome::detail {
// Scene40 owns this archive. A geometry table entry is shared by every set and
// retained collection that actually references the same immutable owner. This
// does not grant source/material/normal authority to a membership record.
class TextureSetArchive {
public:
  explicit TextureSetArchive(TextureSetLimits limits = {});
  nlohmann::json encode(const SculptObject &,PaintImageArchive &,NormalSamplingArchive *,PaintSamplingArchive *);
  nlohmann::json geometry_records() const;
  static TextureSetArchive decode_geometry(const nlohmann::json &,TextureSetLimits limits = {});
  void decode(SculptObject &,const nlohmann::json &,const PaintImageArchive &,
      const NormalSamplingArchive *,const PaintSamplingArchive *,const SourcePaintDecodeContext &);
  void require_reachable() const;
private:
  TextureSetLimits limits_;
  size_t bytes_=0,work_=0,sets_=0;
  std::vector<std::shared_ptr<const TextureSetTopology>> topology_;
  mutable std::set<uint32_t> reached_;
  void charge(size_t,size_t);
  void step(size_t=1);
  uint32_t add(const std::shared_ptr<const TextureSetTopology> &);
};
} // namespace dome::detail
