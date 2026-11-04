// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/skin-authoring.hpp"
#include "native-anatomical-weights.hpp"
namespace dome {
bool author_anatomical_skin(Scene&scene,std::span<const uint64_t>targets,
 const SkinAuthoringOptions&options,const std::function<bool()>&cancelled){
 options.validate();if(options.mode!=SkinAuthoringMode::Anatomical)
  throw std::invalid_argument("Choose the explicit anatomical source method");
 if(cancelled&&cancelled())throw std::runtime_error("Anatomical binding cancelled");
 detail::NativeAnatomicalOptions request_options;
 request_options.maximum_work=options.anatomical.maximum_work;
 request_options.maximum_workspace=options.anatomical.maximum_workspace;
 request_options.maximum_total=options.anatomical.maximum_total;
 request_options.locks=options.locks;
 auto request=detail::capture_native_anatomical(scene,targets,options.reference,request_options);
 if(cancelled&&cancelled())throw std::runtime_error("Anatomical binding cancelled after owning capture");
 auto prepared=detail::prepare_native_anatomical(request,cancelled);
 if(cancelled&&cancelled())throw std::runtime_error("Anatomical binding cancelled before publication");
 return detail::adopt_native_anatomical(scene,request,std::move(prepared));
}
} // namespace dome
