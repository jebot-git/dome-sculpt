// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/document.hpp"
#include "affine-surface.hpp"
namespace dome {
// Immutable detached preview authority. Original source/local node matrices are
// never replaced by a fitted Canvas pose. The pivot is only a tracking anchor.
struct Document::HeldAffine {
  detail::AffineInteractionFrame parent;
  std::shared_ptr<const NativeAffineEdit> base_edit,preview_edit;
  CanvasTransform base_pivot{},preview_pivot{};
  explicit HeldAffine(const Scene &scene,uint64_t id)
    :parent(detail::placement_frame(scene.group_path(scene.object(id).group),{},{})),
     base_edit(scene.object(id).affine_edit),preview_edit(base_edit) { rebase(scene.object(id).transform); }
  AffineTransform world(CanvasTransform local)const {
    return parent.world()*(preview_edit?preview_edit->matrix():AffineTransform{})*affine_similarity(local);
  }
  void rebase(CanvasTransform local) {
    base_edit=preview_edit;const auto position=detail::affine_position(world(local).point({0,0,0}));
    base_pivot={{position.x,position.y,position.z},{},1};preview_pivot=base_pivot;
  }
  void move(CanvasTransform delta) {
    if(delta.position.x==0&&delta.position.y==0&&delta.position.z==0&&delta.orientation.x==0&&delta.orientation.y==0&&delta.orientation.z==0&&delta.orientation.w==1&&delta.scale==1){preview_edit=base_edit;preview_pivot=base_pivot;return;}
    const auto local=parent.inverse()*affine_similarity(delta)*parent.world()*(base_edit?base_edit->matrix():AffineTransform{});
    preview_edit=std::make_shared<const NativeAffineEdit>(local);
    preview_pivot=ob::compose(delta,base_pivot);
  }
};
} // namespace dome
