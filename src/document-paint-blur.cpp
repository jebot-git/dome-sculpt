// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/document.hpp"
#include "texture-set-stroke.hpp"
#include "document-authority.hpp"
namespace dome {
void Document::blur_color_layer(PaintBlurOptions options,AssetSource expected){
 std::lock_guard lock(model_mutex_);queue_blur_color_locked(options,expected);
}
void Document::queue_blur_color_locked(PaintBlurOptions options,AssetSource expected){
 require_authority_locked(expected);
 if(options.allow_software_validation)throw std::invalid_argument("Native blur commands require hardware GPU execution; software validation is detached only");
 if(!std::isfinite(options.strength)||options.strength<0||options.strength>1)throw std::invalid_argument("Invalid GPU layer blur strength");
 if((expected.generation&&expected.generation!=state_.generation)||(expected.revision&&expected.revision!=state_.revision)||(expected.object&&expected.object!=scene_.active().id))throw std::logic_error("GPU layer blur source changed; reopen its controls");
 finish_locked();require_authority_locked(expected);require_object_editable_locked(scene_.active().id);
 if((expected.generation&&expected.generation!=state_.generation)||(expected.revision&&expected.revision!=state_.revision)||(expected.object&&expected.object!=scene_.active().id))throw std::logic_error("GPU layer blur recipient changed while finishing its earlier gesture");
 auto lease=detail::capture_texture_set_recipient(scene_,scene_.active().id);
 auto changed=std::make_shared<bool>(false);auto report=std::make_shared<std::string>();
 const auto source=AssetSource{state_.generation,state_.revision,scene_.active().id,state_.edit_authority};
 queue_scene_operation_locked("Blur active color layer on GPU",[lease,options,changed,report](Scene&scene,const auto&cancelled){
   lease->authenticate_owner(lease);lease->authenticate_scene(scene);
   auto draft=lease->paint()->snapshot();auto result=draft->blur_color_layer_gpu(options,cancelled);
   *changed=result.changed;
   if(result.changed){TextureSetLimits limits;limits.cancelled=cancelled;detail::adopt_texture_set_stroke(scene,lease,*draft,limits);}
   *report=result.changed?"Active layer color and opacity updated":"No painted color changed";
 },source,report,{},changed,{},0,0,{},{},0,{},lease);
}
}
