// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "canonical-normal-owner.hpp"
#include "dome/normal-sampling.hpp"
namespace dome::detail {
struct CanonicalNormalUvOptions {
  size_t max_working_bytes=256*1024*1024;
  std::function<bool()> cancelled;
};
// Exact correspondence from an editable UV atlas to the original conversion.
// All mappings are flattened to the authentic root; repeated edits do not keep
// raster workspaces or an unbounded chain of earlier UV meshes alive.
class CanonicalNormalUv {
public:
  static std::shared_ptr<const CanonicalNormalUv> create(
      std::shared_ptr<const CanonicalNormalConversion>,const Mesh &,
      std::span<const uint32_t>,const CanonicalNormalUvOptions & = {});
  std::shared_ptr<const CanonicalNormalUv> reparameterize(const Mesh &,
      std::span<const uint32_t>,const CanonicalNormalUvOptions & = {}) const;
  bool valid_owner(const std::shared_ptr<const CanonicalNormalUv> &p) const noexcept {
    return p.get()==this&&!owner_.expired()&&!owner_.owner_before(p)&&!p.owner_before(owner_);
  }
  const auto &origin() const noexcept {return origin_;}
  const auto &layout() const noexcept {return layout_;}
  const auto &mesh() const noexcept {return mesh_;}
  const auto &coordinates() const noexcept {return coordinates_;}
  size_t storage_bytes() const noexcept {return bytes_;}
  void validate_current(const Mesh &,const std::function<bool()> & = {}) const;
private:
  CanonicalNormalUv()=default;
  CanonicalNormalUv(const CanonicalNormalUv&)=delete;
  CanonicalNormalUv &operator=(const CanonicalNormalUv&)=delete;
  static std::shared_ptr<const CanonicalNormalUv> build(
      std::shared_ptr<const CanonicalNormalConversion>,const Mesh &,
      std::span<const uint32_t>,const CanonicalNormalUvOptions &);
  std::weak_ptr<const CanonicalNormalUv> owner_;
  std::shared_ptr<const CanonicalNormalConversion> origin_;
  std::shared_ptr<const SourceNormalUvLayout> layout_;
  std::shared_ptr<const Mesh> mesh_;
  std::vector<std::array<float,2>> coordinates_;
  size_t bytes_=0;
};
} // namespace dome::detail
