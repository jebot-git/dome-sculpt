// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/paint.hpp"
namespace dome::detail {
// Private native-weight staging only. Cache/history are not authoring values.
// The immutable SurfaceMap is counted by its complete owner, not mesh alias.
struct NativeWeightPaintAccess {
  struct Surface { std::shared_ptr<const void> owner; size_t bytes=0; };
  static Surface surface(const PaintedMesh &,bool prospective=false);
  static bool same_current(const PaintedMesh &,const PaintedMesh &) noexcept;
  static std::shared_ptr<PaintedMesh> snapshot(const PaintedMesh &);
};
}
