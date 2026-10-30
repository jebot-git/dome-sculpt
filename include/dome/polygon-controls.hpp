// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "polygon-authoring.hpp"
namespace dome {
struct PolygonControls {
  // Displayed scene-world distance, consistent with sculpt resolution controls.
  // Admission resolves current object/group scale; jobs carry local metres.
  float distance_mm = 10;
  unsigned cuts = 1;
  bool move_mode = false, additive = false;
};
enum class PolygonAction {
  Vertices,
  Edges,
  Faces,
  SelectAll,
  Clear,
  Undo,
  Redo,
  Extrude,
  Inset,
  Bridge,
  Dissolve,
  Weld,
  CutEdges,
  ConnectVertices,
  DeleteFaces,
  FlipFaces,
  Pick,
  BeginDrag,
  Drag,
  EndDrag,
  CancelDrag
};
} // namespace dome
