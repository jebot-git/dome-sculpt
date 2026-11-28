// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/skin-authoring.hpp"
#include "native-surface-heat.hpp"
namespace dome {
namespace {
bool author_bounded_skin(Scene&scene,std::span<const uint64_t>targets,
 const SkinAuthoringOptions&options,SkinAuthoringMode expected,const std::function<bool()>&cancelled){
 options.validate();
 if(options.mode!=expected)throw std::invalid_argument("The requested skin binding method does not match its options");
 if(cancelled&&cancelled())throw std::runtime_error("Skin binding cancelled");
 detail::NativeSurfaceHeatOptions bounded;
 bounded.maximum_work=options.anatomical.maximum_work;
 bounded.maximum_workspace=options.anatomical.maximum_workspace;
 bounded.maximum_total=options.anatomical.maximum_total;
 bounded.limits=options.limits;bounded.seed_distance=options.seed_distance;bounded.locks=options.locks;
 if(expected==SkinAuthoringMode::VolumeGeodesic){
   auto &volume=bounded.volume.emplace();
   volume.cell_size=options.volume_cell_size;
   volume.binding.maximum_seed_distance=options.seed_distance;
   volume.binding.seed_margin=options.seed_margin;
   volume.binding.falloff=options.falloff;
 }
 auto request=detail::capture_native_surface_heat(scene,targets,options.reference,bounded);
 if(cancelled&&cancelled())throw std::runtime_error("Skin binding cancelled after owning capture");
 auto prepared=detail::prepare_native_surface_heat(request,cancelled);
 if(cancelled&&cancelled())throw std::runtime_error("Skin binding cancelled before publication");
 return detail::adopt_native_surface_heat(scene,request,std::move(prepared),cancelled);
}
}
bool author_surface_heat_skin(Scene&scene,std::span<const uint64_t>targets,
 const SkinAuthoringOptions&options,const std::function<bool()>&cancelled){
 return author_bounded_skin(scene,targets,options,SkinAuthoringMode::SurfaceHeat,cancelled);
}
bool author_volume_skin(Scene&scene,std::span<const uint64_t>targets,
 const SkinAuthoringOptions&options,const std::function<bool()>&cancelled){
 return author_bounded_skin(scene,targets,options,SkinAuthoringMode::VolumeGeodesic,cancelled);
}
}
