// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/skin-authoring.hpp"
#include "native-surface-heat.hpp"
namespace dome {
bool author_surface_heat_skin(Scene&scene,std::span<const uint64_t>targets,
 const SkinAuthoringOptions&options,const std::function<bool()>&cancelled){
 options.validate();if(options.mode!=SkinAuthoringMode::SurfaceHeat)
  throw std::invalid_argument("Choose automatic surface heat binding");
 if(cancelled&&cancelled())throw std::runtime_error("Surface heat binding cancelled");
 detail::NativeSurfaceHeatOptions bounded;
 bounded.maximum_work=options.anatomical.maximum_work;
 bounded.maximum_workspace=options.anatomical.maximum_workspace;
 bounded.maximum_total=options.anatomical.maximum_total;
 bounded.limits=options.limits;bounded.seed_distance=options.seed_distance;bounded.locks=options.locks;
 auto request=detail::capture_native_surface_heat(scene,targets,options.reference,bounded);
 if(cancelled&&cancelled())throw std::runtime_error("Surface heat binding cancelled after owning capture");
 auto prepared=detail::prepare_native_surface_heat(request,cancelled);
 if(cancelled&&cancelled())throw std::runtime_error("Surface heat binding cancelled before publication");
 return detail::adopt_native_surface_heat(scene,request,std::move(prepared));
}
}
