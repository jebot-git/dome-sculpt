// SPDX-License-Identifier: GPL-3.0-or-later
#pragma once
#include "scene.hpp"
namespace dome {
enum class SceneFileKind { Project, AuthoringPackage };
struct LoadedSceneFile {
  Scene scene;
  SceneFileKind kind = SceneFileKind::Project;
};
// Worker-only bounded project loading. Authenticated interchange containers
// restore their exact native Scene, rather than becoming another raw source.
// A package is an unsaved project: its input path must not become the target
// of ordinary native CBOR Save.
LoadedSceneFile load_scene_file(const std::filesystem::path &,
                               const std::function<bool()> &cancelled = {});
} // namespace dome
