// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/render.hpp"
#include "dome/polygon-render.hpp"
#include "polygon-view-state.hpp"
namespace dome::detail {
inline void draw_polygon_preview(PolygonRenderer &renderer,const PolygonPreview &preview,CanvasTransform canvas,const Mat4 &vp,uint32_t accent) {
 if(!preview.view)return;
 auto world=model_matrix(ob::compose(canvas,preview.world));
 renderer.draw(*preview.view,multiply(vp,world),world,accent,true);
}
}
