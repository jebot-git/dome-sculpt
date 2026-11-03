// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "normal-key-derivation.hpp"
#include "key-frame-archive.hpp"
#include "dome/creator-archive.hpp"
namespace dome::detail {
// Material10 supplies these earlier owning contexts. This record cannot decode
// from an unbound frame2/geometry leaf or from guessed geometry equality.
class KeyDerivationCodec {
public:
  static nlohmann::json encode(const SourceKeyDerivation &,NormalFrameArchive &,
      uint32_t paint,uint32_t before_binding,CreatorAssetArchive &,PaintImageArchive &);
  using PaintResolver=std::function<std::shared_ptr<const PaintedMesh>(uint32_t,
      const std::shared_ptr<const SourceMaterialBindings> &)>;
  static std::shared_ptr<const SourceKeyDerivation> decode(const nlohmann::json &,
      uint32_t current_binding,
      const std::vector<std::shared_ptr<const SourceMaterialBindings>> &earlier,
      const PaintResolver &,KeyFrameArchive &,const CreatorAssetArchive &,
      const std::function<bool()> &cancelled={});
};
} // namespace dome::detail
