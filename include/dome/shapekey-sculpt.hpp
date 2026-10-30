// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "deformation.hpp"
#include "paint.hpp"
namespace dome {
struct ShapeSculptOptions {
 size_t maximum_vertices=2'000'000,maximum_work=100'000'000;
 bool front_faces_only=true;
};
// One detached gesture in the selected target's full-weight posed surface.
// Brush coordinates/radius are world metres. Results are unposed REST-domain
// target positions suitable for source-retaining author_shapekey(Replace).
class ShapeSculptDraft {
public:
 ShapeSculptDraft(std::shared_ptr<const PaintedMesh>,
   std::shared_ptr<const DeformationState>,size_t target,const RigMatrix &world,
   ShapeSculptOptions={},const std::function<bool()> &cancelled={});
 bool stamp(const Brush &,const std::function<bool()> &cancelled={});
 std::span<const Vec3> positions()const{return positions_;}
 Mesh preview()const;
 bool changed()const{return changed_;}
private:
 std::shared_ptr<const PaintedMesh> paint_;
 std::shared_ptr<const DeformationState> deformation_;
 std::vector<Vec3> original_,positions_,world_positions_,world_normals_;
 std::vector<float> coverage_;
 std::vector<std::vector<uint32_t>> neighbors_;
 std::vector<RigMatrix> joints_;
 RigMatrix world_inverse_{};
 ShapeSculptOptions options_;
 size_t work_=0;
 bool changed_=false;
 void consume(size_t,const std::function<bool()> &);
 Vec3 unpose_delta(size_t,Vec3)const;
};
} // namespace dome
