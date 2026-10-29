// SPDX-License-Identifier: Apache-2.0
#include "controller-profiles.h"
#include <algorithm>
namespace ob {
std::vector<ControllerProfile>
controllerProfiles(const std::vector<std::string> &extensions) {
  std::vector<ControllerProfile> result{
      {"/interaction_profiles/oculus/touch_controller", "", true}};
  for (auto profile :
       {ControllerProfile{"/interaction_profiles/facebook/touch_controller_pro",
                          "XR_FB_touch_controller_pro", true},
        ControllerProfile{"/interaction_profiles/meta/touch_controller_plus",
                          "XR_META_touch_controller_plus", true}})
    if (std::find(extensions.begin(), extensions.end(), profile.extension) !=
        extensions.end())
      result.push_back(profile);
  for (auto path : {"/interaction_profiles/valve/index_controller",
                    "/interaction_profiles/htc/vive_controller",
                    "/interaction_profiles/khr/simple_controller"})
    result.push_back({path, "", false});
  return result;
}
std::vector<ControllerBinding> controllerBindings(const ControllerProfile &p) {
  std::vector<ControllerBinding> b;
  bool simple = p.path.find("simple") != std::string::npos;
  for (auto hand : {"left", "right"}) {
    std::string base = "/user/hand/" + std::string(hand) + "/";
    b.push_back({"grip", base + "input/grip/pose"});
    b.push_back({"aim", base + "input/aim/pose"});
    b.push_back({"trigger", base + (simple ? "input/select/click"
                                           : "input/trigger/value")});
    b.push_back({"haptic", base + "output/haptic"});
    if (!simple)
      b.push_back({"squeeze", base + (p.path.find("vive") != std::string::npos
                                          ? "input/squeeze/click"
                                          : "input/squeeze/value")});
  }
  if (p.touch) {
    for (auto item : std::vector<ControllerBinding>{
             {"menu", "/user/hand/left/input/x/click"},
             {"eraser", "/user/hand/left/input/y/click"},
             {"undo", "/user/hand/right/input/a/click"},
             {"redo", "/user/hand/right/input/b/click"},
             {"left_stick", "/user/hand/left/input/thumbstick"},
             {"right_stick", "/user/hand/right/input/thumbstick"},
             {"mirror", "/user/hand/left/input/thumbstick/click"},
             {"recenter", "/user/hand/right/input/thumbstick/click"}})
      b.push_back(item);
  } else
    b.push_back({"menu", std::string("/user/hand/left/input/") +
                             (p.path.find("index") != std::string::npos
                                  ? "b/click"
                                  : "menu/click")});
  return b;
}
} // namespace ob
