// SPDX-License-Identifier: GPL-3.0-or-later
#include "dome/humanoid-creator.hpp"
#include <nlohmann/json.hpp>
#include <set>
namespace dome {
namespace {
[[noreturn]] void fail(const char *s) { throw std::invalid_argument(s); }
}
void HumanoidControls::validate() const {
  if (baseline != "vroid-beta-female" && baseline != "vroid-beta-male" &&
      baseline != "makehuman-hm08")
    fail("Unknown humanoid baseline identity");
  if (!std::isfinite(height) || height < 1.2f || height > 2.2f)
    fail("Humanoid height must be within 1.2..2.2 metres");
  for (float value : {anime, eye_size, hair_length, hair_volume, fringe})
    if (!std::isfinite(value) || value < 0 || value > 1)
      fail("Humanoid style controls must be within [0,1]");
  for (float value :
       {build, shoulder_width, waist_width, leg_length, arm_length, head_width,
        head_height, eye_spacing, nose_size, mouth_width, jaw_width})
    if (!std::isfinite(value) || value < -1 || value > 1)
      fail("Humanoid proportion controls must be within [-1,1]");
  for (auto color : {skin_color, iris_color, hair_color})
    for (float v : color)
      if (!std::isfinite(v) || v < 0 || v > 1)
        fail("Invalid humanoid material color");
}
nlohmann::json HumanoidControls::encode() const {
  validate();
  return {{"version", 2},
          {"baseline", baseline},
          {"height", height},
          {"anime", anime},
          {"build", build},
          {"shoulder_width", shoulder_width},
          {"waist_width", waist_width},
          {"leg_length", leg_length},
          {"arm_length", arm_length},
          {"head_width", head_width},
          {"head_height", head_height},
          {"eye_size", eye_size},
          {"eye_spacing", eye_spacing},
          {"nose_size", nose_size},
          {"mouth_width", mouth_width},
          {"jaw_width", jaw_width},
          {"hair_length", hair_length},
          {"hair_volume", hair_volume},
          {"fringe", fringe},
          {"skin_color", skin_color},
          {"iris_color", iris_color},
          {"hair_color", hair_color}};
}
HumanoidControls HumanoidControls::decode(const nlohmann::json &j) {
  if (!j.is_object() || ((j.at("version") != 1 || j.size() != 21) &&
                         (j.at("version") != 2 || j.size() != 22)))
    fail("Unsupported humanoid controls record");
  HumanoidControls c;
  c.baseline = j.at("version") == 1 ? "makehuman-hm08"
                                    : j.at("baseline").get<std::string>();
#define READ(n) c.n = j.at(#n).get<decltype(c.n)>()
  READ(height);
  READ(anime);
  READ(build);
  READ(shoulder_width);
  READ(waist_width);
  READ(leg_length);
  READ(arm_length);
  READ(head_width);
  READ(head_height);
  READ(eye_size);
  READ(eye_spacing);
  READ(nose_size);
  READ(mouth_width);
  READ(jaw_width);
  READ(hair_length);
  READ(hair_volume);
  READ(fringe);
  READ(skin_color);
  READ(iris_color);
  READ(hair_color);
#undef READ
  c.validate();
  return c;
}
HumanoidControls humanoid_baseline_defaults(std::string_view baseline) {
  HumanoidControls c;
  c.baseline = baseline;
  if (baseline == "makehuman-hm08") {
    c.height = 1.7f;
    c.anime = .65f;
    c.eye_size = .65f;
    c.nose_size = -.3f;
    c.mouth_width = -.15f;
    c.jaw_width = -.2f;
    c.hair_length = .3f;
    c.hair_volume = .4f;
    c.fringe = .35f;
    c.skin_color = {.78f, .56f, .43f, 1};
    c.iris_color = {.12f, .32f, .65f, 1};
    c.hair_color = {.09f, .035f, .025f, 1};
  } else if (baseline == "vroid-beta-male")
    c.height = 1.7566531f;
  c.validate();
  return c;
}
HumanoidControls humanoid_preset(HumanoidPreset preset,
                                 std::string_view baseline) {
  HumanoidControls c = humanoid_baseline_defaults(baseline);
  if (baseline != "makehuman-hm08") {
    switch (preset) {
    case HumanoidPreset::Neutral:
      break;
    case HumanoidPreset::Toon:
      c.anime = .15f;
      c.head_width = .12f;
      c.eye_size = .12f;
      c.nose_size = -.08f;
      c.jaw_width = -.08f;
      break;
    case HumanoidPreset::Anime:
      c.anime = .25f;
      c.head_width = .15f;
      c.head_height = -.08f;
      c.eye_size = .2f;
      c.nose_size = -.12f;
      c.mouth_width = -.08f;
      c.jaw_width = -.12f;
      break;
    default:
      fail("Unknown humanoid style preset");
    }
    c.validate();
    return c;
  }
  switch (preset) {
  case HumanoidPreset::Neutral:
    c.anime = 0;
    c.eye_size = 0;
    c.nose_size = 0;
    c.mouth_width = 0;
    c.jaw_width = 0;
    c.hair_length = .12f;
    c.fringe = .15f;
    break;
  case HumanoidPreset::Toon:
    c.height = 1.65f;
    c.anime = .7f;
    c.build = .15f;
    c.shoulder_width = .1f;
    c.waist_width = .1f;
    c.leg_length = -.25f;
    c.arm_length = -.15f;
    c.head_width = .4f;
    c.head_height = -.25f;
    c.eye_size = .8f;
    c.nose_size = -.45f;
    c.jaw_width = -.3f;
    c.mouth_width = .1f;
    c.hair_length = .15f;
    c.hair_volume = .6f;
    c.fringe = .5f;
    break;
  case HumanoidPreset::Anime:
    c.height = 1.6f;
    c.anime = 1;
    c.build = -.45f;
    c.shoulder_width = -.4f;
    c.waist_width = -.15f;
    c.leg_length = .25f;
    c.arm_length = -.15f;
    c.head_width = .35f;
    c.head_height = -.35f;
    c.eye_size = 1;
    c.eye_spacing = .1f;
    c.nose_size = -.8f;
    c.mouth_width = -.45f;
    c.jaw_width = -.6f;
    c.hair_length = .45f;
    c.hair_volume = .7f;
    c.fringe = .7f;
    break;
  default:
    fail("Unknown humanoid style preset");
  }
  c.validate();
  return c;
}
void CreatedHumanoid::validate() const {
  controls.validate();
  if (parts.empty() || parts.size() > 64 || primary_part >= parts.size() ||
      (controls.baseline == "makehuman-hm08" && parts.size() != 5))
    fail("Incomplete humanoid parts");
  size_t vertices = 0;
  std::set<std::string> names;
  std::optional<nlohmann::json> reference_rig;
  for (const auto &p : parts) {
    if (p.name.empty() || p.name.size() > 256 || !names.insert(p.name).second)
      fail("Invalid humanoid part identity");
    p.mesh.validate();
    p.deformation.validate(p.mesh);
    if (!p.deformation.skeleton || !p.deformation.skin ||
        !p.deformation.humanoid)
      fail("Unrigged humanoid part");
    p.deformation.humanoid->validate(*p.deformation.skeleton);
    auto rig = p.deformation.skeleton->encode();
    if (reference_rig && rig != *reference_rig)
      fail("Humanoid part skeletons differ");
    reference_rig = std::move(rig);
    if (p.mesh.uv.size() != p.mesh.positions.size())
      fail("Humanoid part requires complete stable UVs");
    if (!std::isfinite(p.roughness) || p.roughness < 0 || p.roughness > 1)
      fail("Invalid humanoid material roughness");
    for (float channel : p.color)
      if (!std::isfinite(channel) || channel < 0 || channel > 1)
        fail("Invalid humanoid part color");
    vertices += p.mesh.positions.size();
    for (auto uv : p.mesh.uv)
      if (uv[0] < 0 || uv[0] > 1 || uv[1] < 0 || uv[1] > 1)
        fail("Humanoid UV outside atlas bounds");
  }
  if (vertices > (controls.baseline == "makehuman-hm08" ? 30'000u : 2'000'000u))
    fail("Humanoid generation budget exceeded");
}
} // namespace dome
