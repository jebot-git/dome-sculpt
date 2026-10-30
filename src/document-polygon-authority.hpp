// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "polygon-paint-authority.hpp"
#include "dome/scene.hpp"
namespace dome::detail {
// Scalar/UI commits may reuse a previously deep-validated immutable derivative,
// including object duplication. New geometry/authority owners must instead use
// a detached worker/replace admission that validates the complete Scene. No
// triangulation, serialization, unbounded cache or raster work occurs here.
inline void require_trusted_polygon_transition(const Scene &before,const Scene &after) {
 for(const auto &object:after.objects()){
  if(!polygon_paint_policy(object.painted.get()))continue;
  require_polygon_paint_owner(object.painted.get(),object.polygon.get(),object.mesh_active);
  const auto owner=object.painted->mesh_owner();
  const bool known=std::any_of(before.objects().begin(),before.objects().end(),[&](const auto &old){
    if(old.polygon!=object.polygon||!old.painted||!old.mesh_active)return false;
    auto old_owner=old.painted->mesh_owner();
    return old_owner==owner&&!old_owner.owner_before(owner)&&!owner.owner_before(old_owner);
  });
  if(!known)throw std::logic_error("New polygon paint geometry requires validated background publication; use the polygon or UV editor");
 }
}
}
