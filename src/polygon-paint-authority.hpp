// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/polygon-scene.hpp"
#include <nlohmann/json.hpp>
namespace dome::detail {
inline bool polygon_paint_policy(const PaintedMesh *paint) {
 return paint&&paint->surface_diagnostics().policy==PaintUvPolicy::PolygonAuthored;
}
// Shape admission is always checked, including cheap display publications.
// The owning Scene/archive deep validation subsequently authenticates exact
// REST positions, triangle/corner order and UVs via validate_polygon_derivative.
inline void require_polygon_paint_owner(const PaintedMesh *paint,const PolygonAuthority *authority,bool mesh_active) {
 if(!polygon_paint_policy(paint))return;
 if(!authority||!authority->active||!mesh_active||authority->topology.faces().empty())
  throw std::invalid_argument("Polygon-authored paint requires its own nonempty active polygon authority and rest derivative");
}
inline bool polygon_paint_policy(const nlohmann::json &paint) {
 return paint.is_object()&&paint.contains("uv_policy")&&paint.at("uv_policy").is_string()&&paint.at("uv_policy").get_ref<const std::string&>()=="polygon-authored";
}
}
