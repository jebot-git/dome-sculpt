// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/normal-frames.hpp"
#include "frame-position-domain.hpp"
namespace dome::detail {
// Private owning-context codec. A frame2 record is never a standalone proof.
// Its domain is reconstructed only from the actual earlier material and Paint
// geometry selected by the owning key record. No equal-valued mesh interning.
class KeyFrameArchive {
public:
  explicit KeyFrameArchive(const nlohmann::json &);
  static nlohmann::json encode(const NormalFrameArchive &);
  const std::shared_ptr<const NormalFrameSet> &at(uint32_t) const;
  std::shared_ptr<const NormalFrameSet> bind(uint32_t,
      std::shared_ptr<const Mesh> current,
      const SourceMaterialBindings &before,
      const std::function<bool()> &cancelled={});
  void require_complete() const;
private:
  friend class KeyDerivationCodec;
  const nlohmann::json &input_;
  NormalFrameArchive frames_;
  std::vector<std::optional<uint32_t>> references_;
  std::vector<std::shared_ptr<const FramePositionDomain>> domains_;
  std::vector<bool> used_;
};
} // namespace dome::detail
