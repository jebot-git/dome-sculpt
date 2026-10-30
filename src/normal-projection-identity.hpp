// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/paint.hpp"
namespace dome::detail {
inline bool normal_projection_identity(const PaintNormalProjection &a,const PaintNormalProjection &b){
  return a.pbr==b.pbr&&a.mtoon==b.mtoon&&!a.pbr.owner_before(b.pbr)&&!b.pbr.owner_before(a.pbr)&&!a.mtoon.owner_before(b.mtoon)&&!b.mtoon.owner_before(a.mtoon);
}
} // namespace dome::detail
