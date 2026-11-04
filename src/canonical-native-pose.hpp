// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "canonical-normal-uv.hpp"
#include "dome/scene.hpp"
#include "dome/tangents.hpp"
namespace dome::detail {
struct CanonicalNativePoseOptions {
  size_t max_working_bytes=256*1024*1024;
  size_t max_work=128*1024*1024;
  std::function<bool()> cancelled;
};
// Evaluated legacy native directions, not a fabricated portable/intrinsic
// normal preparation. The editable atlas and original normal smoothing/UV
// domains remain separate. This value grants no geometry editing authority.
class CanonicalNativePose {
public:
  static std::shared_ptr<const CanonicalNativePose> create(
      const SculptObject &,const CanonicalNativePoseOptions & = {});
  bool valid_owner(const std::shared_ptr<const CanonicalNativePose> &p) const noexcept {
    return p.get()==this&&!self_.expired()&&!self_.owner_before(p)&&!p.owner_before(self_);
  }
  const auto &origin() const noexcept {return origin_;}
  const auto &uv() const noexcept {return uv_;}
  const auto &rest() const noexcept {return rest_;}
  const auto &mesh() const noexcept {return posed_;}
  const auto &tangents() const noexcept {return tangents_;}
  const auto &root_vertices() const noexcept {return root_vertices_;}
  size_t working_bytes() const noexcept {return working_bytes_;}
private:
  CanonicalNativePose()=default;
  CanonicalNativePose(const CanonicalNativePose&)=delete;
  CanonicalNativePose &operator=(const CanonicalNativePose&)=delete;
  std::weak_ptr<const CanonicalNativePose> self_;
  std::shared_ptr<const CanonicalNormalConversion> origin_;
  std::shared_ptr<const CanonicalNormalUv> uv_;
  std::shared_ptr<const Mesh> rest_;
  std::shared_ptr<const DeformationState> evaluation_;
  Mesh posed_;
  std::vector<Tangent> tangents_;
  std::vector<uint32_t> root_vertices_;
  size_t working_bytes_=0;
};
} // namespace dome::detail
