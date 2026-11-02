// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/document.hpp"
namespace dome::detail {
struct PolygonPreview {
 std::shared_ptr<const PolygonView> view;
 uint64_t object=0;
 CanvasTransform world{};
};
inline PolygonPreview polygon_preview(const EditorSnapshot &frame) {
 auto &state=*frame.state;PolygonPreview preview;
 if(state.polygon&&state.polygon->surface){preview.view=state.polygon;preview.object=state.polygon_object;}
 else if(state.polygon_handoff&&state.polygon_handoff->surface&&
   (frame.surface->generation!=state.polygon_handoff_generation||frame.surface->revision!=state.polygon_handoff_revision)){
   preview.view=state.polygon_handoff;preview.object=state.polygon_handoff_object;
 }
 if(!preview.view)return {};
 auto object=std::find_if(state.objects.begin(),state.objects.end(),[&](const auto &o){return o.id==preview.object;});
 if(object==state.objects.end()||!object->visible||!object->editable||!object->world_transform_valid)return {};
 preview.world=object->world_transform;return preview;
}
}
