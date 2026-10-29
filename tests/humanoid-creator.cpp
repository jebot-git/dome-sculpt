// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/humanoid-creator.hpp"
#include "dome/scene.hpp"
#include <cstdlib>
#include <iostream>
#include <nlohmann/json.hpp>
#include <set>
using namespace dome;
void require(bool v, const char *s) {
  if (!v)
    throw std::runtime_error(s);
  std::cout << "PASS " << s << '\n';
}
void rejects(auto f, const char *s) {
  bool bad = false;
  try {
    f();
  } catch (const std::exception &) {
    bad = true;
  }
  require(bad, s);
}
void export_fixture(const CreatedHumanoid &avatar,
                    const std::filesystem::path &path) {
  std::vector<ObjectSurface> objects;
  for (size_t i = 0; i < avatar.parts.size(); ++i) {
    const auto &p = avatar.parts[i];
    ObjectSurface surface;
    surface.id = i + 1;
    surface.name = p.name;
    surface.material.color = p.color;
    surface.material.toon = p.toon;
    surface.material.roughness = p.roughness;
    surface.mesh = p.mesh;
    surface.deformation =
        std::make_shared<const DeformationState>(p.deformation);
    objects.push_back(std::move(surface));
  }
  export_glb(objects, path);
}
int main() try {
  auto avatar = create_humanoid();
  require(avatar.parts.size() == 5, "Generator produces coherent body, sclera, "
                                    "iris, pupil and hair surfaces");
  for (const auto &part : avatar.parts) {
    PaintedMesh paintable(part.mesh, 256);
    require(paintable.mesh().positions.size() == part.mesh.positions.size(),
            "Every creator part retains authored topology when made paintable");
  }
  auto &body = avatar.parts[0];
  require(body.mesh.positions.size() == 14517 &&
              body.mesh.indices.size() == 80268,
          "Authored MakeHuman body retains real detailed face/finger topology "
          "and UV splits");
  require(body.deformation.skeleton->bones.size() == 163 &&
              body.deformation.humanoid->joints.size() == 55,
          "Complete semantic humanoid maps the authored 163-joint finger/face "
          "skeleton");
  float floor = 1e6f, top = -1e6f;
  for (auto p : body.mesh.positions) {
    floor = std::min(floor, p.y);
    top = std::max(top, p.y);
  }
  require(std::abs(floor) < 1e-6f &&
              std::abs(top - avatar.controls.height) < 1e-5f,
          "Creator converts decimetre centred source into grounded metric "
          "+Y-up coordinates");
  require(
      body.deformation.morphs->targets.size() == 8 &&
          body.deformation.expressions->visemes.size() == 5,
      "Body includes native editable vowel, smile and eyelid morph identities");
  for (size_t t = 0; t < body.deformation.morphs->targets.size(); ++t) {
    auto state = body.deformation;
    state.morph_weights.assign(8, 0);
    state.morph_weights[t] = 1;
    auto posed = state.preview(body.mesh);
    posed.validate();
    require(posed.positions.size() == body.mesh.positions.size(),
            "Each full-strength expression preserves stable nondegenerate "
            "topology");
  }
  auto roundtrip = HumanoidControls::decode(avatar.controls.encode());
  require(roundtrip.encode() == avatar.controls.encode(),
          "Creator parameters roundtrip through explicitly versioned bounded "
          "metadata");
  auto changed = avatar.controls;
  changed.anime = 1;
  changed.head_width = .8f;
  changed.eye_size = 1;
  changed.shoulder_width = -.8f;
  changed.leg_length = .7f;
  changed.arm_length = -.7f;
  changed.hair_length = .9f;
  auto toon = create_humanoid(changed);
  toon.validate();
  for (size_t i = 0; i < avatar.parts.size(); ++i)
    require(toon.parts[i].mesh.positions.size() ==
                    avatar.parts[i].mesh.positions.size() &&
                toon.parts[i].mesh.indices == avatar.parts[i].mesh.indices &&
                toon.parts[i].mesh.uv == avatar.parts[i].mesh.uv,
            "Proportion/style edits preserve every part's vertex order, faces "
            "and UV coordinates");
  require(toon.parts[0].deformation.source != body.deformation.source,
          "A changed parametric rest shape has a distinct deformation source "
          "identity");
  // Disconnected eyes and groom locks are deliberately authored and weighted,
  // not assigned through an unbounded nearest-bone fallback.
  for (size_t i = 1; i < avatar.parts.size(); ++i) {
    auto state = avatar.parts[i].deformation;
    state.pose.clear();
    for (auto &b : state.skeleton->bones)
      state.pose.push_back(b.rest);
    state.pose[state.humanoid->joints.at("head")].rotation =
        (state.pose[state.humanoid->joints.at("head")].rotation *
         Quat{.9950042f, 0, .0998334f, 0})
            .unit();
    auto preview = state.preview(avatar.parts[i].mesh);
    preview.validate();
    require(
        length(preview.positions[0] - avatar.parts[i].mesh.positions[0]) >
            1e-5f,
        "Disconnected eye/hair parts remain fully weighted under head posing");
  }
  HumanoidControls minimal;
  minimal.anime = 0;
  minimal.eye_size = 0;
  minimal.hair_length = 0;
  minimal.hair_volume = 0;
  minimal.fringe = 0;
  auto neutral = create_humanoid(minimal);
  neutral.validate();
  require(neutral.parts[4].mesh.indices == avatar.parts[4].mesh.indices,
          "Zero-length groom controls retain valid fixed topology");
  for (int variation = 0; variation < 8; ++variation) {
    HumanoidControls extreme;
    float v = variation & 1 ? 1.f : -1.f;
    extreme.height = variation & 2 ? 1.2f : 2.2f;
    extreme.anime = variation & 4 ? 1 : 0;
    extreme.build = v;
    extreme.shoulder_width = -v;
    extreme.waist_width = v;
    extreme.leg_length = -v;
    extreme.arm_length = v;
    extreme.head_width = v;
    extreme.head_height = -v;
    extreme.eye_size = 1;
    extreme.eye_spacing = -v;
    extreme.nose_size = v;
    extreme.mouth_width = -v;
    extreme.jaw_width = v;
    extreme.hair_length = variation & 2 ? 0 : 1;
    extreme.hair_volume = variation & 4 ? 0 : 1;
    extreme.fringe = variation & 1 ? 0 : 1;
    create_humanoid(extreme).validate();
  }
  require(true, "Extreme bounded height/body/face/groom combinations retain "
                "valid coherent topology");
  auto invalid = changed;
  invalid.height = 100;
  rejects([&] { create_humanoid(invalid); },
          "Unbounded geometry controls are rejected");
  invalid = changed;
  invalid.eye_size = 2;
  rejects([&] { create_humanoid(invalid); },
          "Extreme face sliders cannot bypass bounded validation");
  rejects([&] { create_humanoid({}, [] { return true; }); },
          "Creation supports worker cancellation before producing geometry");
  auto json = avatar.controls.encode();
  json["version"] = 2;
  rejects([&] { HumanoidControls::decode(json); },
          "Future creator records are not silently misinterpreted");
  for (auto preset :
       {HumanoidPreset::Neutral, HumanoidPreset::Toon, HumanoidPreset::Anime}) {
    auto styled = create_humanoid(humanoid_preset(preset));
    styled.validate();
    for (const auto &part : styled.parts)
      PaintedMesh paintable(part.mesh, 256);
    require(styled.parts[1].roughness < styled.parts[0].roughness &&
                styled.parts[2].roughness < styled.parts[4].roughness,
            "Eye gloss and body/hair material roughness are deliberate "
            "per-part values");
    if (const char *path = std::getenv("DOME_HUMANOID_FIXTURE")) {
      auto f = std::filesystem::path(path);
      std::string suffix = preset == HumanoidPreset::Neutral ? "-neutral"
                           : preset == HumanoidPreset::Toon  ? "-toon"
                                                             : "-anime";
      export_fixture(styled,
                     f.parent_path() / (f.stem().string() + suffix + ".glb"));
    }
  }
  rejects([&] { humanoid_preset(static_cast<HumanoidPreset>(999)); },
          "Unknown style presets are rejected");
  if (const char *path = std::getenv("DOME_HUMANOID_FIXTURE"))
    export_fixture(avatar, path);

  return 0;
} catch (const std::exception &e) {
  std::cerr << "FAIL " << e.what() << '\n';
  return 1;
}
