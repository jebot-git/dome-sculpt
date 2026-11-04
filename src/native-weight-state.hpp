// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
#include <bit>
namespace dome::detail::native_weight_state {
template<class A,class B>bool owner(const std::shared_ptr<A>&a,const std::shared_ptr<B>&b)noexcept{return a.get()==b.get()&&!a.owner_before(b)&&!b.owner_before(a);}
inline bool scalar(float a,float b)noexcept{return std::bit_cast<uint32_t>(a)==std::bit_cast<uint32_t>(b);}
template<class V>bool vector(const V&a,const V&b)noexcept{return scalar(a.x,b.x)&&scalar(a.y,b.y)&&scalar(a.z,b.z);}
inline bool transform(CanvasTransform a,CanvasTransform b)noexcept{return vector(a.position,b.position)&&scalar(a.orientation.w,b.orientation.w)&&scalar(a.orientation.x,b.orientation.x)&&scalar(a.orientation.y,b.orientation.y)&&scalar(a.orientation.z,b.orientation.z)&&scalar(a.scale,b.scale);}
inline bool material(const Material&a,const Material&b)noexcept{
 if(a.name!=b.name||a.toon!=b.toon||a.double_sided!=b.double_sided||a.alpha!=b.alpha||a.unlit!=b.unlit)return false;
 for(size_t i=0;i<4;++i)if(!scalar(a.color[i],b.color[i]))return false;
 for(size_t i=0;i<3;++i)if(!scalar(a.emissive[i],b.emissive[i]))return false;
 return scalar(a.metallic,b.metallic)&&scalar(a.roughness,b.roughness)&&scalar(a.alpha_cutoff,b.alpha_cutoff);
}
inline bool world(const std::array<double,16>&a,const std::array<double,16>&b)noexcept{for(size_t i=0;i<16;++i)if(std::bit_cast<uint64_t>(a[i])!=std::bit_cast<uint64_t>(b[i]))return false;return true;}
}
