// SPDX-License-Identifier: Apache-2.0
#pragma once
#include <string>
#include <vector>
namespace ob {
struct ControllerProfile {
  std::string path, extension;
  bool touch = false;
};
struct ControllerBinding {
  std::string action, path;
};
std::vector<ControllerProfile>
controllerProfiles(const std::vector<std::string> &extensions);
std::vector<ControllerBinding>
controllerBindings(const ControllerProfile &profile);
} // namespace ob
