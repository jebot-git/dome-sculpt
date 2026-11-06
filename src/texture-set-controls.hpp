// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/texture-sets.hpp"
#include "dome/vr-controls.hpp"
#include <bit>
namespace dome::detail {
inline bool same_texture_material(const Material&a,const Material&b)noexcept{
 const auto bit=[](float x,float y){return std::bit_cast<uint32_t>(x)==std::bit_cast<uint32_t>(y);};
 if(a.name!=b.name||a.toon!=b.toon||a.double_sided!=b.double_sided||a.alpha!=b.alpha||a.unlit!=b.unlit||!bit(a.metallic,b.metallic)||!bit(a.roughness,b.roughness)||!bit(a.alpha_cutoff,b.alpha_cutoff))return false;
 for(size_t k=0;k<4;++k)if(!bit(a.color[k],b.color[k]))return false;for(size_t k=0;k<3;++k)if(!bit(a.emissive[k],b.emissive[k]))return false;return true;
}
inline bool texture_set_control(VrParameter p)noexcept{
 return p==VrParameter::TextureResolution||p==VrParameter::Metallic||p==VrParameter::Roughness||p==VrParameter::Opacity||p==VrParameter::BaseColor||p==VrParameter::MaterialEmission||p==VrParameter::LayerOpacity||p==VrParameter::LayerOrder;
}
// Values are staged in the existing common controls; the immutable recipient is
// unchanged until the common accepted worker transaction validates and applies.
// No raster replay or geometry copy occurs on a slider/second-eye publication.
inline TextureSetEdit texture_set_control_request(const SculptObject&o,const VrControlState&c){
 if(!o.texture_sets||!texture_set_control(c.parameter))throw std::invalid_argument("No selected texture field for these controls");
 const auto field=active_texture_set(o);TextureSetEdit edit;edit.set=field.id;edit.material=field.material;edit.action=TextureSetAction::Material;
 switch(c.parameter){
 case VrParameter::TextureResolution:edit.action=TextureSetAction::Resolution;edit.resolution=64u<<std::clamp(int(std::lround(c.value[0])),0,5);break;
 case VrParameter::Metallic:edit.material.metallic=c.value[0];break;
 case VrParameter::Roughness:edit.material.roughness=c.value[0];break;
 case VrParameter::Opacity:edit.material.color[3]=c.value[0];break;
 case VrParameter::BaseColor:edit.material.color=c.value;break;
 case VrParameter::MaterialEmission:for(size_t k=0;k<3;++k)edit.material.emissive[k]=c.value[k];break;
 case VrParameter::LayerOpacity:case VrParameter::LayerOrder:edit.action=c.parameter==VrParameter::LayerOpacity?TextureSetAction::LayerOpacity:TextureSetAction::LayerOrder;edit.layer=field.painted->active_layer();edit.value=c.value[0];break;
 default:throw std::invalid_argument("Unknown texture-set control");
 }return edit;
}
} // namespace dome::detail
