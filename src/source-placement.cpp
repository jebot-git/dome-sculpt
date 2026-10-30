// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/source-placement.hpp"
#include "dome/scene.hpp"
#include "affine-matrix.hpp"
#include "text-utf8.hpp"
#include <algorithm>
#include <cmath>
#include <set>
#include <stdexcept>
namespace dome {
void SourceNodePlacement::validate() const {
  if(!model || !model->original_bytes || !model->asset || !model->decoded_images ||
      model->asset->mode!=ImportMode::PreserveGltf || source_node>=model->asset->nodes.size())
    throw std::invalid_argument("Original node placement lacks immutable source model/node authority");
  if(!detail::gltf_trs_matrix(model->asset->nodes[source_node].local))
    throw std::invalid_argument("Original node placement is not a representable glTF local TRS matrix");
}
AffineTransform SourceNodePlacement::local() const {
  validate();return AffineTransform(model->asset->nodes[source_node].local);
}
NativeAffineEdit::NativeAffineEdit(AffineTransform matrix):matrix_(std::move(matrix)) {
  // A native manipulation is reversible. Singular original source locals are
  // a different authority and are never smuggled in as a native edit.
  (void)matrix_.inverse();(void)matrix_.orientation();
}
AffineTransform affine_similarity(CanvasTransform t) {
  if(!ob::finite(t) || t.scale<=0)throw std::invalid_argument("Native similarity must be finite with positive scale");
  double x=t.orientation.x,y=t.orientation.y,z=t.orientation.z,w=t.orientation.w;
  const double q=x*x+y*y+z*z+w*w;
  if(!std::isfinite(q) || std::abs(q-1)>.0001)
    throw std::invalid_argument("Native similarity quaternion must be unit length");
  const double inverse=1/std::sqrt(q);x*=inverse;y*=inverse;z*=inverse;w*=inverse;
  const double s=t.scale;
  return AffineTransform(std::array<double,16>{
    (1-2*(y*y+z*z))*s,2*(x*y+w*z)*s,2*(x*z-w*y)*s,0,
    2*(x*y-w*z)*s,(1-2*(x*x+z*z))*s,2*(y*z+w*x)*s,0,
    2*(x*z+w*y)*s,2*(y*z-w*x)*s,(1-2*(x*x+y*y))*s,0,
    t.position.x,t.position.y,t.position.z,1});
}
AffineTransform group_local_matrix(const SceneGroup &g) {
  validate_transform(g.transform);auto local=affine_similarity(g.transform);
  if(g.affine_edit)local=g.affine_edit->matrix()*local;
  if(g.source_placement)local=local*g.source_placement->local();
  return local;
}
AffineTransform object_local_matrix(const SculptObject &o) {
  validate_transform(o.transform);auto local=affine_similarity(o.transform);
  return o.affine_edit?o.affine_edit->matrix()*local:local;
}
bool has_affine_placement(const SculptObject &o,std::span<const SceneGroup> groups) {
  return bool(o.affine_edit) || std::any_of(groups.begin(),groups.end(),[](const auto&g){return bool(g.affine_edit)||bool(g.source_placement);});
}
void SourcePlacementCapsule::validate() const {
  validate_transform(object_transform);
  if(groups.size()>128)throw std::length_error("Captured source placement exceeds group depth budget");
  std::set<uint64_t> ids;uint64_t parent=0;
  for(const auto &g:groups) {
    if(!g.id || g.parent!=parent || !ids.insert(g.id).second || g.name.empty() ||
        g.name.size()>256 || g.name.find('\0')!=g.name.npos || !text::valid_utf8(g.name))
      throw std::invalid_argument("Captured source placement group path is invalid");
    if(g.humanoid)g.humanoid->validate();
    (void)group_local_matrix(g);parent=g.id;
  }
  (void)world_matrix();
}
AffineTransform SourcePlacementCapsule::world_matrix() const {
  AffineTransform world;
  for(const auto &g:groups)world=world*group_local_matrix(g);
  auto local=affine_similarity(object_transform);
  if(object_edit)local=object_edit->matrix()*local;
  return world*local;
}
std::shared_ptr<const SourcePlacementCapsule> capture_source_placement(const Scene &scene,uint64_t id) {
  const auto &o=scene.object(id);auto path=scene.group_path(o.group);
  if(!has_affine_placement(o,path))return {};
  auto result=std::make_shared<SourcePlacementCapsule>();
  result->object_transform=o.transform;result->object_edit=o.affine_edit;result->groups=std::move(path);
  result->validate();return result;
}
} // namespace dome
