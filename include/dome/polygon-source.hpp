// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "polygon.hpp"
namespace dome {
struct CreatorAppearance;
struct PolygonSourceTopology {
  PolygonMesh topology;
  // Each derived packed corner -> exact old rest-mesh vertex. This witnesses
  // correspondence only; it does not authorize rig/paint transport by itself.
  std::vector<uint32_t> source_vertices;
  bool authored_ngons = false;
};
// Detached bounded topology preparation. Rest geometry and immutable source
// remain untouched. Original face loops are recovered only from the checked
// native-adapter witness; arbitrary coplanar triangles are never guessed quads.
PolygonSourceTopology
prepare_polygon_topology(const Mesh &rest,
                         const CreatorAppearance *appearance = nullptr,
                         const PolygonLimits & = {});
} // namespace dome
