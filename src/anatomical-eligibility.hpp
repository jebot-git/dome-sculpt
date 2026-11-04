// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include "dome/creator-archive.hpp"
#include "dome/material-bindings.hpp"
namespace dome::detail {
// Cheap UI hint only. Actual completeness, REST/corner/morph/placement and
// whole owning closure are authenticated on the worker; no geometry scan here.
inline bool anatomical_source_target(const SculptObject&o){
 if(!o.mesh_active||!o.painted||!o.volume||o.painted->in_stroke()||o.volume->in_stroke()||!o.group||o.retopology||o.polygon||!o.appearance)return false;
 const auto&a=*o.appearance;
 if(!a.asset||!a.original_bytes||!a.asset->skins.empty()||!a.source_joint_nodes.empty()||a.source_node<0||size_t(a.source_node)>=a.asset->nodes.size()||a.asset->nodes[size_t(a.source_node)].skin>=0||a.asset->primitives.empty()||a.asset->primitives.size()>64||a.source_vertices.size()!=o.painted->mesh().positions.size())return false;
 if(o.painted->paint_sampling_attachment()||o.painted->color_projection()||o.painted->normal_projection())return false;
 if(o.material_bindings&&(o.material_bindings->color_preparation||o.material_bindings->normal_preparation||o.material_bindings->key_derivation||(o.material_bindings->normal_frames&&o.material_bindings->normal_frames->position_domain)))return false;
 return !o.painted->mesh().indices.empty();
}
} // namespace dome::detail
