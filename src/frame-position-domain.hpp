// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "corner-key-expansion.hpp"
#include <memory>
#include <set>
namespace dome {
struct SourceMaterialBindings;
namespace detail {
// Native frame POSITION authority only. SourceMaterialBindings::uv_origin and
// NormalFrames::source_* retain their separate actual original attribute IDs.
class FramePositionDomain {
public:
  ~FramePositionDomain()=default;
  const std::shared_ptr<const Mesh> &basis() const noexcept{return basis_;}
  const std::shared_ptr<const Mesh> &before_basis() const noexcept{return before_;}
  const std::shared_ptr<const Mesh> &current_geometry() const noexcept{return current_;}
  const std::shared_ptr<const Mesh> &source_frame_basis() const noexcept{return source_;}
  const std::shared_ptr<const FramePositionDomain> &previous() const noexcept{return previous_;}
  const std::vector<uint32_t> &current_vertices() const noexcept{return current_vertices_;}
  const std::vector<uint32_t> &origin_vertices() const noexcept{return origin_vertices_;}
  const std::vector<uint32_t> &source_frame_vertices() const noexcept{return source_vertices_;}
  size_t storage_bytes() const;
  void charge_storage(size_t &,size_t,std::set<const FramePositionDomain*> &,std::set<const Mesh*> &) const;
  void validate(bool deep=true,const std::function<bool()> &cancelled={}) const;
private:
  FramePositionDomain()=default;
  FramePositionDomain(const FramePositionDomain&)=delete;
  FramePositionDomain &operator=(const FramePositionDomain&)=delete;
  std::shared_ptr<const Mesh> basis_,before_,current_,source_;
  std::shared_ptr<const FramePositionDomain> previous_;
  std::vector<uint32_t> current_vertices_,origin_vertices_,source_vertices_;
  friend std::shared_ptr<const FramePositionDomain> derive_frame_position_domain(
      std::shared_ptr<const Mesh>,const SourceMaterialBindings &,
      const std::function<bool()> &);
};
std::shared_ptr<const FramePositionDomain> derive_frame_position_domain(
    std::shared_ptr<const Mesh> current,const SourceMaterialBindings &,
    const std::function<bool()> &cancelled={});
const Mesh &frame_position_basis(const SourceMaterialBindings &);
std::vector<std::array<float,2>> frame_position_coordinates(
    const SourceMaterialBindings &,uint32_t,
    const std::function<bool()> &cancelled={});
} // namespace detail
} // namespace dome
