// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "dome/math.hpp"
#include <array>
#include <cstdint>
#include <stdexcept>
namespace dome::desktop {
struct SpaceSettings {
  bool enabled = true;
  float translation = .8f, rotation = 90.f, deadzone = 15.f;
  std::array<bool,6> inverted{};
  bool swap_buttons = false;
  bool valid() const noexcept {
    return std::isfinite(translation)&&translation>=.05f&&translation<=5 &&
           std::isfinite(rotation)&&rotation>=5&&rotation<=360 &&
           std::isfinite(deadzone)&&deadzone>=0&&deadzone<=120;
  }
  void validate() const {
    if(!valid())throw std::invalid_argument("Invalid 3D mouse navigation settings");
  }
};
struct SpaceView {
  Quat rotation{};
  Vec3 pan{}, pivot{};
  float distance=2.2f,limit=20.f;
};
inline std::array<float,6> space_axes(const std::array<int32_t,6>&raw,
                                       const SpaceSettings&settings) {
  std::array<float,6> axes{};
  if(!settings.valid())return axes;
  for(size_t i=0;i<6;++i) {
    // Convert before abs: INT32_MIN is a valid wire integer, not an overflow.
    double value=raw[i],magnitude=std::abs(value);
    if(magnitude>settings.deadzone)
      axes[i]=float(std::copysign(std::min(1.,(magnitude-settings.deadzone)/
                          (350-settings.deadzone)),value))*(settings.inverted[i]?-1.f:1.f);
  }
  for(size_t start:{size_t(0),size_t(3)}) {
    float magnitude=std::sqrt(axes[start]*axes[start]+axes[start+1]*axes[start+1]+axes[start+2]*axes[start+2]);
    if(magnitude>1)for(size_t i=start;i<start+3;++i)axes[i]/=magnitude;
  }
  return axes;
}
// Integrate a held, mapped force for elapsed monotonic seconds. This updates
// only the desktop view. Combined pan/dolly is an exact normalized-screen ODE;
// rotation is the exponential of the full 3-axis angular velocity.
inline bool advance_space_view(SpaceView &view,const std::array<float,6>&axes,
                                const SpaceSettings &settings,double seconds) {
  if(!settings.valid()||!settings.enabled||!std::isfinite(seconds)||seconds<0||seconds>.15 ||
      !finite(view.pan)||!finite(view.pivot)||!std::isfinite(view.distance)||
      !std::isfinite(view.limit)||view.distance<.01f||view.limit<view.distance)
    return false;
  double norm=double(view.rotation.w)*view.rotation.w+double(view.rotation.x)*view.rotation.x+
              double(view.rotation.y)*view.rotation.y+double(view.rotation.z)*view.rotation.z;
  if(!std::isfinite(norm)||std::abs(norm-1)>.001)return false;
  for(auto value:axes)if(!std::isfinite(value)||std::abs(value)>1.00001f)return false;
  bool moving=std::any_of(axes.begin(),axes.end(),[](float x){return x!=0;});
  if(!moving||seconds==0)return false;
  auto candidate=view;
  const auto before=view.rotation.rotate(view.pivot)+view.pan;
  const double depth=double(view.distance)-before.z;
  if(depth<=.00001 || !std::isfinite(depth))return false;
  double target_depth=depth*std::exp(double(axes[2])*settings.translation*seconds);
  candidate.distance=float(std::clamp(double(before.z)+target_depth,
                            std::max(.01,double(before.z)+.00001),double(view.limit)));
  target_depth=double(candidate.distance)-before.z;
  if(!std::isfinite(target_depth)||target_depth<=.00001)return false;
  const double relative=target_depth/depth;
  Vec3 center{float(double(before.x)*relative+double(axes[0])*settings.translation*seconds*target_depth),
              float(double(before.y)*relative+double(axes[1])*settings.translation*seconds*target_depth),before.z};
  Vec3 rate{axes[3],axes[4],axes[5]};
  double magnitude=std::sqrt(double(rate.x)*rate.x+double(rate.y)*rate.y+double(rate.z)*rate.z);
  if(magnitude>0) {
    double half=magnitude*settings.rotation*(3.14159265358979323846/180)*seconds*.5;
    double scale=std::sin(half)/magnitude;
    Quat delta{float(std::cos(half)),float(rate.x*scale),float(rate.y*scale),float(rate.z*scale)};
    candidate.rotation=(delta*view.rotation).unit();
  }
  candidate.pan=center-candidate.rotation.rotate(candidate.pivot);
  if(!finite(candidate.pan)||!std::isfinite(candidate.distance))return false;
  view=candidate;return true;
}
} // namespace dome::desktop
