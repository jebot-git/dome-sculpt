// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/scene.hpp"
namespace dome::detail {
inline void shape_sculpt_preview(Scene &scene,uint64_t object,size_t target){
 auto &o=scene.object(object);
 if(!o.deformation||!o.deformation->morphs||target>=o.deformation->morphs->targets.size())throw std::invalid_argument("Shapekey preview target changed");
 auto d=std::make_shared<DeformationState>(*o.deformation);
 d->morph_weights.assign(d->morphs->targets.size(),0);d->morph_weights[target]=1;
 o.deformation=std::move(d);
}
}
