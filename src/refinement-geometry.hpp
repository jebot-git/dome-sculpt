// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "key-mirror-refinement.hpp"
#include "dome/paint-sampling.hpp"
namespace dome::detail {
// Pure geometry/stencil leaf. No Paint/material/model/normal recipe edges.
// Each consumer must independently authenticate its actual before owner.
class RefinementGeometry final {
public:
 static std::shared_ptr<const RefinementGeometry> create(std::shared_ptr<const Mesh>,
     KeyMirrorPlane,float tolerance,const PaintSamplingLimits & = {},size_t later_bytes_per_corner=0);
 bool valid_owner(const std::shared_ptr<const RefinementGeometry> &) const noexcept;
 const auto &refinement() const noexcept{return refinement_;}
 const auto &source() const noexcept{return refinement_.source;}
 const auto &mesh() const noexcept{return mesh_;}
 float tolerance() const noexcept{return tolerance_;}
 size_t working_bytes() const noexcept{return working_bytes_;}
 bool matches_source(const std::shared_ptr<const Mesh> &) const noexcept;
 size_t retained_bytes() const;
 void validate(const PaintSamplingLimits & = {}) const;
private:
 RefinementGeometry()=default;
 RefinementGeometry(const RefinementGeometry&)=delete;
 RefinementGeometry&operator=(const RefinementGeometry&)=delete;
 std::weak_ptr<const RefinementGeometry> self_;
 std::weak_ptr<const Mesh> source_owner_;
 KeyMirrorRefinement refinement_;
 std::shared_ptr<const Mesh> mesh_;
 float tolerance_=0;
 size_t working_bytes_=0,later_bytes_per_corner_=0;
};
}
