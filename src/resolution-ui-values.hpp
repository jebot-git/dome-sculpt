// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/sculpt-resolution.hpp"
namespace dome {
struct ResolutionUiValues {
  SculptResolutionOptions options;
  double factor=1000,current_mm=30,target_mm=30,minimum_mm=.01,maximum_mm=1000;
  double surface_mm=15,layer_mm=30,normal_degrees=60,mask_percent=10;
  bool automatic=true;
  ResolutionUiValues(float current_voxel,float world_scale,SculptResolutionOptions initial)
      :options(std::move(initial)),factor(double(world_scale)*1000) {
    if(!std::isfinite(world_scale) || world_scale<=0 || !std::isfinite(current_voxel) || current_voxel<.00001f || current_voxel>1 || !std::isfinite(options.voxel_size) || options.voxel_size<.00001f || options.voxel_size>1)
      throw std::invalid_argument("Resolution is unavailable at this object scale");
    current_mm=double(current_voxel)*factor;target_mm=double(options.voxel_size)*factor;
    minimum_mm=double(.00001f)*factor;maximum_mm=factor;
    automatic=options.maximum_surface_error==0 && options.maximum_layer_error==0;
    surface_mm=(options.maximum_surface_error>0?double(options.maximum_surface_error):std::max(double(current_voxel),double(options.voxel_size))*.5)*factor;
    layer_mm=(options.maximum_layer_error>0?double(options.maximum_layer_error):std::max(double(current_voxel),double(options.voxel_size)))*factor;
    normal_degrees=std::acos(std::clamp(double(options.minimum_normal_dot),0.,1.))*180/3.14159265358979323846;
    mask_percent=double(options.maximum_mask_error)*100;
  }
  void automatic_bounds() {
    if(automatic){surface_mm=std::max(current_mm,target_mm)*.5;layer_mm=std::max(current_mm,target_mm);}
  }
  double normalized() const {return std::clamp(std::log(target_mm/minimum_mm)/std::log(maximum_mm/minimum_mm),0.,1.);}
  void slider(double t){target_mm=std::clamp(std::exp(std::log(minimum_mm)+std::clamp(t,0.,1.)*std::log(maximum_mm/minimum_mm)),minimum_mm,maximum_mm);automatic_bounds();}
  SculptResolutionOptions accepted() const {
    if(!std::isfinite(target_mm) || target_mm<minimum_mm || target_mm>maximum_mm ||
        !std::isfinite(surface_mm) || surface_mm<0 || (!automatic && surface_mm==0) || surface_mm>factor ||
        !std::isfinite(layer_mm) || layer_mm<0 || (!automatic && layer_mm==0) || layer_mm>factor*1e6 ||
        !std::isfinite(normal_degrees) || normal_degrees<0 || normal_degrees>90 ||
        !std::isfinite(mask_percent) || mask_percent<0 || mask_percent>100)
      throw std::invalid_argument("Enter finite resolution and quality values within their displayed ranges");
    auto result=options;
    // Unchanged displayed world units must not round a local voxel spacing.
    if(target_mm!=double(options.voxel_size)*factor)result.voxel_size=std::clamp(float(target_mm/factor),.00001f,1.f);
    result.maximum_surface_error=automatic?0:float(surface_mm/factor);
    result.maximum_layer_error=automatic?0:float(layer_mm/factor);
    result.minimum_normal_dot=normal_degrees>=90?0:normal_degrees<=0?1:float(std::cos(normal_degrees*3.14159265358979323846/180));
    result.maximum_mask_error=float(mask_percent/100);result.cancelled={};result.progress={};result.validate();return result;
  }
};
}
